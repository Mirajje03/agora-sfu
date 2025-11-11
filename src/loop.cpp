#include "loop.hpp"

namespace sfu {

void Loop::Run() {
    while (true)
    {
        Task task;

        {
            std::unique_lock<std::mutex> lock(Mutex_);
            Cv_.wait(lock, [this]{
                return !TaskQueue_.empty();
            });
            task = std::move(TaskQueue_.front());
            TaskQueue_.pop();
        }
        task();
    }
}

void Loop::EnqueueTask(Task&& task) {
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        TaskQueue_.push(std::move(task));
    }

    Cv_.notify_one();
}

} //namespace sfu
