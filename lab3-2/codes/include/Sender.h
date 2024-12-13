#ifndef SENDER_H
#define SENDER_H

#include "Datagram.h"
#include <string>
#include <WinSock2.h>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <vector>

class Sender {
public:
    Sender();
    ~Sender();
    void sendFile(const std::string& filename);

private:
    int sock;
    struct sockaddr_in routerAddr;
    struct sockaddr_in clientAddr;
    std::atomic<bool> ackReceived; // 改为原子变量
    int expectedAck;
    std::mutex mtx;
    std::condition_variable cv;

    //新增：用于滑动窗口
    //static const int WINDOW_SIZE = 4; // 设置发送窗口大小
    std::unordered_map<int, Datagram> window; // 发送窗口，用于存储已发送但未确认的数据包
    std::atomic<int> base; // 当前窗口的起始序号
    std::atomic<int> nextseq; // 下一个发送数据包的序号
    //std::vector<bool> ackwindow;//存储每轮收到的ack信息
    int getAck;//获得的ack 

    void sendPacket(Datagram& packet);
    bool waitForAck();
    void receiveAck();
    bool setupConnection();
    bool receivePacket(Datagram& packet);
    void disconnect();
};

#endif // SENDER_H