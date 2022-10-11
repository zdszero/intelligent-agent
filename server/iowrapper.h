#pragma once

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "sysmsg.h"

class IOWrapper {
   public:
    IOWrapper() = default;
    ~IOWrapper() = default;

    int SetNonBlocking(int fd) {
        int old_op = fcntl(fd, F_GETFL);
        int new_op = old_op | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_op);
        return old_op;
    }

    void AddFd(int fd, bool one_shot) {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        if (one_shot) {
            event.events |= EPOLLONESHOT;
        }
        epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event);
        SetNonBlocking(fd);
    }

    void RemoveFd(int fd) {
        epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, 0);
        close(fd);
    }

    void ModFd(int fd, int ev) {
        epoll_event event;
        event.data.fd = fd;
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLHUP;
        epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event);
    }

    void SendSysMsg(int fd, const SysMsg &msg) {
        size_t send_len = sizeof(uint32_t) + strlen(msg.buf);
        const char *data = msg.Serialized();
        if (!sendBytes(fd, data, send_len)) {
            fprintf(stderr, "Fail to send SysMsg\n");
            exit(1);
        }
    }

    bool ReadSysMsg(int fd, SysMsg &msg) {
        char *buf = readBytes(4, fd);
        if (buf == nullptr) {
            return false;
        }
        msg.len = *((uint32_t *)buf);
        free(buf);
        buf = readBytes(msg.len, fd);
        if (buf == nullptr) {
            return false;
        }
        msg.buf = buf;
        return true;
    }

   private:
    int epollfd_;

    char *readBytes(int fd, size_t len) {
        char *buf = (char *)malloc((sizeof(char) + 1) * len);
        int read_idx = 0;
        size_t bytes_read;
        while (read_idx != len) {
            bytes_read = recv(fd, buf + read_idx, len - read_idx, 0);
            if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                return nullptr;
            } else if (bytes_read == 0) {
                return nullptr;
            }
            read_idx += bytes_read;
        }
        return buf;
    }

    bool sendBytes(int fd, const char *data, size_t len) {
        int write_idx = 0;
        size_t bytes_written;
        while (write_idx != len) {
            bytes_written = send(fd, data, len - write_idx, 0);
            if (bytes_written == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                return false;
            } else if (bytes_written == 0) {
                return false;
            }
            write_idx += bytes_written;
        }
        return true;
    }
};
