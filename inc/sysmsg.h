#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

struct SysMsg {
    uint32_t len;
    char* buf;
    char* Serialized(size_t &length) const {
        length = sizeof(uint32_t) + len;
        char* data = (char*)malloc(length);
        *(reinterpret_cast<uint32_t *>(data)) = len;
        strcpy(data + 4 , buf);
        return data;
    }
};

class MsgWrapper {
   public:
    MsgWrapper() {}
    static SysMsg wrap(const std::string& s) {
        SysMsg msg;
        msg.len = s.size() + 1;
        msg.buf = (char*)malloc(msg.len);
        strcpy(msg.buf, s.c_str());
        return msg;
    }
    static SysMsg wrap(const char* m) {
        SysMsg msg;
        msg.len = strlen(m) + 1;
        msg.buf = (char*)malloc(msg.len);
        strcpy(msg.buf, m);
        return msg;
    }
};
