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
    UNVERIFT = 0,
    CONNECTED,
    QUERY,
    END
};

class TraverseConn {
   public:
    TraverseConn() {}
    ~TraverseConn() {}
    void Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, char* host_name);
    void Process();

   private:
    // listening socket fd
    int sockfd_;
    RedisConn* redis_conn_;
};

class ProxyConn {
   public:
    ProxyConn();
    ~ProxyConn();
    void Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, char* host_name);
    void Process();

   private:
    // host name and address
    char* host_;
    sockaddr_in address_;
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
