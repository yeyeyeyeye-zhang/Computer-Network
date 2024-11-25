#include<iostream>
#include <Winsock2.h>
#include<string>
#include<cstring>
#include<string.h>
#include <fstream> // 包含该行以使用 std::ifstream

#define SERVER_PORT 8886
#define CLIENT_PORT 9996
#define BUFFER_SIZE 1024

struct Datagram
{
    int dataSize;//当前数据大小
    char data[BUFFER_SIZE];//数据
};

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
   
    void sendFile(const std::string& read_file_name)
    {
        //1.定义输入文件流( std::ifstream)对象file,可以使用各种方法来读取文件内容。
        std::ifstream file(read_file_name,std::ios::binary);
        //filename指定了要打开的文件的名称或路径
        //std::ios::binary表示以二进制模式打开文件（可以确保文件按字节读取，特别适合非文本文件，如图像、音频等）

        //2.判断文件是否正常打开
        if(!file.is_open())
        {
            std::cerr<<"无法打开文件："<<read_file_name<<std::endl;
            return;
        }
        std::cout<<"开始发送文件"<<std::endl;
        
        //3.一部分一部分地发送文件
        while(!file.eof())//如果没有抵达文件末尾，就循环发送
        {
            Datagram packet;
            //从文件中读取数据
            file.read(packet.data,BUFFER_SIZE);
            //data:目标数组，读取的内容将存放在这里
            //`BUFFER_SIZE`：要读取的字节数。`read`会尽量从文件中读取 `BUFFER_SIZE` 字节的数据，并将其写入 `data` 数组。
            //(如果文件中剩余字节数小于BUFFER_SIZE，能读多少算多少)
            packet.dataSize=file.gcount();//返回上一个成功的读操作实际读取的字节数。(<=BUFFER_SIZE)
            
            if(sendto(clientSock, (char*)(&packet),sizeof(packet),0,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0)
            {
                std::cerr<<"发送数据包失败"<<std::endl;
            }
            else
            {
                std::cout<<"成功发送"<<packet.dataSize<<"字节"<<std::endl;
            }
        }
      
        std::cout<<"文件发送完毕"<<std::endl;
        //4.关闭文件
        file.close();


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
   
    void receiveFile(const std::string& write_file_name)
    {
        //1.定义输出文件流( std::ofstream)对象file,可以使用各种方法来写入文件内容。
        std::ofstream file(write_file_name,std::ios::binary);
        //write_file_name指定了要打开的文件的名称或路径
        //std::ios::binary表示以二进制模式打开文件（可以确保文件按字节写入，特别适合非文本文件，如图像、音频等）

        //2.判断文件是否正常打开
        if(!file.is_open())
        {
            std::cerr<<"无法创建文件："<<write_file_name<<std::endl;
            return;
        }
        std::cout<<"开始接收文件"<<std::endl;
        int len=sizeof(serverAddr);
        //3.一部分一部分地接收文件
        while(true)//一直等待接收，直到收到"end"结束
        {
            Datagram packet;
            if(recvfrom(serverSock,(char*)&packet,sizeof(packet),0,(struct sockaddr*)&serverAddr,&len)<0)
            {
                std::cerr<<"接收数据包失败"<<std::endl;
                continue;//继续等待下一次消息
            }
            else if(packet.dataSize<BUFFER_SIZE)//结束条件
            {
                break;
            }
            else//接收成功，写入文件
            {
                file.write(packet.data, packet.dataSize);
            }

        }
        
        std::cout<<"文件接收完毕"<<std::endl;
        //4.关闭文件
        file.close();

    }
    ~Server(){closesocket(serverSock);}

};


int main(int argc,char* argv[])
{
   

    if(argc!=3)
    {
        std::cerr<<"用法：./exefile Server filename或./exefile Client filename"<<std::endl;
        return 1;
    }
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)//指定请求的 Winsock 版本为 2.2。
    {
        std::cerr << "载入socket库失败" << std::endl;
    }
    std::string role=argv[1];
    std::string filename=argv[2];
    if(role=="Server")
    {
        Server server;
        server.receiveFile(filename);
    }
    else if(role=="Client")
    {
        Client client;
        client.sendFile(filename);
    }
    else
    {
        std::cerr << "只能是Server/Client" << std::endl;
    }
    WSACleanup();
    return 0;
}

