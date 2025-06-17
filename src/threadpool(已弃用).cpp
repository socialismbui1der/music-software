#include"threadpool(已弃用).h"
ThreadPool::ThreadPool(size_t numThreads) : stopFlag(false) {
    // 创建指定数量的线程
    for (size_t i = 0; i < numThreads; ++i) {//这会开启numThreads个线程同时执行worker函数，这些线程会共同访问上面的成员，不要和多进程搞混了。
        workers.emplace_back(&ThreadPool::worker, this);//必须绑定对象，因为函数worker是对象的内容
    }//构造函数直接开启所有的工作线程
}

ThreadPool::~ThreadPool() {
    stop();  // 停止线程池
}

void ThreadPool::addTask(std::function<void(int,int)> task) {//调用此函数后，worker函数才会被唤醒
    {
        std::lock_guard<std::mutex> lock(tasksMutex);  // 保护任务队列，防止可能多个函数共同修改了任务队列
        tasks.push(task);  // 将任务添加到队列
        //tasks.push(std::bind(task, arg1, arg2));  // 使用std::bind传递参数，这个方法需要将tasks容器换成两个参数的函数，下面的move不能move函数，只能用lambda
        /* tasks.push([task, arg1, arg2]() {//这个lambda无参数，但是执行体是有参数的函数，很巧妙，下面的move就能把这个无参数的lambda赋值给task了
            task(arg1, arg2);  // 执行任务并传递参数
        }); */
    }
    condition.notify_one();  // 通知一个等待的线程来处理任务
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(tasksMutex);//保护stopflag
        stopFlag = true;
    }
    condition.notify_all();  // 通知所有线程退出
    for (auto& worker : workers) {
        if (worker.joinable()) {//joinable不会阻塞，这是防止线程已经detach
            worker.join();  // 等待所有线程完成，会阻塞等待
        }
    }
}

void ThreadPool::worker() {
    while (true) {//无限循环执行任务，执行完一个之后这个线程又可以用了
        std::function<void(int,int)> task;
        {
            std::unique_lock<std::mutex> lock(tasksMutex);//加锁
            condition.wait(lock, [this] { return stopFlag || !tasks.empty(); });
            //如果工作队列有新的工作进入，或者stop设置为1，就要工作了
            if (stopFlag && tasks.empty()) {
                return;  // 如果停止标志已设置且任务队列为空，线程直接退出，不再进行工作
            }
            
            task = std::move(tasks.front());  // 获取队列中的任务
            tasks.pop();  // 从队列中移除任务
        }

        task(0,0);  // 执行任务，只有抢到响应的线程会执行，其他的线程仍然在等待(这里的0，0没有意义)
    }
}
