#include "../include/Sender.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <algorithm> // 包含 std::find


void Sender::sendPacket(Datagram& packet) {
        packet.checksum = packet.calChecksum(clientAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr);
        sendto(sock, reinterpret_cast<const char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&routerAddr, sizeof(routerAddr));
    }

 bool Sender::waitForAck() {
    std::unique_lock<std::mutex> lock(mtx);
    //如果 `ackReceived` 在超时之前变为 `true`，则 `wait_for` 返回并继续执行后续代码；如果超时后 `ackReceived` 仍为 `false`，则 `wait_for` 也会返回。
    return cv.wait_for(lock, std::chrono::milliseconds(5*TIMEOUT), [this]() { return ackReceived.load() ; });//超时或者收到所有ack返回
}

void Sender::receiveAck() {
    Datagram ackPacket(SERVER_PORT,ROUTER_PORT);
    socklen_t len = sizeof(routerAddr);
    while (true) {
        if (recvfrom(sock, reinterpret_cast<char*>(&ackPacket), sizeof(ackPacket), 0, (struct sockaddr*)&routerAddr, &len) > 0) {
            if (ackPacket.flag == 0 && ackPacket.validateChecksum(clientAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr)) {
                if(ackPacket.ack==65535)
                {
                    continue;
                }
                std::lock_guard<std::mutex> lock(mtx);
                std::cout << "收到ack=" << ackPacket.ack << std::endl;
                if(ackPacket.ack>getAck){getAck=ackPacket.ack;}
                if(ackPacket.ack==expectedAck)
                {
                    ackReceived = true;
                    cv.notify_one();
                }
                
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
            std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿
            return true;
        }
        
    }
    return false;
}

bool Sender::receivePacket(Datagram& packet) {
    socklen_t len = sizeof(routerAddr);
    return recvfrom(sock, reinterpret_cast<char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&routerAddr, &len) > 0 && packet.validateChecksum(clientAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr);
}


Sender::Sender() : ackReceived(false), expectedAck(0),getAck(-1) {
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
        base = 0;
        nextseq = 0;
        int no_receive_count=0;

        while (true) {

            //1.创建接收线程，避免第三次握手时ACK的丢包
            Datagram AckPacket(SERVER_PORT,ROUTER_PORT);
            if(nextseq<3)
            {
                std::thread ackThread1(&Sender::receivePacket,this, std::ref(AckPacket));
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿
                ackThread1.detach();//修改
            }
            //1.基于窗口连续发送N条消息
            while (nextseq < base + WINDOW_SIZE && !file.eof()) {
                if(nextseq<3&&AckPacket.flag == 2&&AckPacket.validateChecksum(clientAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr))//2.如果此时又收到了SYN-ACK
                {
                    std::cout << "重新收到SYN-ACK包\n";
                    Datagram ackPacket(CLIENT_PORT,ROUTER_PORT);
                    ackPacket.flag = 3; // ACK
                    sendPacket(ackPacket);
                    std::cout << "重新发送ACK包，连接建立成功\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿
                    AckPacket.flag=1;
                }
                std::unique_lock<std::mutex> lock(mtx);
                ackReceived = false;
                expectedAck = base+WINDOW_SIZE-1;
                Datagram packet;//默认构造，从客户端发往路由器端
                packet.seq = nextseq;
                file.read(packet.data, BUFFER_SIZE);
                packet.dataSize = static_cast<int>(file.gcount());
                packet.flag = 0;

                window[nextseq] = packet;
                //window.emplace(nextseq, packet);
                sendPacket(packet);
                std::cout << "发送数据包.SEQ=" << packet.seq <<"，校验码="<< packet.checksum << std::endl;
                nextseq++;
                lock.unlock();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));
            
            // 2.结束条件：如果已经读到文件末尾，则跳出
            if (base==nextseq&&file.eof()) {break;}


            if (waitForAck()) {
                std::unique_lock<std::mutex> lock(mtx);
                base = base + WINDOW_SIZE;
                //std::fill(ackwindow.begin(), ackwindow.end(), false);
                std::cout << "base窗口后移为:" << base << ",开始下一轮发送" << std::endl;
            } else {
                std::unique_lock<std::mutex> lock(mtx);
                
                // if(base==getAck)
                // {
                //     no_receive_count++;
                //     if(no_receive_count==3){break;}//连续三次没有收到任何有效ack,客户端主动断开连接
                // }
                // else{no_receive_count=0;}
                if(getAck==-1){base=0;}
                else{base =getAck+1;}
                //std::fill(ackwindow.begin(), ackwindow.end(), false);
                std::cout << "未收到所有ack,base窗口后移为:" << base << ",重新发送此部分" << std::endl;
                for (int i = base; i < nextseq; ++i) {
                    sendPacket(window[i]);
                    std::cout << "发送数据包.SEQ=" << window[i].seq << "，校验码=" << window[i].checksum << std::endl;
                }
            }
        }

        file.close();
        disconnect();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        double allDuration = duration.count();
        std::cout << "文件发送完成：" << filename << "传输时间是："<<allDuration<<"秒,吞吐率是"<<(double(nextseq*BUFFER_SIZE)/allDuration)<<"字节/s"<<std::endl;
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