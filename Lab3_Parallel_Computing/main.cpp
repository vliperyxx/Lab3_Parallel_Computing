#include <cstdlib>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <iostream>
#include <queue>

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

    std::shared_mutex m_rw_lock;    
public:
    Queue() = default;
    ~Queue() { clear(); }

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