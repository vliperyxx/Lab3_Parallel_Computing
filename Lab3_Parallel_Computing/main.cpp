#include <cstdlib>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <iostream>
#include <queue>
#include <map>

using read_lock = std::shared_lock<std::shared_mutex>;
using write_lock = std::unique_lock<std::shared_mutex>;

class Task {
private:
    int executionTime;
    int id;               

    static const int MIN_EXEC_TIME = 3;
    static const int MAX_EXEC_TIME = 6;

    static int counter;
    static std::mutex counterMutex;

public:
    Task() {
        executionTime = rand() % (MAX_EXEC_TIME - MIN_EXEC_TIME + 1) + MIN_EXEC_TIME;

        counterMutex.lock();
        id = ++counter;
        counterMutex.unlock();
    }

    int getExecutionTime() const {
        return executionTime;
    }

    int getId() const {
        return id;
    }
};

int Task::counter = 0;
std::mutex Task::counterMutex;

class Queue {
private:
    std::queue<Task> tasks;   
    int totalQueueLength = 0;       
    int measureCount = 0;
    std::map<std::thread::id, long long> threadWaitTimes; 
    std::shared_mutex m_rw_lock;    
    std::condition_variable_any condVar;   

    void recordQueueLength() {
        totalQueueLength += tasks.size();
        measureCount++;
    }
    void recordThreadWaitTime(long long waitTime) {
        threadWaitTimes[std::this_thread::get_id()] += waitTime;
    }

public:
    Queue() = default;
    ~Queue() { clear(); }

    void emplace(const Task& task) {
        write_lock lock(m_rw_lock);
        tasks.emplace(task); 
        recordQueueLength();
        condVar.notify_one();
    }

    Task pop() {
        write_lock lock(m_rw_lock);
        auto startWait = std::chrono::high_resolution_clock::now();
        while (tasks.empty()) {
            condVar.wait(lock);
        }
        auto endWait = std::chrono::high_resolution_clock::now();
        long long waitDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(endWait - startWait).count();
        recordThreadWaitTime(waitDuration);

        Task task = tasks.front();
        tasks.pop();
        recordQueueLength();
        return task;
    }

    bool empty() {
        read_lock lock(m_rw_lock);
        return tasks.empty();
    }

    int size() {
        read_lock lock(m_rw_lock);
        return tasks.size();
    }

    void clear() {
        write_lock lock(m_rw_lock);
        while (!tasks.empty()) {
            tasks.pop();
        }
    }
};

int main() {
    srand(time(0));

    return 0;
}