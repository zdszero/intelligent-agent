#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

struct SysMsg {
    uint32_t len;
    char* buf;
    const char* Serialized() const {
        char* ptr = (char*)&len;
        char* data = (char*)malloc(sizeof(uint32_t) + sizeof(char) * (len + 1));
        data[0] = ptr[0];
        data[1] = ptr[1];
        data[2] = ptr[2];
        data[3] = ptr[3];
        strcpy(data + 4 , buf);
        return data;
    }
};

class MsgWrapper {
   public:
    MsgWrapper() {}
    static SysMsg wrap(const std::string& s) {
        SysMsg msg;
        msg.len = s.size();
        msg.buf = (char*)malloc(sizeof(msg.len));
        strcpy(msg.buf, s.c_str());
        return msg;
    }
    static SysMsg wrap(const char* m) {
        SysMsg msg;
        msg.len = strlen(m);
        msg.buf = (char*)malloc(sizeof(msg.len));
        strcpy(msg.buf, m);
        return msg;
    }
};
