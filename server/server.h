#pragma once

#include <cstdio>
#define READ_BUFFER_SIZE 1024
#define WRITE_BUFFER_SIZE 1024

#define CONN_STATU_UNVERIFY 0
#define CONN_STATU_CONNECTED
#define CONN_STATU_QUERY
#define CONN_STATU_END

#define PROXY_TRANVERSE_PORT 30072
#define PROXY_AGENT_PROT 30071

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <thread>
#include <vector>

#include "iowrapper.h"
#include "redis_conn.h"
#include "sysmsg.h"

using namespace std;

enum class ConnStatus {
    UNVERIFT = 0,
    CONNECTED,
    QUERY,
    END
};

class TraverseConn {
   public:
    TraverseConn(char* host, RedisConn* redis_conn, IOWrapper *io) : redis_conn_(redis_conn), io_(io) {
#ifdef IPPROTO_MPTCP
        m_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);
#else
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
#endif
        sockaddr_in addr;
        bzero(&addr, sizeof(sockaddr_in));
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, host, &addr.sin_addr);
        addr.sin_port = PROXY_TRANVERSE_PORT;
        bind(sockfd_, (struct sockaddr*)&addr, sizeof(struct sockaddr));
        listen(sockfd_, 1);
    }
    ~TraverseConn() { close(sockfd_); }
    void Process() {
        struct sockaddr addr;
        socklen_t socklen;
        int fd = accept(sockfd_, &addr, &socklen);
        // get client id from peer
        SysMsg msg;
        if (!io_->ReadSysMsg(fd, msg)) {
            fprintf(stderr, "TraverseConn fail to receive client address from peer\n");
            exit(1);
        }
        std::string client_id(msg.buf);
        // get all the log entries from redis
        std::vector<std::string> log_entries;
        redis_conn_->getLog(client_id, log_entries);
        for (const string& entry : log_entries) {
            io_->SendSysMsg(fd, MsgWrapper::wrap(entry));
        }
        close(fd);
    }

   private:
    // listening socket fd
    int sockfd_;
    RedisConn* redis_conn_;
    IOWrapper *io_;
};

class ProxyConn {
   public:
    ProxyConn();
    ~ProxyConn();
    void Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, const char* host_name);
    void Process();

   private:
    // host name and address
    const char* host_;
    sockaddr_in address_;
    IOWrapper *io_;
    ConnStatus conn_status_;
    int sockfd_;
    string client_id_;
    RedisConn* redis_conn_;
    RedisConn* redis_conf_conn_;

    void init();
    void closeConn();
    std::string parseClient();
    std::string clientVarify();
    std::string getPrevProxy();
    int logTraverse(const string& remote_proxy, const std::string& client_id);
    int runProxyLoop();
    int setLocalProxy();
    int tcpDial(const std::string& host, int port);
};
