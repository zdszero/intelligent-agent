#pragma once

#include <cstdio>
#define READ_BUFFER_SIZE 1024
#define WRITE_BUFFER_SIZE 1024

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
    UNVERIFIED = 0,
    CONNECTED,
    QUERY,
    END
};

class TransferConn {
   public:
    TransferConn() = default;
    ~TransferConn() = default;
    void Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, char* host_name, int epollfd);
    void Process();
    void CloseConn();

   private:
    // listening socket fd
    int sockfd_;
    RedisConn* redis_conn_;
    int epollfd_;
};

class ProxyConn {
   public:
    ProxyConn() = default;
    ~ProxyConn() = default;
    void Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, char* host_name, int epollfd);
    void Process();
    void CloseConn();

   private:
    // host name and address
    int epollfd_;
    char* host_;
    sockaddr_in address_;
    ConnStatus conn_status_;
    int sockfd_;
    string client_id_;
    RedisConn* redis_conn_;
    RedisConn* redis_conf_conn_;

    void init();
    std::string parseClient();
    std::string clientVarify();
    std::string getPrevProxy();
    int logTraverse(const string& remote_proxy, const std::string& client_id);
    int runProxyLoop();
    int setLocalProxy();
    int tcpDial(const std::string& host, int port);
};
