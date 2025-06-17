#pragma once
#include<mutex>
#include<queue>
#include<semaphore.h>
#include<mysql/mysql.h>
using namespace std;


class connection_pool{
public:
    MYSQL* getconnection();
    bool ReleaseConnection(MYSQL *conn);
    void DestroyPool();					 //销毁所有连接
    static connection_pool* getinstance();
    int getcurconn();
    void init(string url, string port,string User, string PassWord, string DataBaseName, int MaxConn);
private:
    connection_pool();
    ~connection_pool();
    int maxconn;
    int curconn;
    int idelconn;
    mutex mtx;
    sem_t sem;
    queue<MYSQL*> connqueue;
    
public:
	string m_url;			 //主机地址
	string m_Port;		 //数据库端口号
	string m_User;		 //登陆数据库用户名
	string m_PassWord;	 //登陆数据库密码
	string m_DatabaseName; //使用数据库名
};

class connectionRALL{
public:
    connectionRALL(MYSQL **con, connection_pool *connPool);//这里用指针的指针是因为：要修改这个指针的值，如果只是传指针，获取到的只是副本，改它的值没用。
    ~connectionRALL();
private:
    MYSQL *conRAII;//当前操作的空闲的数据库连接
	connection_pool *poolRAII;//RALL机制的连接池，这里只用指针，获取连接池指针的副本即可，因为不用对指针进行修改，只是调用方法。
};