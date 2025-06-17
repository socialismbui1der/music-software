#include"httplib.h"
#include<thread>
#include<atomic>

class download{
public:
    static download& getinstance();
    void start_work();
    void stop();
private:
    download();
    ~download()=default;
    std::thread t;
    std::atomic<bool> stopflag;
    httplib::Server svr;
};