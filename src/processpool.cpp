#include"processpool.h"
#include"http_request.h"
#include <cassert>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
//#include <signal.h>
#include <sys/epoll.h>
#include<fcntl.h>
#include <errno.h>
#include <cstring>
#include<unordered_map>
#include"easylogging.h"
#include"sql_connection_pool.h"
template class processpool<task_list>;//模板类的函数实现要么放在头文件，要么在cpp文件中实例化

int sig_pipefd[2];//全局变量在一个源文件中定义，在其他源文件要使用时要声明extern——extern int sig_pipefd[2];

std::unordered_map<std::string,std::string> user_map;//只有子进程加载了用户图

template<typename T>
processpool<T>* processpool<T>::instance=nullptr;

template<typename T>
processpool<T>::processpool(int listen_fd,int pronum):proc_id(-1),listenfd(listen_fd),pro_num(pronum),m_stop(0)
{
    pro_vector=new process[pro_num];
    for(int i=0;i<pro_num;i++){
        int ret=socketpair(PF_UNIX,SOCK_STREAM,0,pro_vector[i].pipefd);
        assert(ret==0);
        pro_vector[i].pid=fork();//！这里把pid赋值为fork，是为了后面的run_parent里的kill打基础
        if(pro_vector[i].pid==0){
            close(pro_vector[i].pipefd[0]);//关掉子进程的一端，但是仍是全双工
            proc_id=i;//修改进程id，区别开父进程和子进程。每个子进程都有自己的id（每个子进程都有独自拥有自己的进程空间）
            break;//子进程跳出循环执行工作代码
        }
        else{
            close(pro_vector[i].pipefd[1]);
            continue;//父进程还需要继续fork
        }
    }
}

template<typename T>
void processpool<T>::run(){
    if(proc_id!=-1){
        run_child();
        return;
    }
    else{
        run_parent();
    }
}

template<typename T>
void processpool<T>::run_parent(){
    setup_sig_pipe();
    addfd(epoll_fd,listenfd);
    epoll_event events[100];//最多同时响应100个
    int cur_pro=0;//循环标志
    int newconn_flag=1;
    download::getinstance().start_work();
    while(!m_stop){
        int number=epoll_wait(epoll_fd,events,100,-1);//-1表示不设置超时
        if(number<0&&errno != EINTR ){
            //errno != EINTR：
            /* errno 是 C/C++ 中的一个全局变量（实际上是一个宏），用于存储最近一次系统调用或库函数发生错误时的错误代码。
            当epoll_wait发生错误时会设置errno，EINTR表示epoll_wait 被信号中断了——
            epoll_wait 是一个阻塞调用，当它在等待事件时，如果进程收到了一个信号（如 SIGINT、SIGTERM、SIGCHLD 等），就会被打断，返回 -1 并设置 errno=EINTR。
            而我已经为这个进程注册了以上信号，所以理应不可能出现这个错误，如果是别的错误那就是系统真的出错了 */
            break;//epoll等待错误并且发生的错误不是EINTR时退出父进程，EINTR表示可以恢复的信号中断，比如SIGCHLD、SIGTERM
        }
        for(int i=0;i<number;i++){
            if(events[i].data.fd==listenfd){
                int temp=cur_pro;
                do{
                    if(pro_vector[i].pid!=-1){//找到一个可用的子进程（评判标准是pid != -1,因为从下面代码可知有一个子进程被回收的标志是把他的pid变为-1）
                        break;
                    }
                    LOG(DEBUG)<<"有子进程退出了"<<std::endl;
                    temp=( temp + 1 ) % pro_num;//循环查找，相当于从队取一个，然后取出来工作完了就放到尾巴，这叫Round Robin算法
                }while ( temp != cur_pro );//从子进程队列中取第一个出来执行任务
                if(pro_vector[temp].pid==-1){
                    m_stop=1;
                    break;
                }
                cur_pro=(temp+1)%pro_num;
                send(pro_vector[temp].pipefd[0],(char*)&newconn_flag,sizeof(newconn_flag),0);
            }

            else if((events[i].data.fd==sig_pipefd[0] ) && ( events[i].events & EPOLLIN ) ){//父进程收到了信号
                int signal;
				int ret = recv( sig_pipefd[0], ( char * )&signal, sizeof( signal ), 0 );
				if ( ret < 0 )
				{
					continue;
				}
                else{
                    switch ( signal )
					{
						case SIGCHLD:
                        {
                            pid_t pid;
							int stat;
                            while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )//回收所有结束的子进程
							{
								for ( int i = 0; i < pro_num; i++ )
								{
									if ( pro_vector[i].pid == pid )
									{
										//printf("child %d will be exit\n", pid);
										pro_vector[i].pid = -1;//将所有已结束的子进程m_pid设置为-1。这样他们就会从空闲子进程列表中删除
										close( pro_vector[i].pipefd[0] );//关闭它们与子进程通信的管道
									}
								}//关闭所有子进程
							}
							m_stop = true;//关闭父进程
                            for ( int i = 0; i < pro_num; i++ )
							{
								if ( pro_vector[i].pid != -1 )
								{
									m_stop = false;
								}
							}//只要还有空闲的子进程，即m_pid != -1，那就不用关闭
							break;
                        }
                        case SIGINT:
                        case SIGTERM:
                        {
                            for ( int i = 0; i < pro_num; i++ )
                            {
                                int pid = pro_vector[i].pid;//每个process对应的子进程的id
                                if(pid==-1)
                                    continue;//kill -1会把所有进程全部杀死

                                close( pro_vector[i].pipefd[0] );
                                kill( pid, SIGTERM );
                            }
                            m_stop = true;
                            break;
                        }//结束所有孩子进程
                    }
                }
            }
            else{
                continue;
            }
        }
    }
    download::getinstance().stop();
    epoll_ctl( epoll_fd, EPOLL_CTL_DEL, listenfd, 0 );
    close(listenfd);
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(epoll_fd);
}

template<typename T>
void processpool<T>::load_user(){
    MYSQL* conn=nullptr;
    connectionRALL cr(&conn,connection_pool::getinstance());
    if (mysql_query(conn, "SELECT username,password FROM users"))//mysql_query成功返回0
    {
        perror("mysql_query");
        return;
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(conn);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        user_map[temp1] = temp2;
    }
}

template<typename T>
void processpool<T>::run_child(){//这个函数是子进程执行的
    setup_sig_pipe();
    addfd(epoll_fd,pro_vector[proc_id].pipefd[1]);//每个子进程都拥有整个pro_vector,但只用监听一个和父进程连接的端口即可
    epoll_event events[100];
    T *work_queue=new T[65536];//需要释放
    //ThreadPool *thread_pool=new ThreadPool(4);不能把线程池放这，因为可能客户的请求是要分先后顺序的，比如客户创建歌单，之后才能往歌单插入歌曲，太快的并发会有问题
    connection_pool::getinstance()->init("127.0.0.1","3306","root","Mym2004jiayou...","music_server",5);
    load_user();
    while(!m_stop){
        int number=epoll_wait(epoll_fd,events,100,-1);
        if(number<0&&errno!=EINTR){

            break;
        }
        for(int i=0;i<number;i++){
            if(( events[i].data.fd == pro_vector[proc_id].pipefd[1] ) && ( events[i].events & EPOLLIN )){//有新链接

                int client = 0;
				//这的recv是个jb，就是一个标志，啥都不是，就是父进程发了个1过来，获取的连接在下面
				int ret = recv( events[i].data.fd, ( char * )&client, sizeof( client ), 0 );//(char *) &client：这意味着将 client 变量的地址当做 char * 类型来处理，指向的是 client 的内存位置，但按字节处理（每个字符占 1 字节），符合recv在按字节接收到缓存区的规定
                if ( ( ret < 0 ) && ( errno != EAGAIN ) || ( ret == 0 ) )
				{
					continue;
				}
                struct sockaddr_in client_add;
                //int connfd=accept(listenfd,(struct sockaddr*)&client_add,sizeof(client_add));错误：
                socklen_t client_add_len = sizeof(client_add);
                int connfd=accept(listenfd,(struct sockaddr*)&client_add, &client_add_len);
                if(connfd<0)
                    //可以加一个日志系统，记录错误
                    continue;
                addfd(epoll_fd,connfd);
                work_queue[connfd].init(connfd);
            }
            else if((events[i].data.fd==sig_pipefd[0]) && ( events[i].events & EPOLLIN )){//有信号发送给子进程

                int signal;
				int ret = recv( sig_pipefd[0], ( char * )&signal, sizeof( signal ), 0 );
				if ( ret < 0 )
				{
					continue;
				}
				else
				{
					switch ( signal )
                    {
                        case SIGCHLD://当一个子进程终止、停止或被暂停时，父进程会收到这个信号。
                        {
                            pid_t pid;
							int stat;
							while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )//回收所有停止的子进程？？？为什么子进程要调用这个玩意
							{
								continue;
							}
							break;//跳出循环
                        }
                        case SIGINT:
						case SIGTERM:
						{

							m_stop = true;//直接退出
							break;
						}
                    }
                }
            }
            else if(events[i].events&EPOLLIN){//events[i].events & EPOLLIN 判断当前事件是否表示文件描述符可读。
                //thread_pool->addTask(work_queue[events[i].data.fd].accept_request);//错误：不能直接传递一个类的非静态函数，要绑定对象
                //法一：使用 std::bind 来将 accept_request() 与特定实例绑定，生成一个可调用的对象。

                int conn=events[i].data.fd;
                work_queue[conn].accept_request(epoll_fd,conn);
                //thread_pool->addTask(std::bind(&task_list::accept_request, &work_queue[events[i].data.fd],epoll_fd,conn));不能用线程池了，因为发送数据是要按顺序的，而且多个线程recv会有竞态
                //法二：使用 lambda 表达式来封装 accept_request() 调用，生成一个可传递的可调用对象。
                //thread_pool->addTask([&work_queue, i]() {
                    //work_queue[events[i].data.fd].accept_request();
                //});
                //这里的work_queue[events[i].data.fd]一定是经过init了的
            }
            else{

                continue;
            }
        }
    }
    delete[] work_queue;

    //thread_pool->stop();
    
    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,sig_pipefd[0],0);
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(pro_vector[proc_id].pipefd[1]);
    close(epoll_fd);
    //添加一些尾处理
}

template<typename T>
processpool<T>::~processpool()
{
    delete[] pro_vector;
}

template<typename T>
void processpool<T>::addfd(int epollfd,int fd){
    /* 此处为何要将fd设置为非阻塞：
        因为fd默认是阻塞的，阻塞的特点：
            比如fd=accept；之后在read(fd)操作中会一直阻塞，知道fd可读
        非阻塞特点：
            read(fd)不会阻塞，直接返回。如果fd可读，那么read返回字节数，不可读，会设置errno为EAGAIN，接着程序继续执行
        这里我设置了边缘触发模式，必须要设置非阻塞，这样就能与循环配合做到一次处理完所有数据。 */
    epoll_event event;//创建临时的epoll_event对象
	event.data.fd = fd;//将fd加装到event上
	event.events = EPOLLIN | EPOLLET;// 监听可读事件 + 边缘触发模式
	epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );// 将 fd 添加到 epoll 实例
	setnonblock( fd );
    //注：O_NONBLOCK 是文件状态标志（用于 fcntl）。EPOLLIN 和 EPOLLET 是**epoll 事件**，两者不是同一类标志。
} 

template<typename T>
processpool<T>* processpool<T>::create(int listen_fd,int pro_num){
    if(!instance){
        instance=new processpool<T>(listen_fd,pro_num);
    }
    return instance;
}

void sig_handler(int sig){//当你不希望某个函数会改变主程序的错误处理逻辑，那就在执行这个函数时保存errno
    int save_errno = errno;//errno 是一个全局变量，用于存储最近的错误代码，先保存 errno 的值，因为其他函数可能会修改它
	int msg = sig;
	send( sig_pipefd[1], ( char * )&msg, sizeof( msg ), 0 );//非阻塞发送
	errno = save_errno;//恢复原始的 errno 值，确保信号处理函数不会影响到程序中其他地方的错误代码。
}

template<typename T>
void processpool<T>::addsig(int sig,void(*handler)(int)){
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );// 清空 sa 结构体
    sa.sa_handler=handler;// 设置信号处理函数
    //sa.sa_handler = handler
    sigfillset( &sa.sa_mask);// 将所有信号添加到屏蔽信号集
	assert( sigaction( sig, &sa, NULL ) != -1);// 安装信号处理程序
} 

template<typename T>
int processpool<T>::setnonblock( int fd )
{
	int old_option = fcntl( fd, F_GETFL );
	int new_option = old_option | O_NONBLOCK;
	fcntl( fd, F_SETFL, new_option );
	return old_option; 
}

template< typename T >
void processpool< T >::setup_sig_pipe()
{
	epoll_fd = epoll_create1( 0 );
	assert( epoll_fd != -1 );
	int ret = socketpair( PF_UNIX, SOCK_STREAM, 0, sig_pipefd );//sig_pipefd？？
	setnonblock( sig_pipefd[1] );
	addfd( epoll_fd, sig_pipefd[0] );//监听信号管道
    //这里如果把sig_handler放在类里，那很麻烦：首先要用bind，其次由于函数有参数，要有占位符。而且要改addsig的参数，因为bind不能转化为函数指针，只能转为function（参照线程池类的addtask）
    //然后又因为addsig里的sa.sa_handler要函数指针，又要把function转为函数指针，太麻烦了。
	addsig( SIGCHLD, sig_handler );//SIGCHLD：表示子进程结束信号。
	addsig( SIGINT, sig_handler );//SIGINT：通常是终止程序的信号（Ctrl+C）。
	addsig( SIGTERM, sig_handler );//SIGTERM：终止程序的信号
	//这里注册信号处理的函数包括了往信号管道发送信号类型
}



