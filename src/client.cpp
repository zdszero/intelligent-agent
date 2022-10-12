#include "client.h"

#include <string.h>
#include <unistd.h>

#include "iowrapper.h"
#include "log.h"
#include "sysmsg.h"

std::string ZZElement::GenerateMessage() {
    time_t now = time(nullptr);
    tm* curr_tm = localtime(&now);
    char c_time[80] = {0};
    strftime(c_time, 80, "%Y-%m-%d %H:%M:%S", curr_tm);
    return std::string(c_time) + " - " + std::to_string(counter_++);
}

Addr::Addr(std::string& host, uint port) {
    inet_pton(AF_INET, host.data(), &this->addr);
    this->port = port;
}

Addr::Addr(const char* host, uint port) {
    inet_pton(AF_INET, host, &this->addr);
    this->port = port;
}

std::string Addr::GetAddrInfo() const { return std::string(inet_ntoa(this->addr)) + ":" + std::to_string(this->port); }

sockaddr_in Addr::GetSockAddr() const {
    sockaddr_in ip;
    bzero(&ip, sizeof(ip));
    ip.sin_addr = this->addr;
    ip.sin_family = AF_INET;
    ip.sin_port = htons(this->port);
    return ip;
}

bool LogClient::Connect(const Addr& addr) {
#ifdef IPPROTO_MPTCP
    sockfd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);
#else
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
#endif
    sockaddr_in sockaddr = addr.GetSockAddr();
    if (connect(sockfd_, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        perror("client connect failed");
        close(sockfd_);
        return false;
    }
    cur_addr_ = addr;
    IOWrapper::SendSysMsg(sockfd_, MsgWrapper::wrap(zz_->Name()));
    return true;
}

void LogClient::Close() {
    close(sockfd_);
}

void LogClient::Send() {
    std::string msg = zz_->GenerateMessage();
    IOWrapper::SendSysMsg(sockfd_, MsgWrapper::wrap(msg));
    DPrintf("send %s to %s\n", msg.c_str(), cur_addr_.GetAddrInfo().c_str());
}
