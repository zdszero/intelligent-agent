#pragma once

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

// T request type
template< typename T>
class threadpool {
public:
    threadpool(int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request);
private:
    static void* worker(void* arg);
    void run();

private:
    int m_thread_number;
    int m_max_requests;
    pthread_t* m_threads;
    std::list< T* > m_workqueue; //请求队列
    locker m_queuelocker;
    sem m_queuestat;
    bool m_stop;
};

template< typename T >
threadpool<T>::threadpool(int thread_num, int max_req) : 
    m_thread_number(thread_num), m_max_requests(max_req), m_stop(false), m_threads(NULL) {
    if((thread_num <= 0) || (max_req <= 0)) {
        throw std::exception();
    }
    m_threads = new pthread_t(m_thread_number);
    if (!m_threads) {
        throw std::exception();
    }
    for (int i = 0; i < thread_num; ++i) {
        // 传入this指针以使得线程能够访问类内资源
        if (pthread_create(m_threads + i, nullptr, worker, this) != 0) {
            delete [] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template< typename T >
threadpool<T>::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}

template< typename T >
bool threadpool<T>::append(T *request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template< typename T >
void *threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool *)arg;
    pool->run();
    return pool;
}


template< typename T >
void threadpool<T>::run() {
    while(!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (request == nullptr) {
            continue;
        }
        request->Process();
    }
}
