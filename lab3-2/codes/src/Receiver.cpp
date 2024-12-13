#include "../include/Receiver.h"
#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include<thread>
#include<chrono>


void Receiver::sendPacket(Datagram& packet) {
        packet.checksum = packet.calChecksum(serverAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr);
        sendto(sock, reinterpret_cast<const char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&routerAddr, sizeof(routerAddr));
    }

bool Receiver::receivePacket(Datagram& packet) {
    socklen_t len = sizeof(routerAddr);
    return recvfrom(sock, reinterpret_cast<char*>(&packet), sizeof(packet), 0, (struct sockaddr*)&routerAddr, &len) > 0 && packet.validateChecksum(serverAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr);
}

bool Receiver::setupConnection() {
    Datagram synPacket(CLIENT_PORT,ROUTER_PORT);
    if (receivePacket(synPacket) && synPacket.flag == 1) { // SYN
        std::cout << "收到SYN包\n";
        Datagram synAckPacket(SERVER_PORT,ROUTER_PORT);
        synAckPacket.flag = 2; // SYN-ACK
        Datagram ackPacket(CLIENT_PORT,ROUTER_PORT);

        while(true)
        {
            std::thread ackThread(&Receiver::receivePacket,this, std::ref(ackPacket));
            //std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿
            ackThread.detach();//修改

            sendPacket(synAckPacket);
            std::cout << "发送SYN-ACK包\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); // 超时等待重传
            if (ackPacket.validateChecksum(serverAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr)&&ackPacket.flag == 3) { // ACK
                std::cout << "收到ACK包,连接建立成功\n";
                return true;
            }
        }
    }
    return false;
}


Receiver::Receiver() {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "创建Receiver套接字失败" << std::endl;
        exit(1);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    routerAddr.sin_family = AF_INET;
    routerAddr.sin_port = htons(ROUTER_PORT);
    routerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    if (bind(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "绑定Receiver地址失败:" <<WSAGetLastError()<<std::endl;
        exit(1);
    }
}

void Receiver::receiveFile(const std::string& filename) {
    if (!setupConnection()) {
        std::cerr << "连接建立失败\n";
        return;
    }

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "无法创建文件：" << filename << std::endl;
        return;
    }

    std::unordered_map<int, Datagram> buffer; // 缓存乱序到达的包
    int expectedSeq = 0;
    int lastAckSent = -1;

    while (true) {
        Datagram packet(CLIENT_PORT, ROUTER_PORT);
        if (!receivePacket(packet)) {
            std::cerr << "接收数据包失败\n";
            continue;
        }

        if (packet.flag == 4) { // FIN
            std::cout << "收到FIN包，开始断开连接\n";
            Datagram finAckPacket(SERVER_PORT,ROUTER_PORT);
            finAckPacket.flag = 5; // FIN-ACK

            Datagram ackPacket(CLIENT_PORT,ROUTER_PORT);
            std::thread ackThread(&Receiver::receivePacket,this, std::ref(ackPacket));
            //std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿
            ackThread.detach();//修改
            while(true)
            {
                sendPacket(finAckPacket);
                std::cout << "发送FIN-ACK包\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); // 超时等待重传
                std::thread ackThread(&Receiver::receivePacket,this, std::ref(ackPacket));
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT)); //休眠等一会儿
                ackThread.detach();//修改
                if (ackPacket.validateChecksum(serverAddr.sin_addr.S_un.S_addr, routerAddr.sin_addr.S_un.S_addr)&&ackPacket.flag == 3) { // ACK
                    std::cout << "收到ACK包，连接断开成功\n";
                    break;
                }
            }
            break;
        } else if (packet.seq == expectedSeq) {
            // 按序到达的包
            std::cout << "收到数据包，序列号：" << packet.seq << ", 校验码" << packet.checksum << std::endl;
            file.write(packet.data, packet.dataSize);

            expectedSeq++;

            // 检查缓存中是否有后续的包
            while (buffer.find(expectedSeq) != buffer.end()) {
                const auto& cachedPacket = buffer[expectedSeq];
                file.write(cachedPacket.data, cachedPacket.dataSize);
                buffer.erase(expectedSeq);
                expectedSeq++;
            }

            // 发送新的ACK，表示此序号前的所有包都已按序接收
            Datagram ackPacket(SERVER_PORT, ROUTER_PORT);
            ackPacket.ack = expectedSeq - 1;
            ackPacket.flag = 0; // 数据包的ACK
            sendPacket(ackPacket);
            lastAckSent = ackPacket.ack;
            std::cout << "发送ACK, ack=" << ackPacket.ack << std::endl;
        } else if (packet.seq > expectedSeq) {
            // 序列号大于期望的，缓存它
            std::cout << "收到乱序数据包，序列号：" << packet.seq << "，期待序列号：" << expectedSeq << std::endl;
            buffer[packet.seq] = packet;

            // 发送最后一次按顺序接收到的包的ACK
            Datagram ackPacket(SERVER_PORT, ROUTER_PORT);
            ackPacket.ack = lastAckSent;
            //ackPacket.ack=packet.seq;//修改成，收到啥发送啥
            ackPacket.flag = 0; // 重复的ACK
            sendPacket(ackPacket);
            std::cout << "发送重复ACK, ack=" << ackPacket.ack << std::endl;
        } else {
            // 序列号小于期望的，说明是重复的包，重新发送最后一次的ACK
            std::cout << "收到重复数据包，序列号：" << packet.seq << std::endl;
            Datagram ackPacket(SERVER_PORT, ROUTER_PORT);
            ackPacket.ack = lastAckSent;
            ackPacket.flag = 0; // 重复的ACK
            sendPacket(ackPacket);
            std::cout << "发送重复ACK, ack=" << ackPacket.ack << std::endl;
            
        }
    }

    file.close();
    std::cout << "文件接收并保存完成：" << filename << std::endl;
}
Receiver::~Receiver() { closesocket(sock); }