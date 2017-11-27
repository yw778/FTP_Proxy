#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#define BUFFSIZE 65535
#define SVPORT 21
#define PASSTIME 60
#define ACTIVE 1
#define PASV 0
#define CACHE 0
#define DOWNLOAD 1
#define DEFAULTFLOW 2
#define UPLOAD 3

int connectToClientData(int port){
	int sock;
	struct sockaddr_in DestAddr;
	memset(&DestAddr, 0, sizeof(DestAddr));
	DestAddr.sin_family = AF_INET; /* Internet addr family */
	DestAddr.sin_addr.s_addr = inet_addr("192.168.56.1");
	DestAddr.sin_port = htons(port);
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket() failed.\n");
		exit(1);
	}
	if((connect(sock, (struct sockaddr *) &DestAddr, sizeof(DestAddr)))<0)
	{
		perror("connect() failed.\n");
		exit(1);
	}
	return sock;
}

int getFileDescriptor(char *fileName)
{
    int fd;
    
    if ((fd = open(fileName, O_RDWR|O_CREAT)) == -1) {
        perror("open error.\n");
    }
    
    return fd;
}


int fileIsExist(char *fileName)
{
    int fd;
    fd = open(fileName,O_RDONLY,0666);    
    printf("in file is exist %d\n",fd);
    return fd;
}


int acceptCmdSocket(int sock)
{
    struct sockaddr_in clt_addr;
    int clt_size= sizeof(clt_addr);
	memset(&clt_addr, 0, clt_size);
    int new_sock;
    if ((new_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket() failed.\n");
		exit(1);
	}
    if((new_sock = accept(sock,(struct sockaddr *) &clt_addr,&clt_size))<0)
    {
        perror("accept failed.\n");
        exit(1);
    }
	return new_sock;
}

int bindAndListenSocket(int port)
{
    struct sockaddr_in proxy_Addr;
    memset(&proxy_Addr, 0, sizeof(proxy_Addr));
    proxy_Addr.sin_family = AF_INET; /* Internet addr family */
    proxy_Addr.sin_addr.s_addr = inet_addr("192.168.56.101");
    proxy_Addr.sin_port = htons(port);
    int sock;
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket() failed.\n");
		exit(1);
	}
    if((bind(sock,(struct sockaddr *) &proxy_Addr, sizeof(proxy_Addr)))<0)
    {
       printf("bind() failed.\n");
       exit(1);
    }
    if((listen(sock,BUFFSIZE))<0)
    {
        printf("listen() failed.\n");
        exit(1);
    }
    return sock;
}



int connectToServer()
{
    struct sockaddr_in serv_Addr;
    memset(&serv_Addr, 0, sizeof(serv_Addr));
    serv_Addr.sin_family = AF_INET; /* Internet addr family */
    serv_Addr.sin_addr.s_addr = inet_addr("192.168.56.1");
    serv_Addr.sin_port = htons(21);
    int sock;
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("socket() failed.\n");
		exit(1);
	}
    if((connect(sock, (struct sockaddr *) &serv_Addr, sizeof(serv_Addr)))<0)
	{
		perror("connect failed");
		exit(1);
	}
	return sock;
}


int main(int argc, const char *argv[])
{
	fd_set master_set, working_set;  //�ļ�����������
    struct timeval timeout;          //select �����еĳ�ʱ�ṹ��
    int proxy_cmd_socket    = 0;     //proxy listen��������
    int accept_cmd_socket   = 0;     //proxy accept�ͻ�������Ŀ�������
    int connect_cmd_socket  = 0;     //proxy connect������������������
    int proxy_data_socket   = 0;     //proxy listen��������
    int accept_data_socket  = 0;     //proxy accept�õ�������������ӣ�����ģʽʱaccept�õ��������������ӵ����󣬱���ģʽʱaccept�õ��ͻ����������ӵ�����
    int connect_data_socket = 0;     //proxy connect������������ ������ģʽʱconnect�ͻ��˽����������ӣ�����ģʽʱconnect�������˽����������ӣ�
    int selectResult = 0;     //select��������ֵ
    int select_sd = 20;    //select ��������������ļ�������
    
    int i=0;
    int dataPort;
    int active_port;
    char* a;
    char* b;
    char filename[BUFFSIZE]={"\0"};
    int download_fd;
    int dataflow=DEFAULTFLOW;
    int download_fd_data;
    int mode;
    int short_cut_socket;
    int short_cut_socket_pass;
      
    FD_ZERO(&master_set);   //���master_set���� 
    bzero(&timeout, sizeof(timeout));
    
	proxy_cmd_socket = bindAndListenSocket(21);   
    FD_SET(proxy_cmd_socket, &master_set);  //��proxy_cmd_socket����master_set����
//    FD_SET(proxy_data_socket, &master_set);//��proxy_data_socket����master_set����
    
    timeout.tv_sec = PASSTIME;    //Select�ĳ�ʱ����ʱ��
    timeout.tv_usec =0;    //ms
    
    while (1) {
        FD_ZERO(&working_set); //���working_set�ļ�����������
        memcpy(&working_set, &master_set, sizeof(master_set)); //��master_set����copy��working_set����
        dataflow=DEFAULTFLOW;
        //selectѭ������ ����ֻ�Զ������ı仯���м�����working_setΪ���Ӷ������������������ļ��ϣ�,�����͵��ĸ�������NULL������д����������������м���
        selectResult = select(select_sd, &working_set, NULL, NULL, &timeout);
        
        // fail
        if (selectResult < 0) {
            perror("select() failed\n");
            exit(1);
        }
        
        // timeout
        if (selectResult == 0) {
            printf("select() timed out.\n");
            timeout.tv_sec = PASSTIME;
            continue;
        }
        
        // selectResult > 0 ʱ ����ѭ���ж��б仯���ļ�������Ϊ�ĸ�socket
        for (i = 0; i < select_sd; i++) {
            //�жϱ仯���ļ��������Ƿ������working_set����
            if (FD_ISSET(i, &working_set)) {
                if (i == proxy_cmd_socket) {
                    printf("enter proxy_cmd_socket\n");
					close(accept_cmd_socket);					
					FD_CLR(accept_cmd_socket,&master_set);
					accept_cmd_socket = acceptCmdSocket(proxy_cmd_socket);  //ִ��accept����,����proxy�Ϳͻ���֮��Ŀ�������
                    connect_cmd_socket = connectToServer(); //ִ��connect����,����proxy�ͷ�������֮��Ŀ�������
                    
                    //���µõ���socket���뵽master_set�����
                    FD_SET(accept_cmd_socket, &master_set);
                    FD_SET(connect_cmd_socket, &master_set);
                }
                
                if (i == accept_cmd_socket) {
                    printf("enter accept_cmd_socket\n");
                    char buff[BUFFSIZE] = {'\0'};
					char buff_di[BUFFSIZE] = {'\0'};
					char buff_toClientData[BUFFSIZE] = {'\0'};
					char buff_toClientCmd[BUFFSIZE]={'\0'};
				    dataflow=DEFAULTFLOW;
                   
                    if (read(i, buff, BUFFSIZE) > 0){
                        //������յ�����,������ݽ��б�Ҫ�Ĵ���֮���͸��������ˣ�д��connect_cmd_socket��

                        //����ͻ��˷���proxy��request������������Ҫ���д�����PORT��RETR��STOR                        
//                        char *c;
                        //////////////                                               
                        //////////////
                        //////////////
                        //PORT
                    	printf("client: %s",buff);

						//PORT
                        if(strncmp(buff,"PORT",4)==0)
                        {
                            mode=ACTIVE;
							printf("port cmd\n");
							a = strtok(buff," ");
                            int j;
							for(j=0;j<3;j++)
                            {
                                a = strtok(NULL,",");
                                b = strtok(NULL,",");
                            }
	                        int x = atoi(a);
	                        int y = atoi(b);
	                        dataPort = x*256+y;
//							x=100+rand()%(100);
//							y=100+rand()%(100);
//							active_port=x*256+y;
	                            
	                        close(proxy_data_socket);
	                        FD_CLR(proxy_data_socket, &master_set);
	                            
	                        sprintf(buff,"PORT 192,168,56,101,%d,%d\r\n",x,y);
	                        printf("-----enter port");
	                            
			                proxy_data_socket = bindAndListenSocket(dataPort);
			                FD_SET(proxy_data_socket, &master_set);
                        }
                        //RETR
						else if(strncmp(buff,"RETR",4)==0)
                        {
							printf("get cmd\n");
							memset(filename,0,sizeof(filename));
							strcpy(buff_di,buff);
							strtok(buff_di," ");
							strcpy(filename,strtok(NULL,"\r"));
							printf("filename: %s\n",filename);
                            download_fd=fileIsExist(filename);
							if(download_fd > 0)
							{	
								dataflow=CACHE;
								printf("cache exist");
								if(mode=ACTIVE){
									short_cut_socket = connectToClientData(dataPort);									
								}else{
									short_cut_socket_pass=acceptCmdSocket(proxy_data_socket);
								}
								int len=0;
								sprintf(buff_toClientCmd,"150 Opening data channel for file download from server of /%s\r\n",filename);
							    write(i,buff_toClientCmd,strlen(buff_toClientCmd));
								while((len=read(download_fd,buff_toClientData,BUFFSIZE))>0){
									if(mode=ACTIVE){
										write(short_cut_socket,buff_toClientData,len);										
									}else{
										write(short_cut_socket_pass,buff_toClientData,len);
									}
								}
								sprintf(buff_toClientCmd,"226 Successfully transferred /%s\n",filename);
								write(i,buff_toClientCmd,strlen(buff_toClientCmd));
								close(download_fd);
								if(mode==ACTIVE){
									close(short_cut_socket);
								}else{
									close(short_cut_socket_pass);
								}
							}
                            else
                            {
								dataflow=DOWNLOAD;
								printf("cache not exist");
								download_fd_data=getFileDescriptor(filename);
//								printf("%d---\n",download_fd_data);
                            }
		                }   
						else if(strncmp(buff,"PASV",4)==0){
		                    	printf("enter pasv mode");
								mode=PASV;
						}
						else if(strncmp(buff,"STOR",4)==0){
							printf("store cmd");	
							dataflow=UPLOAD;						
						}
                        //д��proxy��server������cmd����,����PORT֮�⣬ֱ��ת��buff����
                        if(dataflow!=CACHE){
							write(connect_cmd_socket, buff, strlen(buff));
		                    printf("no cache or other data!\n");
                        }
                	}
                }
                
                if (i == connect_cmd_socket) {
                  printf("connect_cmd_socket\n");
                  char buff[BUFFSIZE] = {'\0'};
                  char buff_di[BUFFSIZE]={'\0'};
                  if (read(i, buff, BUFFSIZE) > 0)
                    {    
						strcpy(buff_di,buff);						                    
                  		//PASV�յ��Ķ˿� 227 ��port��
                        if(strncmp(buff,"227",3)==0&&mode==PASV)
                        {
                            a = strtok(buff_di," ");//?
                            int j; 
							for(j=0;j<3;j++)
                            {
                                a = strtok(NULL,",");
                                b = strtok(NULL,",");
                            }
                        int x = atoi(a);
                        int y = atoi(b);
                        dataPort = x*256+y;
                        close(proxy_data_socket);
                        FD_CLR(proxy_data_socket, &master_set);
                        sprintf(buff,"227 Entering Passive Mode (192,168,56,101,%d,%d)\r\n",x,y);
		                proxy_data_socket = bindAndListenSocket(dataPort);
		                FD_SET(proxy_data_socket, &master_set);//����proxy_data_socketo��??master_seto?��?
                        }
                        write(accept_cmd_socket, buff, strlen(buff));
					}
				}
              
                if (i == proxy_data_socket) {
                  printf("proxy_data_socket\n");
				  accept_data_socket = acceptCmdSocket(proxy_data_socket);
				  connect_data_socket = connectToClientData(dataPort);
				  FD_SET(accept_data_socket, &master_set);
				  FD_SET(connect_data_socket, &master_set);
                }
                
                if (i == accept_data_socket) {
                  printf("accept_data_socket\n");
				    //�������أ������ϴ� 
				  char buff_data[BUFFSIZE]={'\0'};
				//  memset(buff_data,0,BUFFSIZE);
				  int lenr=0;
				  while((lenr=read(accept_data_socket,buff_data,BUFFSIZE))>0){
				  	if(download_fd_data>0){
				  		printf("download_fd_data for active download\n");
			            write(download_fd_data,buff_data,lenr);
				  	}
				  	write(connect_data_socket,buff_data,lenr);
				  }
 				  if(download_fd_data>0){
 				  	close(download_fd_data);
 				  }
			     close(i); //������ղ�������,��ر�Socket
			     FD_CLR(i, &master_set);
			     close(connect_data_socket); //������ղ�������,��ر�Socket
				 FD_CLR(connect_data_socket, &master_set);				    
                }
                
                if (i == connect_data_socket) {
                    printf("connect_data_socket\n");
					char buffer[BUFFSIZE]={'\0'};					
					memset(buffer,0,BUFFSIZE);
					int readdata=0;
					while((readdata=read(connect_data_socket,buffer,BUFFSIZE))>0){
						if(download_fd_data>0){
							printf("passive download\n");
							write(download_fd_data,buffer,readdata);
						}
						write(accept_data_socket,buffer,readdata);
					}
                	if(download_fd_data>0){
                		close(download_fd_data);                  		
                	}
                    close(i);
					FD_CLR(i, &master_set);
					close(accept_data_socket); //������ղ�������,��ر�Socket
					FD_CLR(accept_data_socket, &master_set);

                }
            }
        }
    }
    
    return 0;
}