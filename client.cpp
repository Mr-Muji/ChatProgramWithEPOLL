#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <fcntl.h>

// 소켓에서 데이터를 수신.
void receive_data(int sockfd)
{
    char buf[1024];
    while (1)
    {
        memset(buf, 0, sizeof(buf)); // buf를 0으로 초기화
        if (recv(sockfd, buf, 1024, 0) == -1)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                std::cerr << "Error receiving data" << std::endl;
                break;
            }
        }
        if (strlen(buf) > 0)
        {
            printf("%s\n", buf);
        }
    }
}

int Client::connect_to_server()
{
    // 먼저 소켓을 생성해 놓아야 함
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // connect의 인수로 사용할 주소 구조체 생성
    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str()); // inet_addr는 주어진 문자열에서 ip주소를 찾고, 이를 네트워크 바이트 순서로 변환해 리턴.

    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) // 소켓을 논블로킹으로 설정
    {
        std::cerr << "Error setting socket to non-blocking" << std::endl;
        return 1;
    }

    // if(connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
    //     std::cerr << "Error connecting to server" << std::endl;
    //     return 1;
    // }

    // 넌블록킹으로 커넥트. 리턴값을 취하지 않음
    (void)connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    char nickname[1024];
    printf("닉네임 입력 : ");
    fgets(nickname, 1024, stdin);
    nickname[strcspn(nickname, "\n")] = 0; // fgets는 개행문자를 포함해서 읽어오므로, 개행문자를 널문자로 바꿔줌
    // send
    send(sockfd, nickname, strlen(nickname), 0);
    printf("메세지를 입력하시면 됩니다.\n");

    // 새로운 스레드를 만들어서 데이터를 수신하도록 함
    std::thread t(receive_data, sockfd);
    t.detach();

    char buf[1024];
    while (1)
    {
        memset(buf, 0, sizeof(buf)); // buf를 0으로 초기화
        //printf("메세지 입력 : ");
        fgets(buf, 1024, stdin);
        buf[strcspn(buf, "\n")] = 0;
        send(sockfd, buf, strlen(buf), 0);
        
    }
    t.join();
    close(sockfd);

    return 0;
}

void main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <server-ip> <server-port>\n", argv[0]);
        exit(1);
    }
    std::string server_ip = argv[1];
    int server_port = atoi(argv[2]);
    Client client(server_ip, server_port);
    client.connect_to_server();
}