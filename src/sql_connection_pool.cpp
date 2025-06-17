#include"sql_connection_pool.h"

connection_pool::connection_pool():idelconn(0),curconn(0){}

connection_pool* connection_pool::getinstance(){
    static connection_pool ins;//返回的都是同一个对象()
    return &ins;
}

MYSQL* connection_pool::getconnection(){
    if(connqueue.size()==0)
        return nullptr;
    MYSQL *temp=nullptr;
    sem_wait(&sem);//信号量就是为了多线程而产生的，内部是原子操作，不用放锁里
    {
    unique_lock<mutex> lock(mtx);
    temp=connqueue.front();
    connqueue.pop();
    idelconn--;
    curconn++;
    }
    return temp;
}

void connection_pool::init(string url, string port,string User, string PassWord, string DataBaseName, int MaxConn){
    //cout<<"初始化数据库连接池"<<endl;
    m_url = url;//"localhost"
	m_Port = port;
	m_User = User;
	m_PassWord = PassWord;
	m_DatabaseName = DataBaseName;
    maxconn=MaxConn;
    for(int i=0;i<maxconn;i++){
        MYSQL* con=nullptr;
        con=mysql_init(con);
        if(con==nullptr){
            perror("mysql_init");
            exit(1);
        }
        con=mysql_real_connect(con,url.c_str(),User.c_str(),PassWord.c_str(),DataBaseName.c_str(),stoi(port),nullptr,0);
        if(con==nullptr){
            perror("mysql_real_connect");
            exit(1);
        }
        connqueue.emplace(con);
        idelconn++;
    }
    sem_init(&sem,0,maxconn);
}

int connection_pool::getcurconn(){
    return curconn;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL* conn){
    if (NULL == conn)
		return false;
    {
    unique_lock<mutex> lock(mtx);
    connqueue.emplace(conn);
    idelconn++;
    curconn--;
    }
    sem_post(&sem);
    return true;
}

connection_pool::~connection_pool(){
    DestroyPool();
}

void connection_pool::DestroyPool(){
    {
    unique_lock<mutex> lock(mtx);
    while(!connqueue.empty()){
        MYSQL* temp=connqueue.front();
        connqueue.pop();
        mysql_close(temp); // 关闭 MySQL 连接
    }
    idelconn=0;
    curconn=0;
    sem_destroy(&sem);
    }
}

connectionRALL::connectionRALL(MYSQL **con, connection_pool *connPool){
    *con=connPool->getconnection();
    conRAII=*con;
    poolRAII=connPool;
}

connectionRALL::~connectionRALL(){
    poolRAII->ReleaseConnection(conRAII);
}