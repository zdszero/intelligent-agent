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

    static int SetNonBlocking(int fd) {
        int old_op = fcntl(fd, F_GETFL);
        int new_op = old_op | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_op);
        return old_op;
    }

    static void AddFd(int epollfd, int fd, bool one_shot) {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        if (one_shot) {
            event.events |= EPOLLONESHOT;
        }
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
        SetNonBlocking(fd);
    }

    static void RemoveFd(int epollfd, int fd) {
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
        close(fd);
    }

    static void ModFd(int epollfd, int fd, int ev) {
        epoll_event event;
        event.data.fd = fd;
        event.events = ev | EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLHUP;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    }

    static void SendSysMsg(int fd, const SysMsg &msg) {
        size_t send_len = sizeof(uint32_t) + strlen(msg.buf);
        const char *data = msg.Serialized();
        if (!sendBytes(fd, data, send_len)) {
            fprintf(stderr, "Fail to send SysMsg\n");
            exit(1);
        }
    }

    static bool ReadSysMsg(int fd, SysMsg &msg, bool persist) {
        char *buf = readBytes(4, fd, persist);
        if (buf == nullptr) {
            return false;
        }
        msg.len = *((uint32_t *)buf);
        free(buf);
        buf = readBytes(msg.len, fd, true);
        if (buf == nullptr) {
            return false;
        }
        msg.buf = buf;
        return true;
    }

   private:
    static char *readBytes(int fd, size_t len, bool persist) {
        char *buf = (char *)malloc(sizeof(char) * (len + 1));
        int read_idx = 0;
        size_t bytes_read;
        while (read_idx != len) {
            bytes_read = recv(fd, buf + read_idx, len - read_idx, 0);
            if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (!persist) {
                        break;
                    }
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

    static bool sendBytes(int fd, const char *data, size_t len) {
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