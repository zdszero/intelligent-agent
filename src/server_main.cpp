#include <signal.h>

#include <cassert>

#include "log.h"
#include "server.h"
#include "threadpool.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define MAX_BACKLOG 10
#define MAX_HOST_LEN 500

#define SERVER_PROXY 1
#define SERVER_TRANV 2

typedef uint8_t server_t;

void add_sig(int sig, void(handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char* info) {
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

std::string GetLocalIPv4() {
    std::string cmd = "ip addr | grep 'inet ' | awk '{print $2}' | grep -v -P '\\.1/\\d+'";
    FILE* fp = popen(cmd.c_str(), "r");
    if (fp == NULL) {
        return "";
    }
    std::string out = "";
    char buf[256];
    while (fgets(buf, 256, fp) != NULL) {
        out += std::string(buf);
    }
    pclose(fp);
    return out.substr(0, out.find('/'));
}

int main(int argc, char* argv[]) {
    const char* host = "0.0.0.0";
    int tranv_port = PROXY_TRANSFER_PORT;
    int proxy_port = PROXY_AGENT_PROT;

    if (argc == 3) {
        host = argv[1];
        tranv_port = atoi(argv[2]);
        proxy_port = atof(argv[3]);
    }

    RedisConn redis_local, redis_remote;
    if (!redis_local.Connect("localhost", 7777)) {
        fprintf(stderr, "error: fail to connect to local redis\n");
        exit(EXIT_FAILURE);
    }

    char* remote_redis = getenv("REMOTE_REDIS");
    std::string ip = "zds-704";
    int port = 7777;
    if (remote_redis) {
        std::string s(remote_redis);
        size_t pos = s.find(':');
        if (pos != std::string::npos) {
            ip = s.substr(0, pos);
            port = std::stoi(s.substr(pos+1));
        } else {
            ip = s;
        }
    }
    DPrintf("remote redis = %s:%d\n", ip.c_str(), port);
    if (!redis_remote.Connect(ip, port)) {
        fprintf(stderr, "error: fail to connect to remote redis\n");
        exit(EXIT_FAILURE);
    }

    // char host_name[MAX_HOST_LEN];
    // gethostname(host_name, MAX_HOST_LEN);

    string ipv4 = GetLocalIPv4();
    DPrintf("local ip = %s\n", ipv4.c_str());
    add_sig(SIGPIPE, SIG_IGN);

    threadpool<ProxyConn>* proxy_pool = NULL;
    threadpool<TransferConn>* tranv_pool = NULL;
    try {
        proxy_pool = new threadpool<ProxyConn>(2, 1000);
        tranv_pool = new threadpool<TransferConn>(2, 1000);
    } catch (std::exception& e) {
        return -1;
    }

    server_t conn_type[MAX_FD];
    bzero(conn_type, sizeof(server_t) * MAX_FD);
    ProxyConn* proxy_clients = new ProxyConn[MAX_FD];
    TransferConn* tranv_clients = new TransferConn[MAX_FD];

    int client_count = 0;
#ifdef IPPROTO_MPTCP
    int proxy_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_MPTCP);
    int tranv_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_MPTCP);
#else
    int proxy_fd = socket(PF_INET, SOCK_STREAM, 0);
    int tranv_fd = socket(PF_INET, SOCK_STREAM, 0);
#endif
    assert(proxy_fd >= 0);
    assert(tranv_fd >= 0);
    linger tmp = {1, 0};
    setsockopt(proxy_fd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    setsockopt(tranv_fd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    int ret = 0;
    sockaddr_in proxy_addr, tranv_addr;
    bzero(&proxy_addr, sizeof(proxy_addr));
    bzero(&tranv_addr, sizeof(tranv_addr));
    proxy_addr.sin_family = AF_INET;
    tranv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, host, &proxy_addr.sin_addr);
    inet_pton(AF_INET, host, &tranv_addr.sin_addr);
    proxy_addr.sin_port = htons(proxy_port);
    tranv_addr.sin_port = htons(tranv_port);
    ret = bind(proxy_fd, (sockaddr*)&proxy_addr, sizeof(proxy_addr));
    ret = listen(proxy_fd, MAX_BACKLOG);
    assert(ret >= 0);
    ret = bind(tranv_fd, (sockaddr*)&tranv_addr, sizeof(tranv_addr));
    ret = listen(tranv_fd, MAX_BACKLOG);
    assert(ret >= 0);
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    IO::AddFd(epollfd, proxy_fd, false);
    IO::AddFd(epollfd, tranv_fd, false);
    while (true) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll error\n");
            break;
        }
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == proxy_fd) {
                sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int connfd = accept(proxy_fd, (sockaddr*)&client_addr, &client_addrlen);
                if (connfd < 0) {
                    continue;
                }
                conn_type[connfd] = SERVER_PROXY;
                proxy_clients[connfd].Init(connfd, client_addr, &redis_local, &redis_remote, ipv4.c_str(), epollfd);
            }
            if (sockfd == tranv_fd) {
                sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int connfd = accept(tranv_fd, (sockaddr*)&client_addr, &client_addrlen);
                if (connfd < 0) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                conn_type[connfd] = SERVER_TRANV;
                DPrintf("get client\n");
                tranv_clients[connfd].Init(connfd, client_addr, &redis_local, &redis_remote, ipv4.c_str(), epollfd);
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                if (conn_type[sockfd] == SERVER_PROXY) {
                    proxy_clients[sockfd].CloseConn();
                } else {
                    tranv_clients[sockfd].CloseConn();
                }
            } else if (events[i].events & EPOLLIN) {
                if (conn_type[sockfd] == SERVER_PROXY) {
                    proxy_pool->append(&proxy_clients[sockfd]);
                }
                if (conn_type[sockfd] == SERVER_TRANV) {
                    tranv_pool->append(&tranv_clients[sockfd]);
                }
            }
        }
    }
}
