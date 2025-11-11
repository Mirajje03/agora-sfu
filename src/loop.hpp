#pragma once

#include <condition_variable>
#include <mutex>
#include <functional>
#include <queue>

namespace sfu {

using Task = std::function<void()>;

class Loop {
public:
    void EnqueueTask(Task&& task);
    void Run();

private:
    std::mutex Mutex_;
    std::condition_variable Cv_;
    std::queue<Task> TaskQueue_;
};

} // namespace sfu
