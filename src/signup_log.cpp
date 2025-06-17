#include"signup_log.h"
#include"util.h"
#include <mysql/mysql.h>
#include <iomanip>
#include"common.h"
#include"sql_connection_pool.h"
#include"easylogging.h"

extern unordered_map<string,string> user_map;

int signup_log::get_userid(std::string& username){
    MYSQL* conn=nullptr;
    connectionRALL(&conn,connection_pool::getinstance());
    MYSQL_STMT* stmt=nullptr;
    stmt=mysql_stmt_init(conn); 
    string query="select user_id from users where username=?;";
    if (!stmt || mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        std::string temp="server has some problems,please try later";
        util::sendres(m_sockfd,res_type::sighup_fail,temp);
        LOG(DEBUG)<<"wrong"<<endl;
        return -1;
    }
    MYSQL_BIND bind[1];
    memset(bind,0,sizeof(bind));
    bind[0].buffer=(char*)username.c_str();
    bind[0].buffer_length=username.length();
    bind[0].buffer_type=MYSQL_TYPE_STRING;
    mysql_stmt_bind_param(stmt,bind);
    if (mysql_stmt_execute(stmt)) {
        std::string temp="server has some problems,please try later";
        util::sendres(m_sockfd,res_type::sighup_fail,temp);
        mysql_stmt_close(stmt);
        LOG(DEBUG)<<"wrong"<<endl;
        return -1;
    }
    MYSQL_BIND result[1];
    memset(result,0,sizeof(result));
    int user_id;
    result[0].buffer=&user_id;
    result[0].buffer_type=MYSQL_TYPE_LONG;
    mysql_stmt_bind_result(stmt,result);
    // 重要：调用 mysql_stmt_fetch 来提取查询结果
    if (mysql_stmt_fetch(stmt) == 0) {
        // 数据成功提取，返回 user_id
        mysql_stmt_close(stmt);
        return user_id;
    } else {
        // 没有返回任何结果或发生了错误
        std::string temp = "server has some problems, please try later";
        util::sendres(m_sockfd,res_type::sighup_fail, temp);
        mysql_stmt_close(stmt);
        LOG(DEBUG) << "fetch error" << std::endl;
        return -1;
    }
}

void signup_log::login(char* msg){
    //其实还有一个可从tinyweb_master借鉴的地方：就是在子进程那里存储一个用户字典，在初始化的时候就查询所有的用户账号密码，存在字典，这样登陆的时候就不用每次都查询数据库了，而且以后关于用户的操作都很方便
    std::string temp(msg);
    int pos=temp.find("#end#");
    std::string id=temp.substr(0,pos);
    std::string password=temp.substr(pos+5);
    MYSQL *conn=nullptr;
    connectionRALL cr(&conn,connection_pool::getinstance());
    // 连接到数据库（注意：如果是远程数据库，host 填写服务器 IP）——不需要了，我使用了链接池
    /* if (!mysql_real_connect(conn, "127.0.0.1", "root", "@Mym2004jiayou...", "music_server", 3306, NULL, 0)) {
        std::cerr << "MySQL Connection Failed: " << mysql_error(conn) << std::endl;
        sql_shutdown();
        mysql_close(conn);
        return;
    } */
    //std::cout << "Connected to MySQL successfully!" << std::endl;1
    if(user_map.find(id)!=user_map.end()){//账号正确
        //成功获取到密码，但是还要比较
        if(user_map[id]==password){//密码正确
            MYSQL_STMT *stmt;
            MYSQL_BIND bind[1], result[1];
            MYSQL_RES *res;
            MYSQL_TIME lastban_time;
            memset(&lastban_time, 0, sizeof(lastban_time));
            // 1. 查询 lastban 时间
            std::string query = "SELECT lastban FROM users WHERE username=?";
            stmt = mysql_stmt_init(conn);
            if (!stmt) {
                //std::cerr << "MySQL stmt init failed\n";
                util::sql_shutdown(m_sockfd);
                LOG(DEBUG)<<"stmt wrong"<<endl;
                //mysql_close(conn);
                return;
            }
            if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
                //std::cerr << "MySQL stmt prepare failed: " << mysql_stmt_error(stmt) << "\n";
                mysql_stmt_close(stmt);
                util::sql_shutdown(m_sockfd);
                LOG(DEBUG)<<"stmt wrong"<<endl;
                //mysql_close(conn);
                return;
            }
            // 绑定查询参数
            memset(bind, 0, sizeof(bind));
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (void*)id.c_str();
            bind[0].buffer_length = id.length();
            mysql_stmt_bind_param(stmt, bind);
            if (mysql_stmt_execute(stmt)) {
                //std::cerr << "Query execution failed: " << mysql_stmt_error(stmt) << "\n";
                mysql_stmt_close(stmt);
                util::sql_shutdown(m_sockfd);
                LOG(DEBUG)<<"wrong"<<endl;
                //mysql_close(conn);
                return;
            }
                // 绑定结果
            memset(result, 0, sizeof(result));
            result[0].buffer_type = MYSQL_TYPE_TIMESTAMP;
            result[0].buffer = (void*)&lastban_time;
            mysql_stmt_bind_result(stmt, result);

            bool has_result = (mysql_stmt_fetch(stmt) == 0);
            mysql_stmt_close(stmt);
                // 2. 计算时间差
            auto now = std::chrono::system_clock::now();
            std::time_t current_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm* current_tm = std::localtime(&current_time_t);
            
            std::tm tm_lastban = {};
            if(has_result){//下面要将mysql的时间数据转化为std::tm类型
                tm_lastban.tm_year = lastban_time.year - 1900;  //std::tm 中，年份是以 1900 为基准的。例如，tm_year = 120 代表 2020 年（因为 120 表示从 1900 年开始的年份差）。但是，MYSQL_TIME 中的 year 字段表示的是完整的年份数，不需要偏移。
                tm_lastban.tm_mon = lastban_time.month - 1;     //在 std::tm 中，月份是从 0 到 11 来表示的：0 是 1 月，1 是 2 月，依此类推
                tm_lastban.tm_mday = lastban_time.day;
                tm_lastban.tm_hour = lastban_time.hour;
                tm_lastban.tm_min = lastban_time.minute;
                tm_lastban.tm_sec = lastban_time.second;
            }
            std::time_t lastbanTime = std::mktime(&tm_lastban);
            if (!has_result || (std::difftime(current_time_t, lastbanTime) > 3*60*60)) {
                // 超过 3 小时或 lastban 为空，则更新 lefttime 并返回 'a'
                std::string update_query = "UPDATE users SET lefttime=5,lastlog=NOW() WHERE username=?";
                stmt = mysql_stmt_init(conn);
                if (!stmt || mysql_stmt_prepare(stmt, update_query.c_str(), update_query.length())) {
                    std::cerr << "Prepare statement failed: " << mysql_stmt_error(stmt) << "\n";
                    mysql_stmt_close(stmt);
                    util::sql_shutdown(m_sockfd);
                    LOG(DEBUG)<<"wrong"<<endl;
                    //mysql_close(conn);
                    return;
                }
                mysql_stmt_bind_param(stmt, bind);
                mysql_stmt_execute(stmt);
                mysql_stmt_close(stmt);
                //查询user_id(利用usermap)
                user_id=get_userid(id);
                username=id;
                if(user_id==-1){
                    util::sql_shutdown(m_sockfd);
                    return;
                }
                //接下来要与本地数据库进行数据核对。核对信息（仅限于当前登录用户）:歌单，歌单的歌曲。歌曲。用户
                //先更新用户列表，但是可能
                //---------------------------------------------------------------------------------------
                std::string temp_user_id=to_string(user_id);//to_string返回右值，不能传给左值引用
                util::sendres(m_sockfd,res_type::login_success,temp_user_id);
                //应当添加一个线程间的标志，该标志满足后再往下执行update_song
                //update_song(id);//先同步音乐（发送）——这里有问题：如果客户端是异地登陆，此时它正在疯狂接收数据，然后accept_request线程在疯狂发送数据，这里发送的数据可能会丢失
                //send_update(user_id);//再发送上一次的同步时间给客户端，为后面接收前端数据做准备

                //update_update(user_id);//最后同步同步时间
                return;
            }
            else{
                // 距离上次封禁不足 3 小时，返回 'z'
                // 将 time_t 转换为本地时间
                std::tm local_tm = *std::localtime(&lastbanTime);
                // 使用 stringstream 进行格式化
                std::ostringstream oss;
                oss << "Current time: " << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
                std::string msg = oss.str();
                std::string temp1="you are baned in";
                msg=temp1+msg;
                util::sendres(m_sockfd,res_type::login_fail,msg);
                return;
            }
        }
        else{//密码错误
            int lefttime=0;
            MYSQL_STMT *stmt;
            MYSQL_BIND bind[3], result[3];
            MYSQL_RES *res;
            MYSQL_ROW row;
            std::string selectQuery = "SELECT lefttime, lastlog, lastban FROM users WHERE username = ?";
            stmt = mysql_stmt_init(conn);
            if (!stmt || mysql_stmt_prepare(stmt, selectQuery.c_str(), selectQuery.length())) {
                util::sql_shutdown(m_sockfd);
                LOG(DEBUG)<<"stmt wrong"<<endl;
                return;
            }
            memset(bind, 0, sizeof(bind));
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (char *)id.c_str();
            bind[0].buffer_length = id.length();
            mysql_stmt_bind_param(stmt, bind);
            if (mysql_stmt_execute(stmt) || mysql_stmt_store_result(stmt)) {
                //std::cerr << "Query execution failed: " << mysql_stmt_error(stmt) << std::endl;
                util::sql_shutdown(m_sockfd);
                LOG(DEBUG)<<"stmt wrong"<<endl;
                mysql_stmt_close(stmt);
                //mysql_close(conn);
                return;
            }
            // 绑定查询结果
            memset(result, 0, sizeof(result));
            result[0].buffer_type = MYSQL_TYPE_LONG;
            result[0].buffer = &lefttime;
            MYSQL_TIME lastlog, lastban;
            result[1].buffer_type = MYSQL_TYPE_TIMESTAMP;
            result[1].buffer = &lastlog;
            result[2].buffer_type = MYSQL_TYPE_TIMESTAMP;
            result[2].buffer = &lastban;
            mysql_stmt_bind_result(stmt, result);
            if (mysql_stmt_fetch(stmt) == 0) {
                lefttime--; // 登录次数 -1
            }
            mysql_stmt_close(stmt);
            // 2. 计算时间间隔
            auto now = std::chrono::system_clock::now();
            auto nowTimeT = std::chrono::system_clock::to_time_t(now);
            if(lastban.year>0){
                std::tm tm_lastban = {};//下面要将mysql的时间数据转化为std::tm类型
                tm_lastban.tm_year = lastban.year - 1900;  //std::tm 中，年份是以 1900 为基准的。例如，tm_year = 120 代表 2020 年（因为 120 表示从 1900 年开始的年份差）。但是，MYSQL_TIME 中的 year 字段表示的是完整的年份数，不需要偏移。
                tm_lastban.tm_mon = lastban.month - 1;     //在 std::tm 中，月份是从 0 到 11 来表示的：0 是 1 月，1 是 2 月，依此类推
                tm_lastban.tm_mday = lastban.day;
                tm_lastban.tm_hour = lastban.hour;
                tm_lastban.tm_min = lastban.minute;
                tm_lastban.tm_sec = lastban.second;
                std::time_t lastbanTime = std::mktime(&tm_lastban);
                if(lastbanTime!=-1){
                    if(difftime(nowTimeT,lastbanTime)>3*60*60){
                        if(lefttime<0){
                            lefttime = 4; // 解除封禁
                        }
                    }
                    else{
                        std::tm local_tm = *std::localtime(&lastbanTime);
                        // 使用 stringstream 进行格式化
                        std::ostringstream oss;
                        oss << "Current time: " << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
                        std::string msg = oss.str();
                        std::string temp1="you are baned in";
                        msg=temp1+msg;
                        util::sendres(m_sockfd,res_type::login_fail,msg);
                        return;
                    }
                }
            }
            if (lastlog.year > 0) {//确保lastlog不为null
                std::tm tm_lastlog = {};//下面要将mysql的时间数据转化为std::tm类型
                tm_lastlog.tm_year = lastlog.year - 1900;  //std::tm 中，年份是以 1900 为基准的。例如，tm_year = 120 代表 2020 年（因为 120 表示从 1900 年开始的年份差）。但是，MYSQL_TIME 中的 year 字段表示的是完整的年份数，不需要偏移。
                tm_lastlog.tm_mon = lastlog.month - 1;     //在 std::tm 中，月份是从 0 到 11 来表示的：0 是 1 月，1 是 2 月，依此类推
                tm_lastlog.tm_mday = lastlog.day;
                tm_lastlog.tm_hour = lastlog.hour;
                tm_lastlog.tm_min = lastlog.minute;
                tm_lastlog.tm_sec = lastlog.second;
                // 将 std::tm 转换为 std::time_t
                std::time_t lastlogTime = std::mktime(&tm_lastlog);
                if(lastlogTime!=-1)
                {
                    if (difftime(nowTimeT, lastlogTime) >= 5 * 60) {
                        lefttime = 4;  // 超过 5 分钟，重置为 4 次
                    }
                }
                else{
                    std::cerr << "时间转换失败" << std::endl;
                    return;
                }
            }
            std::string updateQuery = "UPDATE users SET lastlog = NOW(), lefttime = ? WHERE username = ?";
            stmt = mysql_stmt_init(conn);
            if (!stmt || mysql_stmt_prepare(stmt, updateQuery.c_str(), updateQuery.length())) {
                //std::cerr << "Update preparation failed: " << mysql_stmt_error(stmt) << std::endl;
                util::sql_shutdown(m_sockfd);
                LOG(DEBUG)<<"stmt wrong"<<endl;
                //mysql_close(conn);
                return;
            }
            memset(bind, 0, sizeof(bind));
            bind[0].buffer_type = MYSQL_TYPE_LONG;
            bind[0].buffer = &lefttime;
            bind[1].buffer_type = MYSQL_TYPE_STRING;
            bind[1].buffer = (char *)id.c_str();
            bind[1].buffer_length = id.length();
            mysql_stmt_bind_param(stmt, bind);
            mysql_stmt_execute(stmt);
            mysql_stmt_close(stmt);
            if(lefttime>0){
                char buffer[50];
                std::sprintf(buffer, "you have only %d times left", lefttime);
                std::string msg = buffer;
                util::sendres(m_sockfd,res_type::login_fail,msg);
                //mysql_close(conn);
                return;
            }
            // 4. 如果 `lefttime <= 0`，记录封禁时间
            else {
                std::string banQuery = "UPDATE users SET lastban = NOW() WHERE username = ?";
                stmt = mysql_stmt_init(conn);
                if (!stmt || mysql_stmt_prepare(stmt, banQuery.c_str(), banQuery.length())) {
                    //std::cerr << "Ban update failed: " << mysql_stmt_error(stmt) << std::endl;
                    util::sql_shutdown(m_sockfd);
                    LOG(DEBUG)<<"stmt wrong"<<endl;
                    //mysql_close(conn);
                    return;
                }
                memset(bind, 0, sizeof(bind));
                bind[0].buffer_type = MYSQL_TYPE_STRING;
                bind[0].buffer = (char *)id.c_str();
                bind[0].buffer_length = id.length();

                mysql_stmt_bind_param(stmt, bind);
                mysql_stmt_execute(stmt);
                mysql_stmt_close(stmt);
                now = std::chrono::system_clock::now();
                nowTimeT = std::chrono::system_clock::to_time_t(now);
                // 将 time_t 转换为本地时间
                std::tm local_tm = *std::localtime(&nowTimeT);
                // 使用 stringstream 进行格式化
                std::ostringstream oss;
                oss << "Current time: " << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
                std::string msg = oss.str();
                std::string temp1="you are baned in";
                msg=temp1+msg;
                util::sendres(m_sockfd,res_type::login_fail,msg);
                //mysql_close(conn);
                return;
            }
        }
    }
    std::string dontexits("username dont exits");
    util::sendres(m_sockfd,res_type::login_fail,dontexits);
    //mysql_close(conn);
    return;
}

void signup_log::signup(char* msg){
    std::string temp(msg);
    int pos=temp.find("#end#");
    std::string username=temp.substr(0,pos);
    std::string password=temp.substr(pos+5);
    if(user_map.find(username)!=user_map.end()){
        std::string temp="user has existed";
        util::sendres(m_sockfd,res_type::sighup_fail,temp);
        return;
    }
    MYSQL *conn;
    connectionRALL cr(&conn,connection_pool::getinstance());
    //用户名不存在，可以创建
    MYSQL_STMT *stmt;
    MYSQL_BIND bind[2];
    MYSQL_RES *res;
    std::string query="INSERT INTO users (username, password,lefttime) VALUES (?, ?,5)";
    stmt = mysql_stmt_init(conn);
    if (!stmt || mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        std::string temp="server has some problems,please try later";
        util::sendres(m_sockfd,res_type::sighup_fail,temp);
        return;
    }
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *)username.c_str();
    bind[0].buffer_length = username.length();
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *)password.c_str();
    bind[1].buffer_length = password.length();
    mysql_stmt_bind_param(stmt, bind);
    if (mysql_stmt_execute(stmt)) {
        std::string temp="server has some problems,please try later";
        util::sendres(m_sockfd,res_type::sighup_fail,temp);
        mysql_stmt_close(stmt);
        return;
    }
    //此处获取user_id，并返回
    user_id=get_userid(username);
    if(user_id==-1){
        util::sendres(m_sockfd,res_type::sighup_fail,"server has some problems,please try later");
        return;
    }
    std::string temp_userid=to_string(user_id);
    util::sendres(m_sockfd,res_type::sighup_success,temp_userid);
    mysql_stmt_close(stmt);
}