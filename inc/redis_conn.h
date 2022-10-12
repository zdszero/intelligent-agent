#pragma once

#include <hiredis/hiredis.h>

#include <string>
#include <vector>

#include "log.h"

class RedisConn {
   public:
    RedisConn() {}
    ~RedisConn() { this->_connect = NULL; }
    bool Connect(std::string host, int port) {
        this->_connect = redisConnect(host.c_str(), port);
        if (this->_connect != NULL && this->_connect->err) {
            printf("connect error: %s\n", this->_connect->errstr);
            return 0;
        }
        return 1;
    }

    void AppendLog(const std::string& client_id, std::string& log) { AppendLog(client_id, log.c_str()); }

    void AppendLog(const std::string& client_id, const char* log) {
        DPrintf("rpush %s:%s\n", client_id.c_str(), log);
        redisCommand(this->_connect, "RPUSH %s %s", client_id.c_str(), log);
    }

    void AppendLog(std::string client_id, std::vector<std::string>& logs) {
        if (logs.empty()) {
            return;
        }
        std::string sr = logs[0];
        for (auto& log : logs) {
            sr += " " + log;
        }
        redisCommand(this->_connect, "RPUSH %s %s", client_id.c_str(), sr.c_str());
    }

    // res 传引用，获取结果以存入res
    void GetLog(std::string client_id, std::vector<std::string>& res) {
        redisReply* _reply = (redisReply*)redisCommand(this->_connect, "LRANGE %s 0 -1", client_id.c_str());
        if (_reply->type == REDIS_REPLY_ARRAY) {
            for (int i = 0; i < _reply->elements; ++i) {
                res.push_back(_reply->element[i]->str);
            }
        }
    }

    std::string GetProxyMap(std::string& client_name) {
        redisReply* _reply = (redisReply*)redisCommand(this->_connect, "HGET proxy_map %s", client_name.c_str());
        std::string str;
        if (true) {
            str = std::string{_reply->str};
        }
        freeReplyObject(_reply);
        return str;
    }

    int SetProxyMap(std::string& client_name, const char* host_name) {
        DPrintf("HSET proxy_map %s %s\n", client_name.c_str(), host_name);
        redisReply* _reply =
            (redisReply*)redisCommand(this->_connect, "HSET proxy_map %s %s", client_name.c_str(), host_name);
        if (_reply->type == REDIS_REPLY_ERROR) {
            return -1;
        }
        return 0;
    }

   private:
    redisContext* _connect;
};
