#include <iostream>
#include <string>
#include <chrono>
#include <WinSock2.h>
#include "../include/Sender.h"
#include "../include/Receiver.h"
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "用法" << argv[0] << "<role><filename>" << std::endl;
        std::cerr << "角色：sender|receiver" << std::endl;
        return 1;
    }

    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
        std::cerr << "载入socket库失败" << std::endl;
    }

    std::string role = argv[1];
    std::string filename = argv[2];

    if (role == "sender") {
        Sender sender;
        
        sender.sendFile("../../testfiles/"+filename);
    } else if (role == "receiver") {
        Receiver receiver;
        receiver.receiveFile("../../receivefiles/"+filename);
    } else {
        std::cerr << "未知角色：" << role << std::endl;
        WSACleanup();
        return 1;
    }

    WSACleanup();
    return 0;
}