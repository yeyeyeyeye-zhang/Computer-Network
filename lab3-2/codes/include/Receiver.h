#ifndef RECEIVER_H
#define RECEIVER_H

#include "Datagram.h"
#include <string>
#include <WinSock2.h>
#include <atomic>
#include <unordered_map>
#include <vector>

class Receiver {
public:
    Receiver();
    ~Receiver();
    void receiveFile(const std::string& filename);

private:
    int sock;
    struct sockaddr_in serverAddr;
    struct sockaddr_in routerAddr;
    void sendPacket(Datagram& packet);
    bool receivePacket(Datagram& packet);
    bool setupConnection();
};

#endif // RECEIVER_H