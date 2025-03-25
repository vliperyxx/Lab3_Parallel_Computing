#include <cstdlib>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <iostream>
#include <queue>
#include <map>

using read_lock = std::shared_lock<std::shared_mutex>;
using write_lock = std::unique_lock<std::shared_mutex>;
std::mutex coutMutex;

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
    bool m_shutdown = false; 

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

    void shutdownQueue() {
        write_lock lock(m_rw_lock);
        m_shutdown = true;
        condVar.notify_all();
    }

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

    double getAverageQueueLength() {
        read_lock lock(m_rw_lock);
        if (measureCount == 0) {
            return 0;
        }
        else {
            double averageQueueLength = double(totalQueueLength) / measureCount;
            return averageQueueLength;
        }
    }

    void showStatistics() {
        read_lock lock(m_rw_lock);
        std::cout << "\nAverage queue length: " << getAverageQueueLength() << std::endl;
        std::cout << "Thread wait times:" << std::endl;
        for (auto& entry : threadWaitTimes) {
            double waitTimeMs = entry.second / 1000000.0;
            std::cout << "Thread " << entry.first << ": " << waitTimeMs << " ms" << std::endl;
        }
    }
};

class ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool() { terminate(); }

    void initialize(const size_t worker_count) {
        write_lock lock(m_rw_lock);
        if (m_initialized || m_terminated) {
            return;
        }
        m_workers.reserve(worker_count);
        for (size_t id = 0; id < worker_count; id++) {
            m_workers.emplace_back(&ThreadPool::routine, this);
        }
        m_initialized = !m_workers.empty();
    }
    
    void add_task(const Task& task) {
        {
            read_lock lock(m_rw_lock);
            if (!working_unsafe()) {
                return;
            }
        }
        m_queue.emplace(task);
        m_taskWaiter.notify_one();
    }

    void pause() {
        write_lock lock(m_rw_lock);
        if (!working_unsafe()) {
            return;
        }
        m_paused = true;
    }

    void resume() {
        {
            write_lock lock(m_rw_lock);
            if (!working_unsafe()) {
                return;
            }
            m_paused = false;
        }
        m_taskWaiter.notify_all();
    }

    void start() {
        {
            write_lock lock(m_rw_lock);
            if (!working_unsafe()) {
                return;
            }
            std::cout << "Thread pool started." << std::endl;
            m_terminated = false;
            m_paused = false;
        }
        m_taskWaiter.notify_all();
    }

    void terminate() {
        {
            write_lock lock(m_rw_lock);
            if (working_unsafe()) {
                m_terminated = true;
                m_forceStop = true;
            }
            else {
                m_workers.clear();
                m_terminated = false;
                m_initialized = false;
                return;
            }
        }
        m_queue.shutdownQueue();
        m_taskWaiter.notify_all();
        for (std::thread& worker : m_workers) {
            worker.join();
        }
        m_workers.clear();
        m_terminated = false;
        m_initialized = false;
    }

    void terminateNow() {
        {
            write_lock lock(m_rw_lock);
            m_terminated = true;
            m_forceStop = true;
        }
        m_taskWaiter.notify_all();
        for (auto& worker : m_workers) {
            worker.join();
        }
        m_workers.clear();
        m_initialized = false;
    }

    size_t getTotalTasksExecuted() const {
        return m_totalTasksExecuted;
    }

    double getAverageExecutionTime() const {
        if (m_totalTasksExecuted == 0) {
            return 0;
        }
        else {
            double average = (double)m_totalTaskExecutionTime / m_totalTasksExecuted;
            return average;
        }
    }

    void showStatistics() {
        {
            write_lock lock(m_rw_lock);
            std::cout << "\nTotal tasks executed: " << getTotalTasksExecuted() << std::endl;
            std::cout << "Average execution time: " << getAverageExecutionTime() / 1000000 << " ms" << std::endl;
        }

        std::cout << "\nQueue statistics:" << std::endl;
        m_queue.showStatistics();
    }
private:
    mutable std::shared_mutex m_rw_lock;
    std::condition_variable_any m_taskWaiter;
    std::vector<std::thread> m_workers;
    Queue m_queue;
    size_t m_totalTasksExecuted = 0;
    long long m_totalTaskExecutionTime = 0;

    bool m_initialized = false;
    bool m_terminated = false;
    bool m_paused = false;
    bool m_forceStop = false;

    void routine() {
        while (true) {
            {
                write_lock lock(m_rw_lock);
                while (m_paused && !m_terminated) {
                    m_taskWaiter.wait(lock);
                }
                if (m_terminated || m_forceStop) {
                    return;
                }
            }

            Task task = m_queue.pop();
            {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "Thread " << std::this_thread::get_id() << " started executing task " << task.getId() << " (duratiion: " << task.getExecutionTime() << " s)" << std::endl;
            }

            auto start_time = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_for(std::chrono::seconds(task.getExecutionTime()));
            auto end_time = std::chrono::high_resolution_clock::now();
            long long executionDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

            {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "Thread " << std::this_thread::get_id() << " finished executing task " << task.getId() << std::endl;
            }

            {
                write_lock lock(m_rw_lock);
                m_totalTasksExecuted++;
                m_totalTaskExecutionTime += executionDuration;
            }
        }
    }

    bool working() const {
        read_lock lock(m_rw_lock);
        return working_unsafe();
    }

    bool working_unsafe() const {
        return m_initialized && !m_terminated;
    }
};

class TaskProducer {
public:    
    TaskProducer(ThreadPool& pool) : m_pool(pool) {}

    static const int MIN_SLEEP_SECONDS = 1;
    static const int MAX_SLEEP_SECONDS = 5;

    void run() {
        while (m_running) {
            Task newTask;
            m_pool.add_task(newTask);
            int sleepTime = rand() % (MAX_SLEEP_SECONDS - MIN_SLEEP_SECONDS + 1) + MIN_SLEEP_SECONDS;
            std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
        }
    }

    void stop() {
        m_running = false;
    }

private:
    ThreadPool& m_pool;      
    bool m_running; 
};

int main() {
    srand(time(0));

    return 0;
}