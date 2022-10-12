#include <cstdio>
#include <fstream>
#include <iostream>

#include "client.h"
#include "iowrapper.h"
#include "log.h"

int main(int argc, char *argv[]) {
#ifdef IPPROTO_MPTCP
    DPrintf("use MPTCP\n");
#else
    DPrintf("use TCP\n");
#endif
    // if (argc != 2) {
    //     fprintf(stderr, "usage: %s <server config file>\n", argv[0]);
    //     exit(1);
    // }
    // std::ifstream ifs(argv[0]);
    // std::vector<Addr> addrs;
    // std::string server;
    // while (std::getline(ifs, server)) {
    //     addrs.push_back({server, 30071});
    // }
    ZZElement zz(1, "zz1", "default_cert");
    LogClient cli(&zz);
    cli.Connect(Addr{"172.16.1.145", 30071});
    for (size_t i = 0; i < 10; i++) {
        cli.Send();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    cli.Close();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cli.Connect(Addr{"172.16.1.58", 30071});
    for (size_t i = 0; i < 10; i++) {
        cli.Send();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
