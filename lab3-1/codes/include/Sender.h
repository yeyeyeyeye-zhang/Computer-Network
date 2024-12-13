#ifndef SENDER_H
#define SENDER_H

#include "Datagram.h"
#include <string>
#include <WinSock2.h>
#include <mutex>
#include <condition_variable>

class Sender {
public:
    Sender();
    ~Sender();
    void sendFile(const std::string& filename);

private:
    int sock;
    struct sockaddr_in routerAddr;
    struct sockaddr_in clientAddr;
    bool ackReceived;
    int expectedAck;
    std::mutex mtx;
    std::condition_variable cv;

    void sendPacket(Datagram& packet);
    bool waitForAck(int seqNum);
    void receiveAck();
    bool setupConnection();
    bool receivePacket(Datagram& packet);
    void disconnect();
};

#endif // SENDER_H