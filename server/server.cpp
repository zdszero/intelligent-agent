#include "server.h"

#include <fcntl.h>

void TransferConn::Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, char* host_name, int epollfd) {
    sockfd_ = sockfd;
    redis_conn_ = rconn;
    epollfd_ = epollfd;

    IOWrapper::AddFd(epollfd_, sockfd, true);
}

void TransferConn::CloseConn() {
    IOWrapper::RemoveFd(epollfd_, sockfd_);
    sockfd_ = -1;
}

void TransferConn::Process() {
    struct sockaddr addr;
    socklen_t socklen;
    while (true) {
        int fd = accept(sockfd_, &addr, &socklen);
        // get client id from peer
        SysMsg msg;
        if (!IOWrapper::ReadSysMsg(fd, msg, true)) {
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

void ProxyConn::CloseConn() {
    IOWrapper::RemoveFd(epollfd_, sockfd_);
    sockfd_ = -1;
}


void ProxyConn::Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, char* host_name, int epollfd) {
    sockfd_ = sockfd, address_ = addr;
    redis_conn_ = rconn;
    redis_conf_conn_ = rconnm;
    host_ = host_name;
    epollfd_ = epollfd;
    IOWrapper::AddFd(epollfd, sockfd, true);
    init();
}

void ProxyConn::init() {
    conn_status_ = ConnStatus::UNVERIFIED;
}

std::string ProxyConn::getPrevProxy() { return redis_conf_conn_->getProxy_map(client_id_); }

int ProxyConn::setLocalProxy() { return redis_conf_conn_->setProxy_map(client_id_, host_); }

std::string ProxyConn::parseClient() {
    SysMsg msg;
    if (!IOWrapper::ReadSysMsg(sockfd_, msg, true)) {
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
    while (IOWrapper::ReadSysMsg(sockfd, msg, true)) {
        redis_conn_->appendLog(client_id_, msg.buf);
        free(msg.buf);
    }
    close(sockfd);
    return msg.len;
}

int ProxyConn::runProxyLoop() {
    SysMsg msg;
    while (IOWrapper::ReadSysMsg(sockfd_, msg, false)) {
        redis_conn_->appendLog(client_id_, msg.buf);
        free(msg.buf);
    }
    return msg.len;
}

void ProxyConn::Process() {
    if (conn_status_ == ConnStatus::UNVERIFIED) {
        client_id_ = parseClient();
        if (client_id_.size() == 0) {
            CloseConn();
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
            CloseConn();
            return;
        }
        conn_status_ = ConnStatus::CONNECTED;
    } else if (conn_status_ == ConnStatus::CONNECTED) {
        int ret = runProxyLoop();
        if (ret < 0) {
            CloseConn();
        }
    } else {
        CloseConn();
    }
    IOWrapper::ModFd(epollfd_, sockfd_, EPOLLOUT);
    
}
