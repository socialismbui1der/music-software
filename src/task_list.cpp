#include"task_list.h"
#include"common.h"
#include"threadpool_strand.hpp"
#include"signup_log.h"
#include"syn.h"
#include"easylogging.h"
#include<string>

void task_list::accept_request(int epoll_fd, int conn) {
    while (true) {
        request_msg req;
        int byte;

        // 读取消息头
        int total_read = 0;
        while (total_read < sizeof(req.header)) {
            byte = recv(m_sockfd, ((char*)&req.header) + total_read, sizeof(req.header) - total_read, 0);
            if (byte == -1) {
                if (errno == EINTR) continue;      // 信号中断，继续读
                if (errno == EAGAIN || errno == EWOULDBLOCK) break; // 非阻塞 socket，没数据了
                perror("recv error");//当客户端发送关闭信号时，如果socket里还有数据未读取，客户端就会转而发送一个RST重置信号，导致异常关闭
                close(conn);
                return;
            } else if (byte == 0) {
                std::cout << "客户端关闭连接" << std::endl;
                close(conn);
                return;
            }
            total_read += byte;
        }

        if (byte == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return; // 非阻塞模式下，当前无数据可读，返回
            } else {
                LOG(DEBUG)<<"recv fail"<<errno<<'\n';
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn, nullptr);
                close(conn);
                return;
            }
        }
        if (byte == 0) {
            std::cout << "客户端关闭连接" << std::endl;
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn, nullptr);
            close(conn);
            return;
        }

        // 解析消息头
        req.header.msg_type = ntohl(req.header.msg_type);
        req.header.msg_len = ntohl(req.header.msg_len);
        int req_type = req.header.msg_type;

        // 读取消息体，循环读取以保证完整性
        int total_received = 0;
        while (total_received < req.header.msg_len) {
            byte = recv(m_sockfd, req.msg + total_received, req.header.msg_len - total_received, 0);

            if (byte == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return; // 没有更多数据可读
                } else {
                    perror("recv fail");
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn, nullptr);
                    close(conn);
                    return;
                }
            }
            if (byte == 0) {
                std::cout << "客户端关闭连接" << std::endl;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn, nullptr);
                close(conn);
                return;
            }
            total_received += byte;
        }

        req.msg[req.header.msg_len] = '\0'; // 确保字符串以 '\0' 结尾

        // 处理请求类型
        switch (req_type) {
            case req_type::login://socket
                //不能在lambda中捕获req.msg，因为req是一个局部变量，而msg又是一个指针，它的生命周期是不确定的。
                ThreadPool::getinstance().addTask([this,msg=std::string(req.msg)](){
                    LOG(DEBUG)<<"登录"<<'\n';
                    signup_log_->login((char*)(msg.c_str()));
                    syn_->user_id=signup_log_->user_id;
                    syn_->username=signup_log_->username;
                });
                break;
            case req_type::sighup://socket
                ThreadPool::getinstance().addTask([this,msg=std::string(req.msg)](){
                    signup_log_->signup((char*)(msg.c_str()));
                });
                break;
            case req_type::update_new_musiclist://grpc
                ThreadPool::getinstance().addTask([this,msg=std::string(req.msg)](){
                    syn_->update_musiclists((char*)msg.c_str());
                });
                break;
            case req_type::update_del_musiclist://grpc
                ThreadPool::getinstance().addTask([this,msg=std::string(req.msg)](){
                    syn_->del_musiclists((char*)msg.c_str());
                });
                break;
            case req_type::update_insert_songs://grpc
                ThreadPool::getinstance().addTask([this,msg=std::string(req.msg)](){
                    syn_->insert_musiclist_songs((char*)msg.c_str());
                });
                break;
            case req_type::update_del_songs://grpc
                ThreadPool::getinstance().addTask([this,msg=std::string(req.msg)](){
                    syn_->del_musiclist_songs((char*)msg.c_str());
                });
                break;
            // case req_type::need_data://这个可以删了
            //     ThreadPool::getinstance().addTask([this](){
            //         syn_->send_data();
            //     });
            //     break;
            // case req_type::update_songs://这个可以删了（更新前端歌曲库的）
            //     ThreadPool::getinstance().addTask([this](){
            //         syn_->update_song();
            //     });
            //     break;
            // case req_type::get_last_update://可以保留（发送用户上一次登录时间的）
            //     ThreadPool::getinstance().addTask([this](){
            //         syn_->send_update();
            //     });
            //     break;
            // case req_type::req_end://可以删除
            //     ThreadPool::getinstance().addTask([this](){
            //         syn_->update_update();
            //     });
            //     break;
            case req_type::update_rename_musiclist://grpc
                ThreadPool::getinstance().addTask([this,msg=std::string(req.msg)](){
                    syn_->recv_rename_musiclist((char*)msg.c_str());
                });
                break;
            case req_type::get_song_comments://http
                LOG(DEBUG)<<"获取评论"<<'\n';
                ThreadPool::getinstance().addTask([this,msg=std::string(req.msg)](){
                    syn_->send_song_comments((char*)msg.c_str());
                });
                break;
            case req_type::update_song_comments://grpc
                LOG(DEBUG)<<"上传评论"<<'\n';
                ThreadPool::getinstance().addTask([this,msg=std::string(req.msg)](){
                    syn_->update_song_comments((char*)msg.c_str());
                });
                break;
            default:
                std::cerr << "未知的请求类型：" << req_type << std::endl;
                break;
        }
    }
}


void task_list::init(int socket){
    syn_=new syn(socket);
    signup_log_=new signup_log(socket);
    m_sockfd=socket;
}

task_list::~task_list(){
    delete syn_;
    delete signup_log_;
}

