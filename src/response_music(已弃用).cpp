#include"response_music.h"

http_server::http_server():stopflag(0){}

http_server& http_server::getinstance(){
    static http_server ins;
    return ins;
}

void http_server::start_work(){
    svr.new_task_queue = [] {
        return new httplib::ThreadPool(8); // 最多 8 个并发请求
    };
    t=std::thread([this](){
        svr.Get("/music", [](const httplib::Request& req, httplib::Response& res) {
            std::string name = req.get_param_value("name");
            std::string filepath = "/home/amai/music/" + name;
    
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
    
        LOG(DEBUG) << "Server is running at http://172.18.81.202:8080" << std::endl;
        svr.listen("172.18.81.202", 8080);//这一行会阻塞
        LOG(DEBUG)<< "Server stopped.\n";
        stopflag=1;
    });
}

void http_server::stop(){
    if (!stopflag) {
        svr.stop(); // 通知服务器结束监听
    }
    if (t.joinable()) {
        t.join(); // 回收线程
    }
}
