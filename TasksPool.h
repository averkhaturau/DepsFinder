#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <thread>
#include <vector>
#include <optional>


// Handles tasks instd::function to run in parallel
class TasksPool
{
public:
    void addTask(std::function<void()>&& task) {
        if (runningTasksCount++ > std::thread::hardware_concurrency()) {
            --runningTasksCount;
            std::lock_guard<std::mutex> lock(m_tasksMutex);
            m_deferredTasks.emplace_back(std::move(task));
        } else {
            m_runningTasks.emplace_back(runTask(m_runningTasks.size(), std::move(task)));
        }
        ++allTasksCount;
    }

    void clear() {
        m_deferredTasks.clear();
        m_runningTasks.clear(); // will wait for tasks to finish
        runningTasksCount = 0;
    }

    ~TasksPool() {
    	while(runningTasksCount > 0)
    		std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    short progress() const {
        m_zombieTasks.clear();
        return allTasksCount > 0 ? finishedTasksCount * 100 / allTasksCount : -1;
    }

private:

    std::future<void> runTask(const size_t id, std::function<void()>&& task) {
        return std::async(std::launch::async, [=](){
            task();
            runNext(id);
        });
    }

    void runNext(size_t const id) {
        const auto getNextTask = [this,id]() {
            std::optional<std::function<void()>> nextTask;
            std::lock_guard<std::mutex> lock(m_tasksMutex);
            if (!m_deferredTasks.empty()) {
                nextTask = std::move(m_deferredTasks.front());
                m_deferredTasks.pop_front();
                m_zombieTasks.push_back(std::move(m_runningTasks[id]));
            }
            return nextTask;
        };
        
        if (auto nextTask = getNextTask()) {
            m_runningTasks[id] = runTask(id, std::move(nextTask.value()));
        } else
            --runningTasksCount;
        ++finishedTasksCount;
    }

    std::atomic<size_t> runningTasksCount = 0;
    std::vector<std::future<void>> m_runningTasks;
    std::list<std::function<void()>> m_deferredTasks;
    mutable std::vector<std::future<void>> m_zombieTasks;
    mutable std::mutex m_tasksMutex;
    size_t allTasksCount = 0;
    std::atomic<size_t> finishedTasksCount = 0;
};
