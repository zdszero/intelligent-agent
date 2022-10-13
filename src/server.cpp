#include "server.h"

#include <fcntl.h>

#include "log.h"

void TransferConn::Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, const char* host_name,
                        int epollfd) {
    sockfd_ = sockfd;
    peer_addr_ = addr;
    redis_conn_ = rconn;
    epollfd_ = epollfd;
    IOWrapper::AddFd(epollfd_, sockfd, true);
}

void TransferConn::CloseConn() {
    IOWrapper::RemoveFd(epollfd_, sockfd_);
    sockfd_ = -1;
}

void TransferConn::Process() {
    // TODO: cert
    struct sockaddr addr;
    socklen_t socklen;
    DPrintf("transfer send process\n");
    // get client id from peer
    SysMsg msg;
    if (!IOWrapper::ReadSysMsg(sockfd_, msg, true)) {
        fprintf(stderr, "TraverseConn fail to receive client address from peer\n");
        CloseConn();
        return;
    }
    std::string client_id(msg.buf);
    // get all the log entries from redis
    std::vector<std::string> log_entries;
    redis_conn_->GetLog(client_id, log_entries);
    IOWrapper::SendSysMsg(sockfd_, MsgWrapper::wrap(std::to_string(log_entries.size())));
    for (const string& entry : log_entries) {
        char buf[30];
        inet_ntop(AF_INET, &peer_addr_.sin_addr, buf, sizeof(peer_addr_));
        DPrintf("transter %s to %s\n", entry.c_str(), buf);
        IOWrapper::SendSysMsg(sockfd_, MsgWrapper::wrap(entry));
    }
    shutdown(sockfd_, SHUT_WR);
    IOWrapper::ReadSysMsg(sockfd_, msg, true);
    if (msg.buf) {
        free(msg.buf);
    }
    CloseConn();
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

void ProxyConn::Init(int sockfd, const sockaddr_in& addr, RedisConn* rconn, RedisConn* rconnm, const char* host_name,
                     int epollfd) {
    sockfd_ = sockfd, address_ = addr;
    redis_conn_ = rconn;
    redis_conf_conn_ = rconnm;
    host_ = host_name;
    epollfd_ = epollfd;
    conn_status_ = ConnStatus::UNVERIFIED;
    inited_ = true;
    IOWrapper::AddFd(epollfd, sockfd, true);
}

std::string ProxyConn::getPrevProxy() { return redis_conf_conn_->GetProxyMap(client_id_); }

int ProxyConn::setLocalProxy() { return redis_conf_conn_->SetProxyMap(client_id_, host_); }

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

int ProxyConn::logTransfer(const string& remote_proxy, const string& client_id_) {
    DPrintf("transfer fetch process\n");
    int sockfd = tcpDial(remote_proxy, PROXY_TRANSFER_PORT);
    // send client id to peer
    SysMsg client_msg = MsgWrapper::wrap(client_id_);
    IOWrapper::SendSysMsg(sockfd, client_msg);
    // read data from peer
    SysMsg msg;
    if (!IOWrapper::ReadSysMsg(sockfd, msg, true)) {
        fprintf(stderr, "logTransfer fail to get entry count\n");
        CloseConn();
        return -1;
    }
    size_t count = std::atoi(msg.buf);
    free(msg.buf);
    for (size_t i = 0; i < count; i++) {
        if (!IOWrapper::ReadSysMsg(sockfd, msg, true)) {
            fprintf(stderr, "logTransfer fail to get %ld entry\n", i);
            CloseConn();
            return -1;
        }
        redis_conn_->AppendLog(client_id_, msg.buf);
        free(msg.buf);
    }
    // IOWrapper::SendSysMsg(sockfd, MsgWrapper::wrap("transfer_end"));
    DPrintf("exit log transfer loop\n");
    close(sockfd);
    return msg.len;
}

int ProxyConn::runProxyLoop() {
    DPrintf("enter proxy loop\n");
    SysMsg msg;
    while (IOWrapper::ReadSysMsg(sockfd_, msg, true)) {
        redis_conn_->AppendLog(client_id_, msg.buf);
        free(msg.buf);
    }
    return msg.len;
}

void ProxyConn::Process() {
    client_id_ = parseClient();
    DPrintf("connected with client %s\n", client_id_.c_str());
    if (client_id_.size() == 0) {
        CloseConn();
        return;
    }
    // #ifdef _WITH_CERT_
    //         if (!clientVarify()) {
    //             close_conn();
    //             return;
    //         }
    // #endif
    string prevProxy = getPrevProxy();
    DPrintf("prev proxy = %s\n", prevProxy.c_str());
    if (!prevProxy.empty() && prevProxy.compare(host_) != 0) {
        logTransfer(prevProxy, client_id_);
    }
    if (setLocalProxy() < 0) {
        CloseConn();
        return;
    }
    runProxyLoop();
    CloseConn();
    // IOWrapper::ModFd(epollfd_, sockfd_, EPOLLOUT);
}
