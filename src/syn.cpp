#include"syn.h"
#include<jsoncpp/json/json.h>
#include"easylogging.h"
#include <mysql/mysql.h>
#include"sql_connection_pool.h"
#include"common.h"
#include"util.h"

// void syn::update_song(){
//     MYSQL* conn=nullptr;
//     connectionRALL cr(&conn,connection_pool::getinstance());
//     MYSQL_STMT *stmt;
//     string query="select user_id from users where username=?";

//     stmt=mysql_stmt_init(conn);
//     if (!stmt) {
//         util::sql_shutdown(m_sockfd);
//         LOG(DEBUG)<<"stmt wrong"<<endl;
//         return;
//     }
//     if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
//         mysql_stmt_close(stmt);
//         util::sql_shutdown(m_sockfd);
//         LOG(DEBUG)<<"stmt wrong"<<endl;
//         return;
//     }
//     MYSQL_BIND bind[1];
//     memset(bind,0,sizeof(bind));
//     std::vector<char> username_buffer(username.begin(), username.end());
//     username_buffer.push_back('\0');  // 确保结尾有 '\0'
//     //bind[0].buffer_type = MYSQL_TYPE_VARCHAR;//MYSQL_TYPE_VARCHAR 只能用于 CREATE TABLE 语句，不能用于 mysql_stmt_bind_result()。

//     bind[0].buffer_type = MYSQL_TYPE_VAR_STRING;
//     bind[0].buffer = username_buffer.data();//chat喊我改的，感觉多此一举，他的理由是——MySQL 在 mysql_stmt_execute() 期间可能会访问 bind[0].buffer 的数据，而 std::string 可能会在 execute() 期间发生改变，导致指针失效。
//     bind[0].buffer_length = username.size();//std::string::size() 只计算字符串的长度，不包括 \0 终止符，而某些 MySQL 版本（尤其是旧版本）可能要求 buffer_length 包含 \0 结尾。

//     mysql_stmt_bind_param(stmt,bind);
//     if (mysql_stmt_execute(stmt)) {
//         mysql_stmt_close(stmt);
//         util::sql_shutdown(m_sockfd);
//         LOG(DEBUG)<<"stmt wrong"<<endl;
//         return;
//     }
//     MYSQL_BIND result[1];
//     memset(result,0,sizeof(result));
//     int userid;
//     result[0].buffer=(void*)&userid;
//     result[0].buffer_type=MYSQL_TYPE_LONG;
//     mysql_stmt_bind_result(stmt,result);
//     // 获取数据
//     mysql_stmt_store_result(stmt);
//     if (mysql_stmt_fetch(stmt) != 0) {
//         mysql_stmt_close(stmt);
//         util::sql_shutdown(m_sockfd);
//         LOG(DEBUG)<<"stmt wrong"<<endl;
//         return;
//     }
//     // 关闭 `stmt`
//     user_id=userid;
//     mysql_stmt_close(stmt);//必须关闭再重新配置

//     query="select latest_time from update_users where user_id=?";
//     stmt = mysql_stmt_init(conn);//必须重新配置stmt
//     if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
//         mysql_stmt_close(stmt);
//         util::sql_shutdown(m_sockfd);
//         LOG(DEBUG)<<"stmt wrong"<<endl;
//         return;
//     }
//     memset(bind,0,sizeof(bind));
//     bind[0].buffer=(void*)&user_id;
//     //bind[0].buffer_type=MYSQL_TYPE_INT24;//int类型绑定的是MYSQL_TYPE_LONG，现在这个是3字节的
//     bind[0].buffer_type=MYSQL_TYPE_LONG;
//     bind[0].buffer_length=sizeof(user_id);
//     mysql_stmt_bind_param(stmt,bind);
//     if (mysql_stmt_execute(stmt)) {
//         LOG(DEBUG) << "stmt execution failed: " << mysql_stmt_error(stmt);
//         mysql_stmt_close(stmt);
//         util::sql_shutdown(m_sockfd);
//         LOG(DEBUG)<<"stmt wrong"<<endl;
//         return;
//     }
//     memset(result,0,sizeof(result));
//     MYSQL_TIME update_at;
//     memset(&update_at,0,sizeof(update_at));
//     result[0].buffer=(void*)&update_at;
//     result[0].buffer_type=MYSQL_TYPE_DATETIME;
//     mysql_stmt_bind_result(stmt,result);
//     // 获取数据
//     mysql_stmt_store_result(stmt);
//     bool has_result = (mysql_stmt_fetch(stmt) == 0);
//     mysql_stmt_close(stmt);
//     if(has_result){//说明用户之前登陆过，可以用update_at
//         query="select song_id,name,artist,during from songs where update_at>?;";
//         stmt = mysql_stmt_init(conn);//必须重新配置stmt
//         if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
//             mysql_stmt_close(stmt);
//             util::sql_shutdown(m_sockfd);
//             LOG(DEBUG)<<"stmt wrong"<<endl;
//             return;
//         }
//         memset(bind,0,sizeof(bind));
//         bind[0].buffer=(void*)&update_at;
//         bind[0].buffer_length=sizeof(update_at);
//         bind[0].buffer_type=MYSQL_TYPE_DATETIME;
//         mysql_stmt_bind_param(stmt,bind);
//         if (mysql_stmt_execute(stmt)) {
//             mysql_stmt_close(stmt);
//             util::sql_shutdown(m_sockfd);
//             LOG(DEBUG)<<"stmt wrong"<<endl;
//             return;
//         }
//         MYSQL_BIND result2[4];
//         memset(result2,0,sizeof(result2));
//         int song_id;
//         char song_name[50];
//         char artist[50];
//         char during[10];
//         unsigned long song_name_length, artist_length,during_length;  // 存储真实数据长度
//         result2[0].buffer=&song_id;
//         result2[0].buffer_length=sizeof(song_id);
//         result2[0].buffer_type=MYSQL_TYPE_LONG;

//         result2[1].buffer=song_name;
//         result2[1].buffer_length=sizeof(song_name);
//         result2[1].length=&song_name_length;
//         result2[1].buffer_type=MYSQL_TYPE_VAR_STRING;

//         result2[2].buffer=artist;
//         result2[2].buffer_length=sizeof(artist);
//         result2[2].length=&artist_length;
//         result2[2].buffer_type=MYSQL_TYPE_VAR_STRING;

//         result2[3].buffer=during;
//         result2[3].buffer_length=sizeof(during);
//         result2[3].length=&during_length;
//         result2[3].buffer_type=MYSQL_TYPE_VAR_STRING;

//         mysql_stmt_bind_result(stmt,result2);
//         while (mysql_stmt_fetch(stmt) == 0) {
//             // 处理字符串长度（防止截断）
//             song_name[song_name_length] = '\0';  
//             artist[artist_length] = '\0';
//             during[during_length]='\0';
//             string row=to_string(song_id)+"#"+song_name+"-"+artist+"@@"+during;
//             //row=utf8_to_gbk(row);
//             util::sendres(m_sockfd,res_type::update_music,row);
//         }
//         util::sendres(m_sockfd,res_type::res_end,"end");
//         mysql_stmt_close(stmt);
//     }
//     else{//用户没登陆过
//         query="select song_id,name,artist from songs;";
//         stmt=mysql_stmt_init(conn);
//         if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
//             LOG(DEBUG) << "stmt preparation failed: " << mysql_stmt_error(stmt);
//             mysql_stmt_close(stmt);
//             util::sql_shutdown(m_sockfd);
//             return;
//         }
        
//         if (mysql_stmt_execute(stmt)) {
//             LOG(DEBUG) << "stmt execution failed: " << mysql_stmt_error(stmt);
//             mysql_stmt_close(stmt);
//             util::sql_shutdown(m_sockfd);
//             return;
//         }
        
//         // 绑定结果字段
//         MYSQL_BIND result[3];  // 因为我们要获取 song_id, name, artist 三个字段
//         memset(result, 0, sizeof(result));
        
//         // 绑定 song_id（INT 类型）
//         int song_id;
//         result[0].buffer = (void*)&song_id;
//         result[0].buffer_type = MYSQL_TYPE_LONG;  // INT 类型
//         result[0].buffer_length = sizeof(song_id);
        
//         // 绑定 name（VARCHAR 类型）
//         char song_name[100];  // 假设歌曲名称最大为 100 字符
//         unsigned long songname_len;
//         result[1].buffer = (void*)song_name;
//         result[1].buffer_type = MYSQL_TYPE_STRING;
//         result[1].buffer_length = sizeof(song_name);
//         result[1].length=&songname_len;
//         // 绑定 artist（VARCHAR 类型）
//         char artist[100];  // 假设艺术家名称最大为 100 字符
//         unsigned long artist_len;
//         result[2].buffer = (void*)artist;
//         result[2].buffer_type = MYSQL_TYPE_STRING;
//         result[2].buffer_length = sizeof(artist);
//         result[2].length=&artist_len;
        
//         // 将结果绑定到 stmt
//         if (mysql_stmt_bind_result(stmt, result)) {
//             LOG(DEBUG) << "Binding result failed: " << mysql_stmt_error(stmt);
//             mysql_stmt_close(stmt);
//             util::sql_shutdown(m_sockfd);
//             return;
//         }
//         // 获取结果
//         while (mysql_stmt_fetch(stmt) == 0) {
//             // 打印每一行的结果
//             song_name[songname_len]='\0';
//             artist[artist_len]='\0';
//             //cout<< "Song ID: " << song_id << ", Name: " << song_name << ", Artist: " << artist;
//             string row=to_string(song_id)+"#"+song_name+"-"+artist;
//             row=util::utf8_to_gbk(row);
//             util::sendres(m_sockfd,res_type::update_music,row);
//         }
//         if (mysql_stmt_fetch(stmt) == MYSQL_NO_DATA) {
//             LOG(DEBUG) << "No more data found.";
//         }
//         //只有英文，没有非ascll码字符，不用转为gbk
//         util::sendres(m_sockfd,res_type::res_end,"end");
//         mysql_stmt_close(stmt);
//     }
// }

// void syn::send_update(){
//     MYSQL *conn = nullptr;
//     connectionRALL cr(&conn, connection_pool::getinstance());

//     string query = "select lastlog from users where user_id=?";
//     MYSQL_STMT *stmt = mysql_stmt_init(conn);

//     if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
//         LOG(DEBUG) << "Prepare failed: " << mysql_stmt_error(stmt) << endl;
//         mysql_stmt_close(stmt);
//         util::sql_shutdown(m_sockfd);
//         return;
//     }

//     MYSQL_BIND bind[1];
//     memset(bind, 0, sizeof(bind));
//     bind[0].buffer = &user_id;
//     bind[0].buffer_length = sizeof(user_id);
//     bind[0].buffer_type = MYSQL_TYPE_LONG;
//     mysql_stmt_bind_param(stmt, bind);
//     if (mysql_stmt_execute(stmt)) {
//         LOG(DEBUG) << "Execution failed: " << mysql_stmt_error(stmt) << endl;
//         mysql_stmt_close(stmt);
//         util::sql_shutdown(m_sockfd);
//         return;
//     }

//     MYSQL_BIND result[1];
//     memset(result, 0, sizeof(result));

//     MYSQL_TIME latest_time_mysql = {0}; // 用于存储 MYSQL_DATETIME 结果，存储的类型必须是MYSQL_TIME而不能是字符串

//     result[0].buffer = &latest_time_mysql;
//     result[0].buffer_length = sizeof(latest_time_mysql);
//     result[0].buffer_type = MYSQL_TYPE_DATETIME;
//     mysql_stmt_bind_result(stmt, result);

//     // 2. 执行 fetch
//     if (mysql_stmt_fetch(stmt) == 0) {
//         // 3. 转换为 string
//         char latest_time_str[100];
//         snprintf(latest_time_str, sizeof(latest_time_str), "%04d-%02d-%02d %02d:%02d:%02d",
//                 latest_time_mysql.year, latest_time_mysql.month, latest_time_mysql.day,
//                 latest_time_mysql.hour, latest_time_mysql.minute, latest_time_mysql.second);

//         string latest_time(latest_time_str); // 转换为字符串
//         util::sendres(m_sockfd,res_type::login_success, latest_time);//这里的res_type::login_success没啥用，主要是发送latest_time
//     } 
//     else {
//         LOG(WARNING) << "同步失败" << '\n';
//     }
//     // Clean up
//     mysql_stmt_close(stmt);
// }

//这个函数需要更改，因为update_users这个表要废掉了
// void syn::update_update() {
//     MYSQL *conn = nullptr;
//     connectionRALL cr(&conn, connection_pool::getinstance());

//     string query = "INSERT INTO update_users (user_id, latest_time) "
//                    "VALUES (?, NOW()) ON DUPLICATE KEY UPDATE latest_time = NOW();";

//     MYSQL_STMT *stmt = mysql_stmt_init(conn);
//     if (!stmt) {
//         std::cerr << "mysql_stmt_init failed" << std::endl;
//         return;
//     }

//     if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
//         std::cerr << "mysql_stmt_prepare failed: " << mysql_stmt_error(stmt) << std::endl;
//         mysql_stmt_close(stmt);
//         return;
//     }

//     // 绑定参数
//     MYSQL_BIND bind[1];
//     memset(bind, 0, sizeof(bind));

//     int uid = user_id; // 假设 syn 类中已经有成员 user_id

//     bind[0].buffer_type = MYSQL_TYPE_LONG;
//     bind[0].buffer = (void*)&uid;
//     bind[0].is_null = 0;
//     bind[0].length = 0;

//     if (mysql_stmt_bind_param(stmt, bind) != 0) {
//         std::cerr << "mysql_stmt_bind_param failed: " << mysql_stmt_error(stmt) << std::endl;
//         mysql_stmt_close(stmt);
//         return;
//     }

//     if (mysql_stmt_execute(stmt) != 0) {
//         std::cerr << "mysql_stmt_execute failed: " << mysql_stmt_error(stmt) << std::endl;
//         mysql_stmt_close(stmt);
//         return;
//     }

//     // 清理
//     mysql_stmt_close(stmt);
// }


void syn::update_musiclists(char*msg){
    MYSQL* conn;
    connectionRALL cr(&conn,connection_pool::getinstance());
    MYSQL_STMT* stmt;
    string query="insert into musiclists(user_id,musiclist_name) values(?,?);";
    string musiclist_name=msg;
    stmt=mysql_stmt_init(conn);
    mysql_stmt_prepare(stmt,query.c_str(),query.length());
    MYSQL_BIND bind[2];
    memset(bind,0,sizeof(bind));
    bind[0].buffer=&user_id;
    bind[0].buffer_type=MYSQL_TYPE_LONG;
    bind[0].buffer_length=4;
    bind[1].buffer=(char*)musiclist_name.data();
    bind[1].buffer_type=MYSQL_TYPE_STRING;
    bind[1].buffer_length=musiclist_name.length();
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        LOG(DEBUG) << "Execution failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        return;
    }
    mysql_stmt_close(stmt);
}

void syn::recv_rename_musiclist(char* msg){
    MYSQL* conn=nullptr;
    connectionRALL cr(&conn,connection_pool::getinstance());
    MYSQL_STMT* stmt;
    stmt=mysql_stmt_init(conn);
    string temp=msg;
    size_t pos=temp.find("##");
    string new_name=temp.substr(0,pos);
    string origin_name=temp.substr(pos+2);
    string query="update musiclists set musiclist_name=? where musiclist_name=? and user_id=?";
    mysql_stmt_prepare(stmt,query.c_str(),query.length());
    MYSQL_BIND bind[3];
    memset(bind,0,sizeof(bind));
    bind[0].buffer=(char*)new_name.data();
    bind[0].buffer_type=MYSQL_TYPE_STRING;
    bind[0].buffer_length=new_name.length();
    bind[1].buffer=(char*)origin_name.data();
    bind[1].buffer_type=MYSQL_TYPE_STRING;
    bind[1].buffer_length=origin_name.length();
    bind[2].buffer=&user_id;
    bind[2].buffer_type=MYSQL_TYPE_LONG;
    bind[2].buffer_length=4;
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<mysql_stmt_error(stmt)<<endl;
        return;
    }
    mysql_stmt_close(stmt);
}

void syn::del_musiclists(char* msg){
    string musiclist_name=msg;
    MYSQL* conn=nullptr;
    connectionRALL cr(&conn,connection_pool::getinstance());
    MYSQL_STMT *stmt;
    string query="delete from musiclists where musiclist_name=? and user_id=?";

    stmt=mysql_stmt_init(conn);
    if (!stmt) {
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG) << "SQL Statement Preparation Error: " << mysql_stmt_error(stmt) << endl;
        LOG(DEBUG) << "MySQL Error: " << mysql_error(conn) << endl;
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    MYSQL_BIND bind[2];
    memset(bind,0,sizeof(bind));
    bind[0].buffer=(char*)musiclist_name.data();
    bind[0].buffer_length=musiclist_name.length();
    bind[0].buffer_type=MYSQL_TYPE_STRING;
    bind[1].buffer=&user_id;
    bind[1].buffer_length=4;
    bind[1].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    mysql_stmt_close(stmt);
}

void syn::update_song_comments(char* msg){
    Json::CharReaderBuilder reader;
    Json::Value parsed;
    std::string errs;
    std::istringstream s(msg);
    bool ok = Json::parseFromStream(reader, s, &parsed, &errs);
    if (!ok) {
        LOG(DEBUG) << "反序列化失败" << '\n';
        return;
    }
    int song_id=parsed["song_id"].asInt();
    int user_id=parsed["user_id"].asInt();
    string content=parsed["content"].asString();
    MYSQL* conn=nullptr;
    connectionRALL cr(&conn,connection_pool::getinstance());
    MYSQL_STMT *stmt;
    string query="insert into comment(song_id,user_id,content) values(?,?,?)";
    stmt=mysql_stmt_init(conn);
    if (!stmt) {
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    MYSQL_BIND bind[3];
    memset(bind,0,sizeof(bind));
    bind[0].buffer=&song_id;
    bind[0].buffer_length=4;
    bind[0].buffer_type=MYSQL_TYPE_LONG;

    bind[1].buffer=&user_id;
    bind[1].buffer_length=4;
    bind[1].buffer_type=MYSQL_TYPE_LONG;

    std::string content_copy = content;  // 确保内容活着
    unsigned long content_len = content_copy.length();
    bind[2].buffer = (void*)content_copy.c_str();
    bind[2].buffer_length = content_copy.length();
    bind[2].length = &content_len;
    bind[2].buffer_type=MYSQL_TYPE_STRING;
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    mysql_stmt_close(stmt);
}

void syn::send_song_comments(char* msg){
    string info=msg;
    size_t pos=info.find("-");
    int song_id=stoi(info.substr(0,pos));
    int offset=stoi(info.substr(pos+1));
    MYSQL* conn=nullptr;
    connectionRALL cr(&conn,connection_pool::getinstance());
    MYSQL_STMT *stmt;
    string query="select comment_id,user_id,content,create_time,like_count from comment where song_id=? limit? offset ?";
    stmt=mysql_stmt_init(conn);
    if (!stmt) {
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    int limit=15;
    MYSQL_BIND bind[3];
    memset(bind,0,sizeof(bind));
    bind[0].buffer=&song_id;
    bind[0].buffer_type=MYSQL_TYPE_LONG;
    bind[0].buffer_length=4;
    bind[1].buffer=&limit;
    bind[1].buffer_length=4;
    bind[1].buffer_type=MYSQL_TYPE_LONG;
    bind[2].buffer=&offset;
    bind[2].buffer_length=4;
    bind[2].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    MYSQL_BIND result[5];
    memset(result,0,sizeof(result));
    int comment_id,user_id,like_count;
    MYSQL_TIME create_time;
    memset(&create_time,0,sizeof(create_time));
    char content[256];
    unsigned long content_len;
    result[0].buffer=&comment_id;
    result[0].buffer_type=MYSQL_TYPE_LONG;

    result[1].buffer=&user_id;
    result[1].buffer_type=MYSQL_TYPE_LONG;

    result[2].buffer=content;
    result[2].buffer_type=MYSQL_TYPE_STRING;
    result[2].length=&content_len;
    result[2].buffer_length=sizeof(content);

    result[3].buffer=(void*)&create_time;
    result[3].buffer_type=MYSQL_TYPE_DATETIME;
    result[4].buffer=&like_count;
    result[4].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_result(stmt,result);
    Json::Value root;
    vector<string> username_vec;
    while(mysql_stmt_fetch(stmt) == 0) {
        content[content_len]='\0';
        Json::Value comment_obj;
        comment_obj["comment_id"] = comment_id;
        comment_obj["song_id"] = song_id;
        comment_obj["user_id"] = user_id;
        comment_obj["content"] = content;
        comment_obj["like_count"] = like_count;
        char time_buf[32];
        snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d:%02d",
                create_time.year, create_time.month, create_time.day,
                create_time.hour, create_time.minute, create_time.second);
        comment_obj["create_time"] = time_buf;

        root.append(comment_obj);  // 收集进数组
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    MYSQL_STMT *stmt_;
    stmt_ = mysql_stmt_init(conn);
    query = "select username from users where user_id=?";
    if (mysql_stmt_prepare(stmt_, query.c_str(), query.length())) {
            LOG(DEBUG) << "stmt prepare error: " << mysql_stmt_error(stmt_) << endl;
            mysql_stmt_close(stmt_);
            util::sql_shutdown(m_sockfd);
            LOG(DEBUG)<<"stmt wrong"<<endl;
            return;
    }
    MYSQL_BIND bind_[1];
    memset(bind_,0,sizeof(bind_));
    bind_[0].buffer_type=MYSQL_TYPE_LONG;
    bind_[0].buffer_length=4;

    MYSQL_BIND result_[1];
    memset(result_,0,sizeof(result_));
    char username[100];
    unsigned long username_len;
    result_[0].buffer=username;
    result_[0].buffer_type=MYSQL_TYPE_STRING;
    result_[0].buffer_length=sizeof(username);
    result_[0].length=&username_len;
    //加入username
    for(Json::Value& item : root){
        int user_id=item["user_id"].asInt();
        bind_[0].buffer=&user_id;
        mysql_stmt_bind_param(stmt_,bind_);
        if (mysql_stmt_execute(stmt_)) {
            mysql_stmt_close(stmt_);
            util::sql_shutdown(m_sockfd);
            LOG(DEBUG)<<"stmt wrong"<<endl;
            return;
        }
        
        mysql_stmt_bind_result(stmt_, result_);
        if (mysql_stmt_fetch(stmt_) == 0) {
            username[(username_len >= sizeof(username)) ? (sizeof(username)-1) : username_len] = '\0';
            item["username"] = username;
        }
    }
    mysql_stmt_free_result(stmt_);
    mysql_stmt_close(stmt_);
    Json::StreamWriterBuilder writer;
    std::string json_str = Json::writeString(writer, root);
    util::sendres(m_sockfd,res_type::send_song_comments,json_str);
}

void syn::insert_musiclist_songs(char* msg){
    string info=msg;
    size_t pos=info.find("##");
    string musiclist_name=info.substr(0,pos);
    int song_id=stoi(info.substr(pos+2));
    MYSQL* conn=nullptr;
    connectionRALL cr(&conn,connection_pool::getinstance());
    MYSQL_STMT *stmt;
    //先查询musiclist_id
    string query="select musiclist_id from musiclists where musiclist_name=? and user_id=? ";
    stmt=mysql_stmt_init(conn);
    if (!stmt) {
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    MYSQL_BIND bind[2];
    memset(bind,0,sizeof(bind));
    bind[0].buffer=(char*)musiclist_name.data();
    bind[0].buffer_length=musiclist_name.length();
    bind[0].buffer_type=MYSQL_TYPE_STRING;
    bind[1].buffer=&user_id;
    bind[1].buffer_length=4;
    bind[1].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    MYSQL_BIND result[1];
    memset(result,0,sizeof(result));
    int musiclist_id;
    result[0].buffer=&musiclist_id;
    result[0].buffer_length=4;
    result[0].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_result(stmt,result);
    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    mysql_stmt_close(stmt);

    //插入数据
    stmt=mysql_stmt_init(conn);
    if (!stmt) {
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    query="insert into musiclists_songs(musiclist_id,song_id) values(?,?)";
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        LOG(DEBUG) << "SQL Statement Preparation Error: " << mysql_stmt_error(stmt) << endl;
        LOG(DEBUG) << "MySQL Error: " << mysql_error(conn) << endl;
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    memset(bind,0,sizeof(bind));
    bind[0].buffer=&musiclist_id;
    bind[0].buffer_length=4;
    bind[0].buffer_type=MYSQL_TYPE_LONG;
    bind[1].buffer=&song_id;
    bind[1].buffer_length=4;
    bind[1].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    mysql_stmt_close(stmt);
}

void syn::del_musiclist_songs(char* msg){
    string info=msg;
    size_t pos=info.find("##");
    string musiclist_name=info.substr(0,pos);
    int song_id=stoi(info.substr(pos+2));
    MYSQL* conn=nullptr;
    connectionRALL cr(&conn,connection_pool::getinstance());
    MYSQL_STMT *stmt;
    //先查询musiclist_id
    string query="select musiclist_id from musiclists where musiclist_name=? and user_id=? ";
    stmt=mysql_stmt_init(conn);
    if (!stmt) {
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    MYSQL_BIND bind[2];
    memset(bind,0,sizeof(bind));
    bind[0].buffer=(char*)musiclist_name.data();
    bind[0].buffer_length=musiclist_name.length();
    bind[0].buffer_type=MYSQL_TYPE_STRING;
    bind[1].buffer=&user_id;
    bind[1].buffer_length=4;
    bind[1].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    MYSQL_BIND result[1];
    memset(result,0,sizeof(result));
    int musiclist_id;
    result[0].buffer=&musiclist_id;
    result[0].buffer_length=4;
    result[0].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_result(stmt,result);
    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    mysql_stmt_close(stmt);

    //删除数据
    stmt=mysql_stmt_init(conn);
    if (!stmt) {
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    query="delete from musiclists_songs where musiclist_id=? and song_id=?";
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    memset(bind,0,sizeof(bind));
    bind[0].buffer=&musiclist_id;
    bind[0].buffer_length=4;
    bind[0].buffer_type=MYSQL_TYPE_LONG;
    bind[1].buffer=&song_id;
    bind[1].buffer_length=4;
    bind[1].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        util::sql_shutdown(m_sockfd);
        LOG(DEBUG)<<"stmt wrong"<<endl;
        return;
    }
    mysql_stmt_close(stmt);
    LOG(DEBUG)<<"删除完成"<<'\n';
}

// void syn::send_data(){
//     MYSQL *conn=nullptr;
//     connectionRALL cr(&conn,connection_pool::getinstance());
//     MYSQL_STMT* stmt;
//     vector<pair<int,string>> id_to_name;
//     //先获取用户所拥有的歌单
//     string query="select musiclist_name,musiclist_id from musiclists where user_id=?";
//     stmt=mysql_stmt_init(conn);
//     mysql_stmt_prepare(stmt,query.c_str(),query.length());
//     MYSQL_BIND bind[1];
//     memset(bind,0,sizeof(bind));
//     bind[0].buffer=&user_id;
//     bind[0].buffer_type=MYSQL_TYPE_LONG;
//     bind[0].buffer_length=4;
//     mysql_stmt_bind_param(stmt,bind);
//     if (mysql_stmt_execute(stmt)) {
//         LOG(DEBUG) << "Execution failed: " << mysql_stmt_error(stmt) << endl;
//         mysql_stmt_close(stmt);
//         util::sql_shutdown(m_sockfd);
//         return;
//     }
//     char musiclist_name_[100];
//     unsigned long musiclist_name_len;
//     int musiclist_id;
//     MYSQL_BIND result[2];
//     memset(result,0,sizeof(result));
//     result[0].buffer=musiclist_name_;
//     result[0].buffer_type=MYSQL_TYPE_STRING;
//     result[0].buffer_length=sizeof(musiclist_name_);
//     result[0].length=&musiclist_name_len;
//     result[1].buffer=&musiclist_id;
//     result[1].buffer_type=MYSQL_TYPE_LONG;
//     mysql_stmt_bind_result(stmt, result);
//     while (mysql_stmt_fetch(stmt) == 0) {
//         musiclist_name_[musiclist_name_len]='\0';
//         string musiclist_name=musiclist_name_;
//         id_to_name.emplace_back(make_pair(musiclist_id,musiclist_name));
//     }
//     mysql_stmt_close(stmt);

//     //再发送歌单对应的歌曲
//     for(auto _pair : id_to_name){
//         stmt=mysql_stmt_init(conn);
//         memset(bind,0,sizeof(bind));
//         query="select song_id from musiclists_songs where musiclist_id=?";
//         mysql_stmt_prepare(stmt,query.c_str(),query.length());
//         bind[0].buffer=&(_pair.first);
//         bind[0].buffer_length=4;
//         bind[0].buffer_type=MYSQL_TYPE_LONG;
//         mysql_stmt_bind_param(stmt,bind);
//         if (mysql_stmt_execute(stmt)) {
//             LOG(DEBUG) << "Execution failed: " << mysql_stmt_error(stmt) << endl;
//             mysql_stmt_close(stmt);
//             util::sql_shutdown(m_sockfd);
//             return;
//         }
//         MYSQL_BIND result1[1];
//         memset(result1,0,sizeof(result1));
//         int song_id;
//         result[0].buffer=&song_id;
//         result[0].buffer_type=MYSQL_TYPE_LONG;
//         while (mysql_stmt_fetch(stmt) == 0) {
//             string total_info=_pair.second+"##"+to_string(song_id);//发送歌单名+##+歌曲id——歌曲名的解析放到前端
//             util::sendres(m_sockfd,res_type::send_songs,total_info);
//         }
//         mysql_stmt_close(stmt);
//     }
//     util::sendres(m_sockfd,res_type::res_end,"none");
// }