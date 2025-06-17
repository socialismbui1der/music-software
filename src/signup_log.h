#pragma once
#include<string>
class signup_log{
public:
    signup_log(int sock):m_sockfd(sock){}
    void login(char* msg);
    void signup(char* msg);
    int get_userid(std::string&);
    std::string username;
    int user_id;
private:
    int m_sockfd;
};