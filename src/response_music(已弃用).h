#include"httplib.h"
#include"easylogging.h"
#include <iostream>
#include <fstream>
#include <vector>
#include<thread>
#include<atomic>

class http_server{
public:
    static http_server& getinstance();
    void start_work();
    void stop();
private:
    http_server();
    ~http_server()=default;
    std::thread t;
    std::atomic<bool> stopflag;
    httplib::Server svr;
};