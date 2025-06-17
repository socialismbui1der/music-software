#pragma once
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <thread>
#include <vector>
#include <functional>

class ThreadPool {
public:
    static ThreadPool& getinstance(){
        static ThreadPool temp(3);
        return temp;
    }
    
    void addTask(std::function<void()> task) {
        boost::asio::post(strand, task); // 添加到 strand，确保按顺序执行
    }

    void stop() {
        work_guard.reset(); // 释放 work_guard，让 io_context.run() 能够退出
        io_context.stop();  // 停止 io_context
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();  // 等待所有线程退出
            }
        }
        //std::cout<<"线程池安全退出"<<std::endl;
    }

private:
    ThreadPool(size_t numThreads) 
        : strand(io_context), 
          work_guard(boost::asio::make_work_guard(io_context)) // 保持 io_context 运行
    {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back(&ThreadPool::worker, this);
        }
    }

    ~ThreadPool() {
        stop();  // 退出线程池
    }


private:
    boost::asio::io_context io_context;
    boost::asio::io_context::strand strand;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard;
    std::vector<std::thread> workers;

    static void worker(ThreadPool* pool) {
        pool->io_context.run();
    }
};