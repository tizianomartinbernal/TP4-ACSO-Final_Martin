/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
#include <condition_variable>

using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : workers(numThreads), sem_queue(0), sem_workers(numThreads){
    dt = thread([this](){dispatcher();});
    
    for (size_t i = 0; i < numThreads; i++) {
        workers[i].th = thread([this, i](){worker(i);});
    }
}

void ThreadPool::dispatcher(){
    while(true){
        sem_queue.wait();

        if (finish) {break;}

        mtx_queue.lock();
        function<void(void)> task = tasks.front();
        tasks.pop();
        mtx_queue.unlock();
        sem_workers.wait();

        for (size_t i = 0; i < workers.size(); i++) {
            if (!workers[i].busy) {
                workers[i].task = task;
                workers[i].busy = true;
                workers[i].sem.signal();
                break;
            }
        }
   }

    for (size_t i = 0; i < workers.size(); i++) {
        workers[i].sem.signal();
    }
}

void ThreadPool::worker(int id){
    while(1) {
        
        workers[id].sem.wait();

        if (finish) {break;}

        workers[id].task();
        mtx_count.lock();
        remaining_tasks--;
        mtx_count.unlock();

        workers[id].busy = false;
        sem_workers.signal();

        cv_wait.notify_all(); // también podría chequear si quedan tareas pendientes y en ese caso notificar
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    mtx_queue.lock();
    tasks.push(thunk);
    mtx_count.lock();
    remaining_tasks++;
    mtx_count.unlock();
    mtx_queue.unlock();
    sem_queue.signal();
}

void ThreadPool::wait() {
    unique_lock<mutex> lk(mtx_wait);
    cv_wait.wait(lk, [this](){return remaining_tasks == 0 && tasks.empty();});
}
ThreadPool::~ThreadPool() {
    wait();
    finish = true;
    sem_queue.signal();
    dt.join();
    for (size_t i = 0; i < workers.size(); i++) {
        workers[i].th.join();
    }
}