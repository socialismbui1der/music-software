#include"http_request.h"
#include <vector>
#include <fstream>
#include"easylogging.h"
#include"sql_connection_pool.h"
#include<jsoncpp/json/json.h>

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

        svr.Get("/search", [](const httplib::Request& req, httplib::Response& res) {
                    // 1) 参数校验
            if (!req.has_param("type")) { 
                res.status = 400; 
                res.set_content(R"({"error":"missing type"})", "application/json; charset=utf-8"); 
                LOG(DEBUG)<<"no type"<<'\n';
                return; 
            }
            std::string type = req.get_param_value("type");

            // limit 缺省与边界
            int lim = 10;
            if (req.has_param("limit")) {
                try {
                    lim = std::stoi(req.get_param_value("limit"));
                } catch (...) { /* 保持默认 */ }
            }
            if (lim <= 0) lim = 10;
            if (lim > 50) lim = 50; // 给个上限，避免滥刷

            MYSQL* conn = nullptr;
            connectionRALL cr(&conn, connection_pool::getinstance()); // 假设能拿到已连接的 conn
            Json::Value root;

            if (type == "songname") {
                if (!req.has_param("songname")) {
                    res.status = 400;
                    res.set_content(R"({"error":"missing songname"})", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"no songname"<<'\n';
                    return;
                }
                const std::string key = req.get_param_value("songname");

                // 2) SQL：WHERE 模糊；CASE：精确>前缀>其它（都在 WHERE 的集合内）
                const std::string query =
                    "SELECT DISTINCT song_name FROM songs "
                    "WHERE song_name LIKE ? COLLATE utf8mb4_general_ci "
                    "ORDER BY CASE "
                    "  WHEN song_name LIKE ? THEN 0 "      // 精确
                    "  WHEN song_name LIKE ? THEN 1 "      // 前缀
                    "  ELSE 2 "
                    "END "
                    "LIMIT ?";

                MYSQL_STMT* stmt = mysql_stmt_init(conn);
                if (!stmt) {
                    res.status = 500;
                    res.set_content(R"({"error":"stmt_init_failed"})", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"stmt fail"<<'\n';
                    return;
                }
                if (mysql_stmt_prepare(stmt, query.c_str(), static_cast<unsigned long>(query.size())) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"stmt_prepare_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<err<<'\n';
                    return;
                }

                // 3) 绑定参数：分别是 %key%、key、key% 和 limit(int)
                std::string like_where  = "%" + key + "%";
                std::string like_exact  = key;         // 精确（用 LIKE 但不加通配符）
                std::string like_prefix = key + "%";   // 前缀

                unsigned long len_where  = static_cast<unsigned long>(like_where.size());
                unsigned long len_exact  = static_cast<unsigned long>(like_exact.size());
                unsigned long len_prefix = static_cast<unsigned long>(like_prefix.size());

                MYSQL_BIND bind[4]; memset(bind, 0, sizeof(bind));

                bind[0].buffer_type   = MYSQL_TYPE_STRING;
                bind[0].buffer        = (void*)like_where.c_str();
                bind[0].buffer_length = len_where;
                bind[0].length        = &len_where;

                bind[1].buffer_type   = MYSQL_TYPE_STRING;
                bind[1].buffer        = (void*)like_exact.c_str();
                bind[1].buffer_length = len_exact;
                bind[1].length        = &len_exact;

                bind[2].buffer_type   = MYSQL_TYPE_STRING;
                bind[2].buffer        = (void*)like_prefix.c_str();
                bind[2].buffer_length = len_prefix;
                bind[2].length        = &len_prefix;

                // LIMIT 用整型
                unsigned int lim_u = static_cast<unsigned int>(lim);
                bind[3].buffer_type = MYSQL_TYPE_LONG;     // 或 MYSQL_TYPE_LONG | is_unsigned=true
                bind[3].is_unsigned = true;
                bind[3].buffer      = (void*)&lim_u;

                if (mysql_stmt_bind_param(stmt, bind) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"bind_param_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<err<<'\n';
                    return;
                }

                if (mysql_stmt_execute(stmt) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"stmt_execute_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<err<<'\n';
                    return;
                }

                // 4) 绑定结果
                MYSQL_BIND result[1]; memset(result, 0, sizeof(result));
                char result_songname[256];
                unsigned long songname_len = 0;
                bool is_null = 0; 

                result[0].buffer_type   = MYSQL_TYPE_STRING;
                result[0].buffer        = result_songname;
                result[0].buffer_length = sizeof(result_songname);
                result[0].length        = &songname_len;
                result[0].is_null       = &is_null;

                if (mysql_stmt_bind_result(stmt, result) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"bind_result_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<err<<'\n';
                    return;
                }

                // 可选：把结果缓存到客户端（一次性取完）
                mysql_stmt_store_result(stmt);

                Json::Value songs(Json::arrayValue);
                while (true) {
                    int rf = mysql_stmt_fetch(stmt);
                    if (rf == 0) {
                        if (!is_null) {
                            // 安全截断并补 '\0'
                            size_t n = (songname_len >= sizeof(result_songname)) ? sizeof(result_songname) - 1 : songname_len;
                            result_songname[n] = '\0';
                            songs.append(result_songname);
                        }
                    } else if (rf == MYSQL_NO_DATA) {
                        break;
                    } else {
                        std::string err = mysql_stmt_error(stmt);
                        mysql_stmt_free_result(stmt);
                        mysql_stmt_close(stmt);
                        res.status = 500;
                        res.set_content(std::string(R"({"error":"stmt_fetch_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                        LOG(DEBUG)<<err<<'\n';
                        return;
                    }
                }

                mysql_stmt_free_result(stmt);
                mysql_stmt_close(stmt);

                // 5) 组织 JSON 并回写
                root["results"] = songs;
                Json::StreamWriterBuilder w; 
                w["emitUTF8"] = true; //生成的 JSON 字符串中直接输出 UTF-8 字符（不转成 \uXXXX 形式的转义）。
                const std::string body = Json::writeString(w, root);

                res.status = 200;
                res.set_content(body, "application/json; charset=utf-8");
                return;
            }

            else if(type == "album"){
                if (!req.has_param("album")) {
                    res.status = 400;
                    res.set_content(R"({"error":"missing album"})", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"no album"<<'\n';
                    return;
                }
                const std::string key = req.get_param_value("album");

                // 2) SQL：WHERE 模糊；CASE：精确>前缀>其它（都在 WHERE 的集合内）
                const std::string query =
                    "SELECT DISTINCT song_name FROM songs "
                    "WHERE song_name LIKE ? COLLATE utf8mb4_general_ci "
                    "ORDER BY CASE "
                    "  WHEN song_name LIKE ? THEN 0 "      // 精确
                    "  WHEN song_name LIKE ? THEN 1 "      // 前缀
                    "  ELSE 2 "
                    "END "
                    "LIMIT ?";

                MYSQL_STMT* stmt = mysql_stmt_init(conn);
                if (!stmt) {
                    res.status = 500;
                    res.set_content(R"({"error":"stmt_init_failed"})", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"stmt fail"<<'\n';
                    return;
                }
                if (mysql_stmt_prepare(stmt, query.c_str(), static_cast<unsigned long>(query.size())) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"stmt_prepare_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"stmt_prepare_failed"<<'\n';
                    return;
                }

                // 3) 绑定参数：分别是 %key%、key、key% 和 limit(int)
                std::string like_where  = "%" + key + "%";
                std::string like_exact  = key;         // 精确（用 LIKE 但不加通配符）
                std::string like_prefix = key + "%";   // 前缀

                unsigned long len_where  = static_cast<unsigned long>(like_where.size());
                unsigned long len_exact  = static_cast<unsigned long>(like_exact.size());
                unsigned long len_prefix = static_cast<unsigned long>(like_prefix.size());

                MYSQL_BIND bind[4]; memset(bind, 0, sizeof(bind));

                bind[0].buffer_type   = MYSQL_TYPE_STRING;
                bind[0].buffer        = (void*)like_where.c_str();
                bind[0].buffer_length = len_where;
                bind[0].length        = &len_where;

                bind[1].buffer_type   = MYSQL_TYPE_STRING;
                bind[1].buffer        = (void*)like_exact.c_str();
                bind[1].buffer_length = len_exact;
                bind[1].length        = &len_exact;

                bind[2].buffer_type   = MYSQL_TYPE_STRING;
                bind[2].buffer        = (void*)like_prefix.c_str();
                bind[2].buffer_length = len_prefix;
                bind[2].length        = &len_prefix;

                // LIMIT 用整型
                unsigned int lim_u = static_cast<unsigned int>(lim);
                bind[3].buffer_type = MYSQL_TYPE_LONG;     // 或 MYSQL_TYPE_LONG | is_unsigned=true
                bind[3].is_unsigned = true;
                bind[3].buffer      = (void*)&lim_u;

                if (mysql_stmt_bind_param(stmt, bind) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"bind_param_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"bind_param_failed"<<'\n';
                    return;
                }

                if (mysql_stmt_execute(stmt) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"stmt_execute_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    return;
                }

                // 4) 绑定结果
                MYSQL_BIND result[1]; memset(result, 0, sizeof(result));
                char result_album[256];
                unsigned long album_len = 0;
                bool is_null = 0; 

                result[0].buffer_type   = MYSQL_TYPE_STRING;
                result[0].buffer        = result_album;
                result[0].buffer_length = sizeof(result_album);
                result[0].length        = &album_len;
                result[0].is_null       = &is_null;

                if (mysql_stmt_bind_result(stmt, result) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"bind_result_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"bind_result_failed"<<'\n';
                    return;
                }

                // 可选：把结果缓存到客户端（一次性取完）
                mysql_stmt_store_result(stmt);

                Json::Value albums(Json::arrayValue);
                while (true) {
                    int rf = mysql_stmt_fetch(stmt);
                    if (rf == 0) {
                        if (!is_null) {
                            // 安全截断并补 '\0'
                            size_t n = (album_len >= sizeof(result_album)) ? sizeof(result_album) - 1 : album_len;
                            result_album[n] = '\0';
                            albums.append(result_album);
                        }
                    } else if (rf == MYSQL_NO_DATA) {
                        break;
                    } else {
                        std::string err = mysql_stmt_error(stmt);
                        mysql_stmt_free_result(stmt);
                        mysql_stmt_close(stmt);
                        res.status = 500;
                        res.set_content(std::string(R"({"error":"stmt_fetch_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                        LOG(DEBUG)<<"stmt_fetch_failed"<<'\n';
                        return;
                    }
                }

                mysql_stmt_free_result(stmt);
                mysql_stmt_close(stmt);

                // 5) 组织 JSON 并回写
                root["results"] = albums;
                Json::StreamWriterBuilder w; 
                w["emitUTF8"] = true; //生成的 JSON 字符串中直接输出 UTF-8 字符（不转成 \uXXXX 形式的转义）。
                const std::string body = Json::writeString(w, root);

                res.status = 200;
                res.set_content(body, "application/json; charset=utf-8");
                return;
            }

            else if(type == "artist"){
                if (!req.has_param("artist")) {
                    res.status = 400;
                    res.set_content(R"({"error":"missing artist"})", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"no artist"<<'\n';
                    return;
                }
                const std::string key = req.get_param_value("artist");

                // 2) SQL：WHERE 模糊；CASE：精确>前缀>其它（都在 WHERE 的集合内）
                const std::string query =
                    "SELECT DISTINCT song_name FROM songs "
                    "WHERE song_name LIKE ? COLLATE utf8mb4_general_ci "
                    "ORDER BY CASE "
                    "  WHEN song_name LIKE ? THEN 0 "      // 精确
                    "  WHEN song_name LIKE ? THEN 1 "      // 前缀
                    "  ELSE 2 "
                    "END "
                    "LIMIT ?";

                MYSQL_STMT* stmt = mysql_stmt_init(conn);
                if (!stmt) {
                    res.status = 500;
                    res.set_content(R"({"error":"stmt_init_failed"})", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"stmt fail"<<'\n';
                    return;
                }
                if (mysql_stmt_prepare(stmt, query.c_str(), static_cast<unsigned long>(query.size())) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"stmt_prepare_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"stmt_prepare_failed"<<'\n';
                    return;
                }

                // 3) 绑定参数：分别是 %key%、key、key% 和 limit(int)
                std::string like_where  = "%" + key + "%";
                std::string like_exact  = key;         // 精确（用 LIKE 但不加通配符）
                std::string like_prefix = key + "%";   // 前缀

                unsigned long len_where  = static_cast<unsigned long>(like_where.size());
                unsigned long len_exact  = static_cast<unsigned long>(like_exact.size());
                unsigned long len_prefix = static_cast<unsigned long>(like_prefix.size());

                MYSQL_BIND bind[4]; memset(bind, 0, sizeof(bind));

                bind[0].buffer_type   = MYSQL_TYPE_STRING;
                bind[0].buffer        = (void*)like_where.c_str();
                bind[0].buffer_length = len_where;
                bind[0].length        = &len_where;

                bind[1].buffer_type   = MYSQL_TYPE_STRING;
                bind[1].buffer        = (void*)like_exact.c_str();
                bind[1].buffer_length = len_exact;
                bind[1].length        = &len_exact;

                bind[2].buffer_type   = MYSQL_TYPE_STRING;
                bind[2].buffer        = (void*)like_prefix.c_str();
                bind[2].buffer_length = len_prefix;
                bind[2].length        = &len_prefix;

                // LIMIT 用整型
                unsigned int lim_u = static_cast<unsigned int>(lim);
                bind[3].buffer_type = MYSQL_TYPE_LONG;     // 或 MYSQL_TYPE_LONG | is_unsigned=true
                bind[3].is_unsigned = true;
                bind[3].buffer      = (void*)&lim_u;

                if (mysql_stmt_bind_param(stmt, bind) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"bind_param_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"bind_param_failed"<<'\n';
                    return;
                }

                if (mysql_stmt_execute(stmt) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"stmt_execute_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    return;
                }

                // 4) 绑定结果
                MYSQL_BIND result[1]; memset(result, 0, sizeof(result));
                char result_artist[256];
                unsigned long artist_len = 0;
                bool is_null = 0; 

                result[0].buffer_type   = MYSQL_TYPE_STRING;
                result[0].buffer        = result_artist;
                result[0].buffer_length = sizeof(result_artist);
                result[0].length        = &artist_len;
                result[0].is_null       = &is_null;

                if (mysql_stmt_bind_result(stmt, result) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"bind_result_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"bind_result_failed"<<'\n';
                    return;
                }

                // 可选：把结果缓存到客户端（一次性取完）
                mysql_stmt_store_result(stmt);

                Json::Value artists(Json::arrayValue);
                while (true) {
                    int rf = mysql_stmt_fetch(stmt);
                    if (rf == 0) {
                        if (!is_null) {
                            // 安全截断并补 '\0'
                            size_t n = (artist_len >= sizeof(result_artist)) ? sizeof(result_artist) - 1 : artist_len;
                            result_artist[n] = '\0';
                            artists.append(result_artist);
                        }
                    } else if (rf == MYSQL_NO_DATA) {
                        break;
                    } else {
                        std::string err = mysql_stmt_error(stmt);
                        mysql_stmt_free_result(stmt);
                        mysql_stmt_close(stmt);
                        res.status = 500;
                        res.set_content(std::string(R"({"error":"stmt_fetch_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                        LOG(DEBUG)<<"stmt_fetch_failed"<<'\n';
                        return;
                    }
                }

                mysql_stmt_free_result(stmt);
                mysql_stmt_close(stmt);

                // 5) 组织 JSON 并回写
                root["results"] = artists;
                Json::StreamWriterBuilder w; 
                w["emitUTF8"] = true; //生成的 JSON 字符串中直接输出 UTF-8 字符（不转成 \uXXXX 形式的转义）。
                const std::string body = Json::writeString(w, root);

                res.status = 200;
                res.set_content(body, "application/json; charset=utf-8");
                return;
            }

            else if(type == "playlist"){
                if (!req.has_param("playlist")) {
                    res.status = 400;
                    res.set_content(R"({"error":"missing playlist"})", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"no playlist"<<'\n';
                    return;
                }
                const std::string key = req.get_param_value("playlist");

                // 2) SQL：WHERE 模糊；CASE：精确>前缀>其它（都在 WHERE 的集合内）
                const std::string query =
                    "SELECT DISTINCT song_name FROM songs "
                    "WHERE song_name LIKE ? COLLATE utf8mb4_general_ci "
                    "ORDER BY CASE "
                    "  WHEN song_name LIKE ? THEN 0 "      // 精确
                    "  WHEN song_name LIKE ? THEN 1 "      // 前缀
                    "  ELSE 2 "
                    "END "
                    "LIMIT ?";

                MYSQL_STMT* stmt = mysql_stmt_init(conn);
                if (!stmt) {
                    res.status = 500;
                    res.set_content(R"({"error":"stmt_init_failed"})", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"stmt fail"<<'\n';
                    return;
                }
                if (mysql_stmt_prepare(stmt, query.c_str(), static_cast<unsigned long>(query.size())) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"stmt_prepare_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"stmt_prepare_failed"<<'\n';
                    return;
                }

                // 3) 绑定参数：分别是 %key%、key、key% 和 limit(int)
                std::string like_where  = "%" + key + "%";
                std::string like_exact  = key;         // 精确（用 LIKE 但不加通配符）
                std::string like_prefix = key + "%";   // 前缀

                unsigned long len_where  = static_cast<unsigned long>(like_where.size());
                unsigned long len_exact  = static_cast<unsigned long>(like_exact.size());
                unsigned long len_prefix = static_cast<unsigned long>(like_prefix.size());

                MYSQL_BIND bind[4]; memset(bind, 0, sizeof(bind));

                bind[0].buffer_type   = MYSQL_TYPE_STRING;
                bind[0].buffer        = (void*)like_where.c_str();
                bind[0].buffer_length = len_where;
                bind[0].length        = &len_where;

                bind[1].buffer_type   = MYSQL_TYPE_STRING;
                bind[1].buffer        = (void*)like_exact.c_str();
                bind[1].buffer_length = len_exact;
                bind[1].length        = &len_exact;

                bind[2].buffer_type   = MYSQL_TYPE_STRING;
                bind[2].buffer        = (void*)like_prefix.c_str();
                bind[2].buffer_length = len_prefix;
                bind[2].length        = &len_prefix;

                // LIMIT 用整型
                unsigned int lim_u = static_cast<unsigned int>(lim);
                bind[3].buffer_type = MYSQL_TYPE_LONG;     // 或 MYSQL_TYPE_LONG | is_unsigned=true
                bind[3].is_unsigned = true;
                bind[3].buffer      = (void*)&lim_u;

                if (mysql_stmt_bind_param(stmt, bind) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"bind_param_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"bind_param_failed"<<'\n';
                    return;
                }

                if (mysql_stmt_execute(stmt) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"stmt_execute_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    return;
                }

                // 4) 绑定结果
                MYSQL_BIND result[1]; memset(result, 0, sizeof(result));
                char result_playlist[256];
                unsigned long playlist_len = 0;
                bool is_null = 0; 

                result[0].buffer_type   = MYSQL_TYPE_STRING;
                result[0].buffer        = result_playlist;
                result[0].buffer_length = sizeof(result_playlist);
                result[0].length        = &playlist_len;
                result[0].is_null       = &is_null;

                if (mysql_stmt_bind_result(stmt, result) != 0) {
                    std::string err = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    res.status = 500;
                    res.set_content(std::string(R"({"error":"bind_result_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                    LOG(DEBUG)<<"bind_result_failed"<<'\n';
                    return;
                }

                // 可选：把结果缓存到客户端（一次性取完）
                mysql_stmt_store_result(stmt);

                Json::Value playlists(Json::arrayValue);
                while (true) {
                    int rf = mysql_stmt_fetch(stmt);
                    if (rf == 0) {
                        if (!is_null) {
                            // 安全截断并补 '\0'
                            size_t n = (playlist_len >= sizeof(result_playlist)) ? sizeof(result_playlist) - 1 : playlist_len;
                            result_playlist[n] = '\0';
                            playlists.append(result_playlist);
                        }
                    } else if (rf == MYSQL_NO_DATA) {
                        break;
                    } else {
                        std::string err = mysql_stmt_error(stmt);
                        mysql_stmt_free_result(stmt);
                        mysql_stmt_close(stmt);
                        res.status = 500;
                        res.set_content(std::string(R"({"error":"stmt_fetch_failed","detail":")") + err + "\"}", "application/json; charset=utf-8");
                        LOG(DEBUG)<<"stmt_fetch_failed"<<'\n';
                        return;
                    }
                }

                mysql_stmt_free_result(stmt);
                mysql_stmt_close(stmt);

                // 5) 组织 JSON 并回写
                root["results"] = playlists;
                Json::StreamWriterBuilder w; 
                w["emitUTF8"] = true; //生成的 JSON 字符串中直接输出 UTF-8 字符（不转成 \uXXXX 形式的转义）。
                const std::string body = Json::writeString(w, root);

                res.status = 200;
                res.set_content(body, "application/json; charset=utf-8");
                return;
            }


            // 未知 type
            res.status = 400;
            res.set_content(R"({"error":"unsupported type"})", "application/json; charset=utf-8");
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
