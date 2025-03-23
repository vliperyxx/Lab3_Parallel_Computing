#include <cstdlib>
#include <mutex>

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