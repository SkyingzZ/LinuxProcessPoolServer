#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/stat.h>

//被封装的进程池类
#include "myProcessPool.h"


//用来处理用户连接，真正的send and recv message with user socket
class user_conn
{
private:
	static const int BUFFER_SIZE = 1024;	//读缓冲区的大小
	static int m_epollfd;					//epollfd
	int m_sockfd;							//客户端socketfd
	sockaddr_in m_address;					//客户端地址
	char m_buf[BUFFER_SIZE];				//读缓冲区

	int m_read_idx;							//当前读指针位于读缓冲区的下一个位置

public: 

	/*作为processpoll的模板类，init，process两个函数必须实现*/

	//初始化客户连接，当有客户连接请求时，进程池回调用池函数
	void init(int epollfd, int sockfd, const sockaddr_in& client_addr)
	{
		m_epollfd = epollfd;
		m_sockfd = sockfd;
		m_address = client_addr;
		memset(m_buf, '\0', BUFFER_SIZE);
		m_read_idx = 0;
		printf("user connect is coming!\n");
	}
	
	//处理客户端发来的消息
	void process()
	{
		int idx = 0;
		int ret = -1;
		while(true)
		{
			idx = m_read_idx;
			ret = recv(m_sockfd, m_buf + idx, BUFFER_SIZE-1-idx, 0);

			//如果发生错误，则关闭客户连接，如果暂时无数据可读，则退出循环
			if(ret < 0)
			{
				if(errno != EAGAIN)	
				{
					printf("user error is: %d,and removed it\n",m_sockfd);
					removefd(m_epollfd, m_sockfd);
				}
				break;
			}
			//如果对方关闭连接，则服务器也关闭连接
			else  if(ret == 0)
			{
				printf("user exit is : %d\n",m_sockfd);
				removefd(m_epollfd, m_sockfd);	
				break;
			}
			else
			{
				m_read_idx += ret;
				//如果遇到字符“\r\n”，则开始处理客户请求
				for( ; idx < m_read_idx; ++idx)
				{
					if((idx >= 1) && (m_buf[idx-1] == '\r') && (m_buf[idx] == '\n'))
					{
						break;	
					}
				}
				//如果没有遇到字符'\r\n',则需要读取更多的客户数据
				if(idx == m_read_idx)
				{
					continue;	
				}
				m_buf[idx - 1] = '\0';
				//
				//字符处理逻辑
				//
				printf("user content is : %s\n",m_buf);
				send(m_sockfd, "i am recv your word\n", sizeof("i am recv your word\n"),0);
				memset(m_buf,0,BUFFER_SIZE);
				m_read_idx = 0;
			}

		}
	}

};

int user_conn::m_epollfd = -1;

int main(int argc, char* argv[])
{
	if(argc <= 2)
	{
		printf("usage L %s ip_address port_name\n ",basename(argv[0]));	
		return 1;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);

	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);

	int ret = 0;
	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);
	
	ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
	assert(ret != -1);

	ret = listen(listenfd, 5);
	assert(ret != -1);

	processpool<user_conn>* pool = processpool<user_conn>::create(listenfd);
	if(pool)
	{
		pool->run();
		delete pool;
	}
	close(listenfd);
	return 0;
}
