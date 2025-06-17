#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <assert.h>
#include <signal.h>
#include<thread>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include"processpool.h"
#include"task_list.h"
#include"common.h"
#include"easylogging.h"
//其实还有一个可从tinyweb_master借鉴的地方：就是在子进程那里存储一个用户字典，在初始化的时候就查询所有的用户账号密码，存在字典，这样登陆的时候就不用每次都查询数据库了，而且以后关于用户的操作都很方便
//musiclists里的user_id可以加一个索引，因为异地登陆发送歌单的时候需要
INITIALIZE_EASYLOGGINGPP
int main(void)
{
	int listen_fd, reuse, ret;
	//int listen_fd2,ret2;
	struct sockaddr_in servaddr;
	//struct sockaddr_in servaddr2;
	reuse=1;
	listen_fd = socket( AF_INET, SOCK_STREAM, 0);
	//listen_fd2 = socket( AF_INET, SOCK_STREAM, 0);
	if ( listen_fd == -1 )
	{
		perror( "create socket error" );
		exit(0);
	}
	
	if ( setsockopt( listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) ) == -1 )
	{
		perror( "reuse socket error" );
		exit(0);
	}

	memset( &servaddr, 0, sizeof( servaddr) );
	servaddr.sin_family = AF_INET;
	//servaddr2.sin_family=AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("172.18.81.202");
	//servaddr2.sin_addr.s_addr = inet_addr("172.18.81.202");
	servaddr.sin_port = htons( 8001 );//这是普通通信用的端口
	//servaddr2.sin_port = htons( 8002 );//这是http通信用的端口

	ret = bind( listen_fd, ( struct sockaddr* )&servaddr, sizeof( servaddr ));
	//ret2=bind( listen_fd2, ( struct sockaddr* )&servaddr2, sizeof( servaddr2 ));
	if ( ret == -1 )
	{
		perror( "bind socket error" );
		exit(0);
	}

	ret = listen( listen_fd, 10 );
	//ret2=listen(listen_fd2,10);
	if ( ret == -1 )
	{
		perror(" listen socket error" );
		exit(0);
	}

	//setlogprio( SET_LOG_LEVEL );

	//daemon_init();
    //http_res hr;
    //std::thread http_thread=std::thread(http_res::response,&hr);
    //http_thread.detach();//直接分离，自己阻塞等待得了
	processpool< task_list >* pool = processpool< task_list >::create( listen_fd );
	
	if ( pool )
	{
		pool->run();
	}
	delete pool;
	close( listen_fd );
	std::cout<<"进程结束"<<'\n';
	return 0;
}