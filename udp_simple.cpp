#include<iostream>
#include <Winsock2.h>
#include<string>
#include<cstring>
#include<string.h>
#define SERVER_PORT 8886
#define CLIENT_PORT 9996
#define BUFFER_SIZE 1024

class Client
{
private:
    int clientSock;
    sockaddr_in serverAddr;
    sockaddr_in clientAddr;

public:
    Client()
    {   //1.创建套接字
        clientSock=socket(AF_INET,SOCK_DGRAM,0);//IPV4,UDP
        if(clientSock<0)
        {
            std::cerr<<"创建Client套接字失败,原因是"<<WSAGetLastError()<<std::endl;//perror和cerr的区别？何时需要exit?
            exit(1);
        }
        //2.设置地址
        serverAddr.sin_family=AF_INET;
        serverAddr.sin_port=htons(SERVER_PORT);
        serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//本机
        clientAddr.sin_family=AF_INET;
        clientAddr.sin_port=htons(CLIENT_PORT);
        clientAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//本机
        if(bind(clientSock,(struct sockaddr*)&clientAddr,sizeof(clientAddr))<0)
        {
            std::cerr<<"绑定Client地址失败"<<std::endl;
            exit(1);
        }
    }
    bool test_send_recv(const std::string& message)//客户端向服务器端发送一条消息，然后监听服务器端一条消息
    {
        if(sendto(clientSock,message.c_str(),message.size(),0,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0)
        {
            std::cerr<<"发送数据包失败"<<std::endl;
            return 0;
        }
        std::cout<<"成功发送消息："<<message<<std::endl;
        //接收数据包
        char buffer[BUFFER_SIZE]={0};
        int addr_len = sizeof(serverAddr);
        if(recvfrom(clientSock,buffer,sizeof(buffer),0,(struct sockaddr*)&serverAddr,&addr_len)<0)
        {
            std::cerr<<"接收数据包失败"<<std::endl;
            return 0;
        }
        std::cout<<"成功接收消息："<<buffer<<std::endl;
        return 1;
    }
    ~Client(){closesocket(clientSock);}

};

class Server
{
private:
    int serverSock;
    sockaddr_in serverAddr;
    sockaddr_in clientAddr;

public:
    Server()
    {   //1.创建套接字
        serverSock=socket(AF_INET,SOCK_DGRAM,0);//IPV4,UDP
        if(serverSock<0)
        {
            std::cerr<<"创建Server套接字失败,原因是"<<WSAGetLastError()<<std::endl;//perror和cerr的区别？何时需要exit?
            exit(1);
        }
        //2.设置地址
        serverAddr.sin_family=AF_INET;
        serverAddr.sin_port=htons(SERVER_PORT);
        serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//本机
        clientAddr.sin_family=AF_INET;
        clientAddr.sin_port=htons(CLIENT_PORT);
        clientAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//本机
        //3.绑定服务器地址
        if(bind(serverSock,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0)
        {
            std::cerr<<"绑定Server地址失败"<<std::endl;
            exit(1);
        }
    }
    void test_send_recv()
    {
        while(1)//一直等待客户端消息，如果收到了消息，就发送ACK
        {
             //接收数据包
            char buffer[BUFFER_SIZE]={0};
            int addr_len = sizeof(clientAddr);
            if(recvfrom(serverSock,buffer,sizeof(buffer),0,(struct sockaddr*)&clientAddr,&addr_len)<0)
            {
                 std::cerr<<"接收数据包失败,原因是"<<WSAGetLastError()<<std::endl;
                 continue;
            }
            std::cout<<"成功接收消息："<<buffer<<std::endl;
            std::string message="ACK";
            if(sendto(serverSock,message.c_str(),message.size(),0,(struct sockaddr*)&clientAddr,sizeof(clientAddr))<0)
            {
                std::cerr<<"发送数据包失败"<<std::endl;
                continue;
            }
            std::cout<<"成功发送消息："<<message<<std::endl;
        }
       
    }
    ~Server(){closesocket(serverSock);}

};

int main(int argc,char* argv[])
{
    if(argc!=2)
    {
        std::cerr<<"用法：./exefile Server或./exefile Client"<<std::endl;
        return 1;
    }
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)//指定请求的 Winsock 版本为 2.2。
    {
        std::cerr << "载入socket库失败" << std::endl;
    }
    std::string role=argv[1];
    if(role=="Server")
    {
        Server server;
        server.test_send_recv();
    }
    else if(role=="Client")
    {
        Client client;
        client.test_send_recv("你好");
    }
    else
    {
        std::cerr << "只能是Server/Client" << std::endl;
    }
    WSACleanup();
    return 0;
}

