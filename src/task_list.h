#pragma once
#include <netinet/in.h>

class syn;
class signup_log;
class task_list{
public:
    task_list(){}
    ~task_list();
    void accept_request(int,int);
    void init(int);
private:
    sockaddr_in m_address;//目前这个没啥用
    signup_log* signup_log_;
    syn* syn_;
    int m_sockfd;
};