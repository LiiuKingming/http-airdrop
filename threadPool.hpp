#ifndef __M_THR_H__
#define __M_THR_H__

#include <cstdio>
#include <stdlib.h>
#include <queue>
#include <thread>
#include <unistd.h>
#include <pthread.h>

#define MAX_THREAD 5
#define MAX_QUEUE 10

typedef void (*handler_t)(int data);

class ThreadTask {
    int m_data;
    handler_t m_handler;

public:
    ThreadTask(int data, handler_t handle):
        m_data(data),
        m_handler(handle){}

    void SetTask(int data, handler_t handler){
        m_data = data;
        m_handler = handler;
        return;
    }

    void TaskRun(){
        return m_handler(m_data);
    }
};

class ThreadPool {
    private:
        std::queue<ThreadTask> m_queue;
        int m_capacity;
        pthread_mutex_t m_mutex;
        pthread_cond_t m_condProducer;
        pthread_cond_t m_condConsumers;
        int m_maxThread;

        void thr_start(){
            while (1){
                pthread_mutex_lock(&m_mutex);
                while(m_queue.empty()) {
                    pthread_cond_wait(&m_condConsumers, &m_mutex);
                }
                ThreadTask tt = m_queue.front();
                m_queue.pop();
                pthread_mutex_unlock(&m_mutex);
                pthread_cond_signal(&m_condProducer);

                tt.TaskRun();
            }
            return;
        }

public:
    ThreadPool(int maxq = MAX_QUEUE, int maxt = MAX_THREAD):
        m_capacity(maxq),
        m_maxThread(maxt){
            pthread_mutex_init(&m_mutex, NULL);
            pthread_cond_init(&m_condConsumers, NULL);
            pthread_cond_init(&m_condProducer, NULL);
    }

    ~ThreadPool(){
        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_condConsumers);
        pthread_cond_destroy(&m_condProducer);
    }

    bool PoolInit(){
        for(int i = 0; i < m_maxThread; ++i){
            std::thread thr(&ThreadPool::thr_start, this);
            thr.detach();
        }
        return true;
    }

    bool TaskPush(ThreadTask &tt){
        pthread_mutex_lock(&m_mutex);
        while(m_queue.size() == m_capacity){
            pthread_cond_wait(&m_condProducer, &m_mutex);
        }
        m_queue.push(tt);
        pthread_mutex_unlock(&m_mutex);
        pthread_cond_signal(&m_condConsumers);
        return true;
    }
};
#endif
