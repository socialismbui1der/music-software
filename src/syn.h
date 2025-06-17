#pragma once
#include<string>
class syn{
public:
    syn(int sock):m_sockfd(sock){}
    void update_song();//更新前端的songs
    void send_update();//发送上次同步的时间
    void update_update();//更新update_user表的更新时间
    void update_musiclists(char*msg);//更新数据库的歌单
    void recv_rename_musiclist(char* msg);//用户歌单重命名
    void del_musiclists(char* msg);//删除用户歌单
    void insert_musiclist_songs(char* msg);//用户歌单添加歌曲
    void del_musiclist_songs(char* msg);//用户歌单删除歌曲
    void send_data();//异地登陆发送数据（未测试过）
    void send_song_comments(char* msg);
    void update_song_comments(char *msg);
    
    int user_id;
    std::string username;
private:
    int m_sockfd;
};