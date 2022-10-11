#include "server.h"

#include <fcntl.h>

void TraverseConn::Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, char* host_name) {
    sockfd_ = sockfd;
    redis_conn_ = rconn;
}

void TraverseConn::Process() {
    struct sockaddr addr;
    socklen_t socklen;
    for (;;) {
        int fd = accept(sockfd_, &addr, &socklen);
        // get client id from peer
        SysMsg msg;
        if (!IOWrapper::ReadSysMsg(fd, msg)) {
            fprintf(stderr, "TraverseConn fail to receive client address from peer\n");
            exit(1);
        }
        std::string client_id(msg.buf);
        // get all the log entries from redis
        std::vector<std::string> log_entries;
        redis_conn_->getLog(client_id, log_entries);
        for (const string& entry : log_entries) {
            IOWrapper::SendSysMsg(fd, MsgWrapper::wrap(entry));
        }
        close(fd);
    }
}

int ProxyConn::tcpDial(const std::string& host, int port) {
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, host.c_str(), &address.sin_addr);
    address.sin_port = htons(port);
#ifdef IPPROTO_MPTCP
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);
#else
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
#endif
    IOWrapper::SetNonBlocking(sockfd);
    int ret = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    if (ret == 0 || errno == EINPROGRESS) {
        return sockfd;
    }
    return -1;
}

void ProxyConn::closeConn() {
    IOWrapper::RemoveFd(sockfd_);
    sockfd_ = -1;
}


void ProxyConn::Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, char* host_name) {
    sockfd_ = sockfd, address_ = addr;
    redis_conn_ = rconn;
    redis_conf_conn_ = rconnm;
    host_ = host_name;
    IOWrapper::AddFd(sockfd, true);
    init();
}

void ProxyConn::init() {
    conn_status_ = ConnStatus::UNVERIFT;
}

std::string ProxyConn::getPrevProxy() { return redis_conf_conn_->getProxy_map(client_id_); }

int ProxyConn::setLocalProxy() { return redis_conf_conn_->setProxy_map(client_id_, host_); }

std::string ProxyConn::parseClient() {
    SysMsg msg;
    if (!IOWrapper::ReadSysMsg(sockfd_, msg)) {
        return {""};
    }
    string client_id = string(msg.buf);
    free(msg.buf);
    msg.buf = nullptr;
    return client_id;
}

int ProxyConn::logTraverse(const string& remote_proxy, const string& client_id_) {
    int sockfd = tcpDial(remote_proxy, PROXY_TRANVERSE_PORT);
    // send client id to peer
    SysMsg client_msg = MsgWrapper::wrap(client_id_);
    IOWrapper::SendSysMsg(sockfd, client_msg);
    // read data from peer
    SysMsg msg;
    while (IOWrapper::ReadSysMsg(sockfd, msg)) {
        redis_conn_->appendLog(client_id_, msg.buf);
        free(msg.buf);
    }
    close(sockfd);
    return msg.len;
}

int ProxyConn::runProxyLoop() {
    SysMsg msg;
    while (IOWrapper::ReadSysMsg(sockfd_, msg)) {
        redis_conn_->appendLog(client_id_, msg.buf);
        free(msg.buf);
    }
    return msg.len;
}

// 处理入口函数
void ProxyConn::Process() {
    client_id_ = parseClient();
    if (client_id_.size() == 0) {
        closeConn();
        return;
    }
#ifdef _WITH_CERT_
    if (!clientVarify()) {
        close_conn();
        return;
    }
#endif
    string prevProxy = getPrevProxy();
    if (!prevProxy.empty() && prevProxy.compare(host_) != 0) {
        logTraverse(prevProxy, client_id_);
    }
    if (setLocalProxy() < 0) {
        closeConn();
        return;
    }
    int ret = runProxyLoop();
    IOWrapper::ModFd(sockfd_, EPOLLOUT);
    closeConn();
}
