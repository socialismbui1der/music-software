#pragma once
#include<unordered_map>
//#include"common.h"
#include <sys/types.h>
#include"task_list.h"
class process{
public:
    process():pid(-1){}
    pid_t pid;
    int pipefd[2];
    ~process(){}
};

template<typename T>
class processpool
{
private:
    processpool(int,int);
    void addfd(int,int);
    //static processpool<T>* instance=nullptr;这样的写法是错的，因为静态成员不能直接初始化，要么在类外初始化，要么把它变成编译时常量
    static processpool<T>* instance;
    int proc_id;//用于表示该进程是子进程还是父进程，并且每个子进程的这个值还不同，可以唯一标识一个子进程
    process* pro_vector;//进程类数组，父进程通过这个进程数组可以和所有子进程通信。
    int pro_num;//子进程数量
    int epoll_fd;//epoll的返回值
    int listenfd;//客户端的监听fd
    //int listenfd2;
    bool m_stop;
public:
    ~processpool();
    void run();
    void run_child();
    void run_parent();
    void setup_sig_pipe();
    int setnonblock(int);
    //void sig_handler(int sig);
    void load_user();
    void addsig(int sig,void(*handler)(int));
    //void addsig(int sig,std::function<void(int)> handler);
    static processpool* create(int,int num=8);
};


