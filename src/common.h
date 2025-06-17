#pragma once

/*待解决：
1.注册中断信号,注意要加入线程池的关闭与回收
2.日志系统
4.处理一下头文件包含混乱的问题
5.完善用户系统+数据库
6.歌曲信息也放到数据库
7.http请求的响应需要修改，不用一开始就搞一个线程，我已经使用了另一个端口8002监听这个请求，可以使用条件变量+线程池
8.完善请求歌曲的socket（端口号为8002）创建
*/
namespace//匿名命名空间的作用域在整个 common.h 头文件内，如果 enum req_type 和 enum res_type 里面都定义了 end，那么编译器会认为它们冲突。
{
    enum  req_type{//改进：这里最好用enum class，这样最后的end不会冲突，因为它们有各自的定义域
        sighup,
        login,
        get_music,
        download_music,
        need_data,
        update_songs,
        get_last_update,
        update_new_musiclist,
        update_del_musiclist,
        update_musiclist_song,
        update_insert_songs,
        update_del_songs,
        update_rename_musiclist,
        get_song_comments,
        update_song_comments,
        req_end
    };

    enum res_type{
        sighup_fail,
        sighup_success,
        login_fail,
        login_success,
        get_music_fail,
        get_music_success,
        download_music_fail,
        download_music_success,
        update_music,
        send_songs,
        send_song_comments,
        res_end
    };
};

struct msg_def{
    int msg_len;
    int msg_type;
};

struct request_msg
{
    msg_def header;
    char msg[1024];
};

struct response_msg
{
    msg_def header;
    char msg[1024];
};

