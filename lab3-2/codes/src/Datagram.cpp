#include "../include/Datagram.h"
#include <Winsock2.h>
#include <ws2tcpip.h> // 包含 inet_pton 的定义
#include<vector>

unsigned short calculateChecksum(const void *buf, int length) {
    unsigned long sum = 0;
    const unsigned short *data = static_cast<const unsigned short *>(buf);

    while (length > 1) {
        //sum += *data++;
        unsigned short value = *data++;
        sum += ~value; // 反码运算后再加到sum上
        if (sum & 0x80000000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        length -= 2;
    }

    if (length) {
        sum += *reinterpret_cast<const unsigned char *>(data);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<unsigned short>(~sum);
}


// 实现Datagram构造函数及其方法
Datagram::Datagram(int src_port_, int dst_port_)
    : src_port(src_port_), dst_port(dst_port_), seq(-1), ack(-1), checksum(0), dataSize(0) {
    memset(data, 0, BUFFER_SIZE);
}
Datagram::Datagram()
    : src_port(CLIENT_PORT), dst_port(ROUTER_PORT), seq(-1), ack(-1), checksum(0), dataSize(0) {
    memset(data, 0, BUFFER_SIZE);
}
int Datagram::calChecksum(uint32_t srcAddr, uint32_t destAddr) {
    PseudoHeader pHeader = {srcAddr, destAddr, 0, IPPROTO_UDP, htons(sizeof(Datagram) + dataSize)};
    int tmp = this->checksum;
    this->checksum = 0;

    int totalLen = sizeof(PseudoHeader) + sizeof(Datagram);
    std::vector<unsigned char> buffer(totalLen);
    memcpy(buffer.data(), &pHeader, sizeof(PseudoHeader));
    memcpy(buffer.data() + sizeof(PseudoHeader), this, sizeof(Datagram));

    this->checksum = tmp;
    return calculateChecksum(buffer.data(), totalLen);
}

bool Datagram::validateChecksum(uint32_t srcAddr, uint32_t destAddr) {
    return calChecksum(srcAddr, destAddr) == checksum;
}

