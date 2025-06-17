#pragma once
#include<string>

class util{
public:
    static void sendres(int,int type,const char* msg);
    static void sendres(int,int type,std::string&);
    static std::string utf8_to_gbk(const std::string&);//放到util.h
    static void sql_shutdown(int);//放到util.h
};