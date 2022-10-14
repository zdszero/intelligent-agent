#pragma once

#include <hiredis/hiredis.h>

#include <string>
#include <vector>

#include "log.h"

class RedisConn {
   public:
    RedisConn() {}
    ~RedisConn() { this->connect_ = NULL; }
    bool Connect(const std::string &host, int port) {
        struct timeval timeout = { 3, 0 };
        this->connect_ = redisConnectWithTimeout(host.c_str(), port, timeout);
        if (this->connect_ == nullptr || this->connect_->err) {
            printf("connect error: %s\n", this->connect_->errstr);
            return false;
        }
        return true;
    }

    void AppendLog(const std::string& client_id, std::string& log) { AppendLog(client_id, log.c_str()); }

    void AppendLog(const std::string& client_id, const char* log) {
        DPrintf("rpush %s:%s\n", client_id.c_str(), log);
        redisCommand(this->connect_, "RPUSH %s %s", client_id.c_str(), log);
    }

    void AppendLog(std::string client_id, std::vector<std::string>& logs) {
        if (logs.empty()) {
            return;
        }
        std::string sr = logs[0];
        for (auto& log : logs) {
            sr += " " + log;
        }
        redisCommand(this->connect_, "RPUSH %s %s", client_id.c_str(), sr.c_str());
    }

    // res 传引用，获取结果以存入res
    void GetLog(std::string client_id, std::vector<std::string>& res) {
        redisReply* _reply = (redisReply*)redisCommand(this->connect_, "LRANGE %s 0 -1", client_id.c_str());
        if (_reply->type == REDIS_REPLY_ARRAY) {
            for (int i = 0; i < _reply->elements; ++i) {
                res.push_back(_reply->element[i]->str);
            }
        }
    }

    std::string GetProxyMap(std::string& client_name) {
        redisReply* reply_ = (redisReply*)redisCommand(this->connect_, "HGET proxy_map %s", client_name.c_str());
        std::string str;
        if (reply_->type == REDIS_REPLY_STRING) {
            str = std::string{reply_->str};
        }
        freeReplyObject(reply_);
        return str;
    }

    int SetProxyMap(std::string& client_name, const char* host_name) {
        DPrintf("HSET proxy_map %s %s\n", client_name.c_str(), host_name);
        redisReply* _reply =
            (redisReply*)redisCommand(this->connect_, "HSET proxy_map %s %s", client_name.c_str(), host_name);
        if (_reply->type == REDIS_REPLY_ERROR) {
            return -1;
        }
        return 0;
    }

   private:
    redisContext* connect_;
};
