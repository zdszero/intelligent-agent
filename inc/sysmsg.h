#pragma once

#include <cstdint>
#include <cstring>
#include <string>

struct SysMsg {
    uint32_t len;
    char* buf;
    SysMsg() = default;
    SysMsg(const std::string &s) {
        len = s.size() + 1;
        buf = (char*)malloc(len);
        strcpy(buf, s.c_str());
    }
    SysMsg(const char *m) {
        len = strlen(m) + 1;
        buf = (char*)malloc(len);
        strcpy(buf, m);
    }
    char* Serialized(size_t &length) const {
        length = sizeof(uint32_t) + len;
        char* data = (char*)malloc(length);
        *(reinterpret_cast<uint32_t *>(data)) = len;
        strcpy(data + 4 , buf);
        return data;
    }
};
