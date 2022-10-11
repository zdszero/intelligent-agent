#include <signal.h>

#include <cassert>

#include "server.h"
#include "threadpool.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define MAX_BACKLOG 10
#define MAX_HOST_LEN 500

#define SERVER_PROXY 1
#define SERVER_TRANV 2

typedef uint8_t server_t;

extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);

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

int main(int argc, char* argv[]) {
    const char* host = "localhost";
    int tranv_port = PROXY_TRANVERSE_PORT;
    int proxy_port = PROXY_AGENT_PROT;

    if (argc == 3) {
        host = argv[1];
        tranv_port = atoi(argv[2]);
        proxy_port = atof(argv[3]);
    }

    RedisConn redis_local, redis_remote;
    redis_local.connect("localhost", 5375);
    redis_remote.connect("localhost", 5375);

    char host_name[MAX_HOST_LEN];

    gethostname(host_name, MAX_HOST_LEN);
    add_sig(SIGPIPE, SIG_IGN);

    threadpool<ProxyConn>* proxy_pool = NULL;
    threadpool<TransferConn>* tranv_pool = NULL;
    try {
        proxy_pool = new threadpool<ProxyConn>(4, 1000);
        tranv_pool = new threadpool<TransferConn>(4, 1000);
    } catch (...) {
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
    addfd(epollfd, proxy_fd, false);
    addfd(epollfd, tranv_fd, false);
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
                proxy_clients[connfd].Init(connfd, client_addr, &redis_local, &redis_remote, host_name, epollfd);
            }
            if (sockfd == tranv_fd) {
                sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int connfd = accept(tranv_fd, (sockaddr*)&client_addr, &client_addrlen);
                if (connfd < 0) {
                    continue;
                }
                conn_type[connfd] = SERVER_TRANV;
                tranv_clients[connfd].Init(connfd, client_addr, &redis_local, &redis_remote, host_name, epollfd);
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                if (conn_type[sockfd] == SERVER_PROXY) {
                    proxy_clients[sockfd].CloseConn();
                } else {
                    tranv_clients[sockfd].CloseConn();
                }
            } else if (events[i].events | EPOLLIN) {
                if (conn_type[sockfd] == SERVER_PROXY) {
                    proxy_pool->append(&proxy_clients[sockfd]);
                } else {
                    tranv_pool->append(&tranv_clients[sockfd]);
                }
            }
        }
    }
}
