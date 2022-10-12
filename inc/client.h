#pragma once

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "sysmsg.h"

class ZZElement {
   public:
    ZZElement() = default;
    ZZElement(uint id, const std::string& name, const std::string& cert) : id_(id), name_(name), conn_cert_(cert) {}
    std::string Name() { return name_; }
    std::string Cert() { return conn_cert_; }
    std::string GenerateMessage();

   private:
    uint id_;
    std::string name_;
    std::string conn_cert_;
    int counter_{0};
};

class Addr {
   public:
    Addr() = default;
    Addr(std::string&, uint);
    Addr(const char*, uint);
    Addr(in_addr, uint);
    std::string GetAddrInfo() const;
    sockaddr_in GetSockAddr() const;

   private:
    in_addr addr;
    uint16_t port;
};

class LogClient {
    enum class Status {};

   public:
    LogClient(ZZElement* zz) : zz_(zz) {}
    LogClient(ZZElement* zz, const std::vector<Addr>& addrs) : zz_(zz), entry_points_(addrs) {}
    LogClient(ZZElement* zz, const Addr& addr) : zz_(zz), entry_points_(std::vector<Addr>{addr}) {}
    bool Connect(const Addr& addr);
    void Close();
    void Send();

   private:
    int sockfd_;
    Addr cur_addr_{};
    ZZElement* zz_;
    std::vector<Addr> entry_points_;
};
