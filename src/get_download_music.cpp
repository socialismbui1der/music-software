#include"get_download_music.h"
#include <vector>
#include <fstream>
#include"easylogging.h"
download::download():stopflag(0){}

download& download::getinstance(){
    static download ins;
    return ins;
}

void download::start_work(){
    svr.new_task_queue = [] {
        return new httplib::ThreadPool(8); // 最多 8 个并发请求
    };
    t=std::thread([this](){
        svr.Get("/download", [](const httplib::Request& req, httplib::Response& res) {
            LOG(DEBUG)<<"用户请求下载"<<std::endl;
            std::string name = req.get_param_value("name");
        
            // 安全过滤
            if (name.find("..") != std::string::npos || name.find('/') != std::string::npos) {
                res.status = 400;
                res.set_content("Invalid file name", "text/plain");
                return;
            }
        
            std::string filepath = "/home/amai/music/" + name;
            std::ifstream file(filepath, std::ios::binary);
        
            if (!file) {
                res.status = 404;
                res.set_content("File not found", "text/plain");
                return;
            }
        
            // 获取文件大小
            file.seekg(0, std::ios::end);
            size_t filesize = file.tellg();
            file.close();// 要关上，因为后面 provider 中再打开一次
        
            res.set_content_provider(//set_content_provider(...) 是 httplib 提供的“分块内容发送”接口。它不会把整个内容加载进内存，而是 每次请求读取固定长度的一小段数据。
                filesize,  // 文件总大小，告知客户端 Content-Length
                "audio/mpeg", // MIME 类型，告诉浏览器是 mp3 音频
                [filepath](size_t offset, size_t length, httplib::DataSink &sink) {//设置数据读取的 lambda 回调函数
                    std::ifstream file(filepath, std::ios::binary);//必须在里面重新打开文件,因为file是只可move不可拷贝的，导致lambada不能捕获file
                    if (!file) return false;
                    //sink是数据发送器
                    //这里的offset和length是由httplib框架自动管理的，很方便
                    file.seekg(offset);
                    std::vector<char> buffer(length);
                    file.read(buffer.data(), length);
                    auto read_bytes = file.gcount();
            
                    sink.write(buffer.data(), read_bytes);
                    return true;
                }
            );
            res.set_header("Content-Disposition", "attachment; filename=\"" + name + "\"");//设置 HTTP 响应头，提示浏览器以“附件”方式下载：
            //attachment; filename="骂醒我.mp3" 表示浏览器看到这个响应后要弹出保存文件对话框，而不是直接播放；
            //如果你改成 inline; filename=... 则浏览器会尝试直接播放音频。
        });
        
        svr.Get("/music", [](const httplib::Request& req, httplib::Response& res) {
            LOG(DEBUG)<<"用户请求播放"<<std::endl;
            std::string name = req.get_param_value("name");
            std::string filepath = "/home/amai/music/" + name;
            LOG(DEBUG)<<"FILEPATH:"<<filepath<<'\n';
            // 获取文件大小
            std::ifstream file(filepath, std::ios::binary | std::ios::ate);
            if (!file) {
                res.status = 404;
                res.set_content("File not found", "text/plain");
                return;
            }
            auto filesize = file.tellg();
            file.close(); // 要关上，因为后面 provider 中再打开一次
    
            //这样的话音乐文件一次性加载，很占内存
            //res.set_content(reinterpret_cast<const char*>(mp3_data.data()), mp3_data.size(), "audio/mpeg");这
    
            //这种方式音乐文件分步加载，并且客户端中断后立即终止传输（节省服务端带宽）
            res.set_content_provider(
                static_cast<size_t>(filesize),
                "audio/mpeg",
                [filepath](size_t offset, size_t length, httplib::DataSink& sink) {
                    std::ifstream file(filepath, std::ios::binary);
                    if (!file) return false;
            
                    file.seekg(offset);
                    std::vector<char> buffer(length);
                    file.read(buffer.data(), length);
                    auto read_bytes = file.gcount();
                    
                    if (!sink.is_writable()) {
                        std::cout << "Client disconnected while streaming." << std::endl;
                        return false;
                    }
            
                    sink.write(buffer.data(), read_bytes);
                    return true;
                }
            );        
        });
        LOG(DEBUG) << "Server is running at http://172.18.81.202:8081" << std::endl;
        svr.listen("172.18.81.202", 8081);//这一行会阻塞
        LOG(DEBUG)<< "Server stopped.\n";
        stopflag=1;
    });
}

void download::stop(){
    if (!stopflag) {
        svr.stop(); // 通知服务器结束监听
    }
    if (t.joinable()) {
        t.join(); // 回收线程
    }
}
