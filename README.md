# Computer-Network
An independent programming exploration based on the Computer Network course of Nankai University for the Class of 2022. Mastering socket programming and other network programming from scratch
基于南开大学2022届计算机网络课程，展开的自主编程探索。从0到1掌握socket编程及其他网络编程。

## FOLDER INTRODUCTION文件夹介绍

### 1.basic：

 Organize and utilize UDP for packet transmission, implement file transfer, and simulate the TCP connection process in the code. 整理利用UDP实现数据包传输、并实现文件传输、模拟TCP连接过程的代码。

### 2.lab3-1:

 Implement reliable data transmission with connection-oriented datagram sockets, including functions such as: establishing connections, error detection, acknowledgment and retransmission, etc. Flow control uses a stop-and-wait mechanism. 实现数据报套接字面向连接的可靠数据传输，功能包括：建立连接、差错检 测、确认重传等。流量控制采用停等机制。

### 3.lab3-2

 Based on lab3-1, change the flow control mechanism to a sliding window-based flow control and implement cumulative acknowledgment. 在lab3-1基础上，将流量机制改为基于滑动窗口的流量控制，实现累积确认。

## 代码架构

lab3 Project Root/
├── include/
│   ├── Datagram.h    // 定义 Datagram 结构体及其成员函数声明
│   ├── Sender.h      // 声明 Sender 类
│   └── Receiver.h    // 声明 Receiver 类
│
├── src/
│   ├── Datagram.cpp   // 实现 Datagram 结构体的成员函数
│   ├── Sender.cpp     // 实现 Sender 类
│   ├── Receiver.cpp   // 实现 Receiver 类
│   └── main.cpp       // 包含 main 函数，负责启动程序

## 运行命令

进入src目录下，输入：

```
 g++ -o cs main.cpp Datagram.cpp Sender.cpp Receiver.cpp -lws2_32
```

