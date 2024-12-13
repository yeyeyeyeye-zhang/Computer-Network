#include "../include/Sender.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <WinSock2.h>
#include <ws2tcpip.h>



void Sender::sendPacket(Datagram& packet) {
        packet.checksum = packet.calChecksum(clientAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr);
        sendto(sock, reinterpret_cast<const char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&routerAddr, sizeof(routerAddr));
    }

bool Sender::waitForAck(int seqNum) {
    std::unique_lock<std::mutex> lock(mtx);
    return cv.wait_for(lock, std::chrono::milliseconds(TIMEOUT), [this, seqNum]() { return ackReceived && expectedAck == seqNum; });
}

void Sender::receiveAck() {
    Datagram ackPacket(SERVER_PORT,ROUTER_PORT);
    socklen_t len = sizeof(routerAddr);
    while (true) {
        if (recvfrom(sock, reinterpret_cast<char*>(&ackPacket), sizeof(ackPacket), 0, (struct sockaddr*)&routerAddr, &len) > 0) {
            if (ackPacket.validateChecksum(clientAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr) && ackPacket.ack == expectedAck) {
                std::lock_guard<std::mutex> lock(mtx);
                std::cout<<"收到ACK，ack="<<ackPacket.ack<<std::endl;
                ackReceived = true;
                cv.notify_one();
            }
        }
    }
}

bool Sender::setupConnection() {
    //1.创建接收SYN-ACK的线程
    Datagram synAckPacket(SERVER_PORT,ROUTER_PORT);
    std::thread ackThread(&Sender::receivePacket,this, std::ref(synAckPacket));
    ackThread.detach();

    Datagram synPacket(CLIENT_PORT,ROUTER_PORT);
    synPacket.flag = 1; // SYN
    while (true) {//2.如果没收到消息，会循环发送SYN包
        sendPacket(synPacket);
        std::cout << "发送SYN包\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿
        //std::cout << "接收完毕\n";
        if (synAckPacket.validateChecksum(clientAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr)&&synAckPacket.flag == 2) { // SYN-ACK
            std::cout << "收到SYN-ACK包\n";
            //3.收到后发送ACK包，有可能会被丢包，之后判断
            Datagram ackPacket(CLIENT_PORT,ROUTER_PORT);
            ackPacket.flag = 3; // ACK
            sendPacket(ackPacket);
            std::cout << "发送ACK包,连接建立成功\n";
            return true;
        }
        
    }
    return false;
}

bool Sender::receivePacket(Datagram& packet) {
    socklen_t len = sizeof(routerAddr);
    return recvfrom(sock, reinterpret_cast<char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&routerAddr, &len) > 0 && packet.validateChecksum(clientAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr);
}


Sender::Sender() : ackReceived(false), expectedAck(0) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "创建Sender套接字失败, 原因是" << WSAGetLastError() << std::endl;
        exit(1);
    }

    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(CLIENT_PORT);
    clientAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    if (bind(sock, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0) {
        std::cerr << "绑定Sender地址失败" << std::endl;
        exit(1);
    }

    routerAddr.sin_family = AF_INET;
    routerAddr.sin_port = htons(ROUTER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &routerAddr.sin_addr);

    std::thread(&Sender::receiveAck, this).detach(); // 启动ACK监听线程
    
}

void Sender::sendFile(const std::string& filename) {
    if (!setupConnection()) {
        std::cerr << "连接建立失败\n";
        return;
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "无法打开文件：" << filename << std::endl;
        return;
    }
    auto start = std::chrono::high_resolution_clock::now();
    int seqNum = 0;
    while (!file.eof()) {
        Datagram packet(CLIENT_PORT,ROUTER_PORT);
        packet.seq = seqNum;
        file.read(packet.data, BUFFER_SIZE);
        packet.dataSize = static_cast<int>(file.gcount());
        packet.flag = 0; // 数据包

        ackReceived = false;
        expectedAck = seqNum;

        Datagram AckPacket(SERVER_PORT,ROUTER_PORT);
        while (true) {
            //1.创建接收线程，避免第三次握手时ACK的丢包
            if(seqNum<3)
            {
                std::thread ackThread1(&Sender::receivePacket,this, std::ref(AckPacket));
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿
                ackThread1.detach();//修改
            }
            if(AckPacket.flag == 2&&seqNum<3&&AckPacket.validateChecksum(clientAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr))//2.如果此时又收到了SYN-ACK
            {
                std::cout << "重新收到SYN-ACK包\n";
                Datagram ackPacket(CLIENT_PORT,ROUTER_PORT);
                ackPacket.flag = 3; // ACK
                sendPacket(ackPacket);
                std::cout << "重新发送ACK包，连接建立成功\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿
                AckPacket.flag=1;
            }
            sendPacket(packet);
            std::cout << "发送数据包.SEQ=" << packet.seq <<"，校验码="<< packet.checksum<<std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(5*TIMEOUT)); //休眠等一会儿
            if (waitForAck(seqNum)) {
                break; // 收到ACK，跳出重传循环
            }
            
            std::cout << "ACK超时,重传数据包,SEQ=" << packet.seq << std::endl;
        }
        seqNum++;
    }

    file.close();
    disconnect();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    double allDuration = duration.count();
    std::cout << "文件发送完成：" << filename << "传输时间是："<<allDuration<<"秒,吞吐率是"<<(double(seqNum*BUFFER_SIZE)/allDuration)<<"字节/s"<<std::endl;
}

void Sender::disconnect() {
    Datagram finPacket(CLIENT_PORT,ROUTER_PORT);
    finPacket.flag = 4; // FIN
    while (true) {
        sendPacket(finPacket);
        std::cout << "发送FIN包\n";
        Datagram finAckPacket(SERVER_PORT,ROUTER_PORT);
        std::thread finAckThread(&Sender::receivePacket,this, std::ref(finAckPacket));
        finAckThread.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(2*TIMEOUT)); // 超时等待重传
        if (finAckPacket.flag == 5) { // FIN-ACK
            std::cout << "收到FIN-ACK包\n";
            Datagram ackPacket(CLIENT_PORT,ROUTER_PORT);
            ackPacket.flag = 3; // ACK
            sendPacket(ackPacket);
            //监听，避免对方没收到ack
            Datagram finAckPacket2(SERVER_PORT,ROUTER_PORT);
            std::thread finAckThread(&Sender::receivePacket,this, std::ref(finAckPacket2));
            finAckThread.detach();
            std::this_thread::sleep_for(std::chrono::milliseconds(2*TIMEOUT));
            if(finAckPacket2.flag == 5){sendPacket(ackPacket);}
            //sendPacket(ackPacket);
            std::cout << "发送ACK包\n";
            break;
        }
    }
    std::cout << "连接断开成功\n";
}

Sender::~Sender() { closesocket(sock); }