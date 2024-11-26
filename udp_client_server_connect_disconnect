#include<iostream>
#include <Winsock2.h>
#include<string>
#include<cstring>
#include<string.h>
#include <fstream> // 包含该行以使用 std::ifstream
#include<thread>

#define SERVER_PORT 8886
#define CLIENT_PORT 9996
#define BUFFER_SIZE 1024
#define TIMEOUT 500 // 超时时间为0.005秒

struct Datagram
{
    int dataSize;//当前数据大小
    char data[BUFFER_SIZE];//数据
    int flag;           // 标志位：0-数据，1-SYN，2-SYN-ACK，3-ACK，4-FIN，5-FIN-ACK
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

    bool receivePacket(Datagram& packet)
    {
        int len = sizeof(serverAddr);
        if(recvfrom(clientSock, reinterpret_cast<char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&serverAddr, &len)<0)
        {
            std::cerr<<"客户端接收失败"<<std::endl;
            return false;
        }
        return true;
    }

     bool sendPacket(Datagram& packet) {
        if(sendto(clientSock, reinterpret_cast<const char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr))<0)
        {
            std::cerr<<"客户端发送失败"<<std::endl;
            return false;
        }
        return true;
    }

    bool setupConnection()
    {
        //1.监听回应：创建接收SYN-ACK的线程ackThread,调用receivePacket函数，传入synAckPacket参数
        Datagram synAckPacket;
        std::thread ackThread(&Client::receivePacket,this, std::ref(synAckPacket));
        ackThread.detach();//分离出ackThread，主线程照常运行
        
        //2.发送SYN，提出建立连接
        Datagram synPacket;
        synPacket.flag = 1; // SYN
        while(true)//如果监听线程ackThread中没有收获到理想的回复，会重复发送ACK
        {
            std::cout << "发送SYN包\n";
            sendPacket(synPacket);
            std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //超时重传设置的时长
            if (synAckPacket.flag == 2) { // 说明收到了SYN-ACK
                std::cout << "收到SYN-ACK包\n";
                //3.收到后发送ACK包，有可能会被丢包，之后判断
                Datagram ackPacket;
                ackPacket.flag = 3; // ACK
                sendPacket(ackPacket);
                std::cout << "发送ACK包,连接建立成功\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿再开始传输文件
                return true;
            }

        }
    }
   
    void sendFile(const std::string& read_file_name)
    {
        if (!setupConnection()) {//先建立连接
            std::cerr << "连接建立失败\n";
            return;
        }
        //1.定义输入文件流( std::ifstream)对象file,可以使用各种方法来读取文件内容。
        std::ifstream file(read_file_name,std::ios::binary);

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
            packet.dataSize=file.gcount();//返回上一个成功的读操作实际读取的字节数。(<=BUFFER_SIZE)
            
            if(sendPacket(packet))
            {
                std::cout<<"成功发送"<<packet.dataSize<<"字节"<<std::endl;
            }
        }
      
        std::cout<<"文件发送完毕"<<std::endl;
        //4.关闭文件
        file.close();
        //5.与服务器端脱离连接
        if(disconnect()){std::cout << "脱离连接成功\n";}

    }

    bool disconnect() {
        Datagram finAckPacket;
        std::thread finAckThread(&Client::receivePacket,this, std::ref(finAckPacket));
        finAckThread.detach();

        Datagram finPacket;
        finPacket.flag = 4; // FIN
        while (true) {
            sendPacket(finPacket);
            std::cout << "发送FIN包\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); // 超时等待重传
            if (finAckPacket.flag == 5) { // FIN-ACK
                std::cout << "收到FIN-ACK包\n";
                Datagram ackPacket;
                ackPacket.flag = 3; // ACK
                sendPacket(ackPacket);
                std::cout << "发送ACK包\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));//休眠等一会
                break;
            }
        }
        return true;
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

    bool receivePacket(Datagram& packet)
    {
        int len = sizeof(clientAddr);
        if(recvfrom(serverSock, reinterpret_cast<char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&clientAddr, &len)<0)
        {
            std::cerr<<"服务器端接收失败"<<std::endl;
            return false;
        }
        return true;
    }
    void sendPacket(Datagram& packet) {
        if(sendto(serverSock, reinterpret_cast<const char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr))<0)
        {
            std::cerr<<"服务器端发送失败"<<std::endl;
        }
    }

    bool setupConnection()
    {
        //1.等待客户端发来连接请求（即SYN包）
        Datagram synPacket;
        if (receivePacket(synPacket) && synPacket.flag == 1)
        {
            std::cout << "收到SYN包\n";
            //2.启动监听线程
            Datagram ackPacket;
            std::thread ackThread(&Server::receivePacket,this, std::ref(ackPacket));
            ackThread.detach();

            //3.发送SYN-ACK包（设置了超时重传）
            Datagram synAckPacket;
            synAckPacket.flag = 2; // SYN-ACK
            while(true)
            {
                sendPacket(synAckPacket);
                std::cout << "发送SYN-ACK包\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); // 超时等待重传
                if (ackPacket.flag == 3) { // ACK
                    std::cout << "收到ACK包，连接建立成功\n";
                    break;
                }

            }
            return true;
        }
        return false;
    }

    bool disconnect() {
        Datagram finPacket;
        //1.收到FIN
       if(receivePacket(finPacket)&&finPacket.flag==4)//FIN
       {
            std::cout<<"收到FIN\n";
            //2.启动监听ACK的线程
            Datagram ackPacket;
            std::thread ackThread(&Server::receivePacket,this, std::ref(ackPacket));
            ackThread.detach();

            //3.发送FIN_ACK(支持超时重传)
            Datagram finAckPacket;
            finAckPacket.flag = 5; // FIN-ACK
            while(true)
                {
                    sendPacket(finAckPacket);
                    std::cout << "发送FIN-ACK包\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); // 超时等待重传
                    if (ackPacket.flag == 3) { // ACK
                        std::cout << "收到ACK包，连接断开成功\n";
                        std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));
                        break;
                    }
                }
       }
       return true;
    }
   
    void receiveFile(const std::string& write_file_name)
    {
        if (!setupConnection()) {//先建立连接
            std::cerr << "连接建立失败\n";
            return;
        }
        //1.定义输出文件流( std::ofstream)对象file,可以使用各种方法来写入文件内容。
        std::ofstream file(write_file_name,std::ios::binary);

        //2.判断文件是否正常打开
        if(!file.is_open())
        {
            std::cerr<<"无法创建文件："<<write_file_name<<std::endl;
            return;
        }
        std::cout<<"开始接收文件"<<std::endl;
        int len=sizeof(serverAddr);
        //3.一部分一部分地接收文件
        while(true)
        {
            Datagram packet;
            if(!receivePacket(packet))//没有接收到
            {
                continue;//继续等待下一次消息
            }
            else if (packet.flag == 4) { // 收到FIN(文件传输完毕的标志)
                std::cout << "收到FIN包，开始断开连接\n";
                //1.监听ACK
                Datagram ackPacket;
                std::thread ackThread(&Server::receivePacket,this, std::ref(ackPacket));
                ackThread.detach();//修改
                //2.发送FIN_ACK
                Datagram finAckPacket;
                finAckPacket.flag = 5; // FIN-ACK
                while(true)
                {
                    sendPacket(finAckPacket);
                    std::cout << "发送FIN-ACK包\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(2*TIMEOUT)); // 超时等待重传
                    if (ackPacket.flag == 3) { // 收到ACK
                        std::cout << "收到ACK包，连接断开成功\n";
                        break;//跳出FIN_ACK的超时重传
                    }
                }
                break;//跳出文件接收
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

