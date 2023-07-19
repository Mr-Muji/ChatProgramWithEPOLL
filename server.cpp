#include "server.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h> //sockaddr_in
#include <sys/socket.h> //sockaddr
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <queue>

// broadcasting 함수는 메시지를 보낸 클라이언트를 제외한 모든 클라이언트의 이벤트를 EPOLLOUT으로 바꿔주고, 모든 send_queues에 클라이언트별로 각각 메시지를 저장해 준다.
void Server::broadcasting(int send_socket, int epoll_fd, const char *msg)
{
    struct epoll_event ev;
    for (const auto &client : nicknames) // 모든 클라이언트 순회
    {
        if (client.first != send_socket)
        {
            send_queues[client.first].push(msg);
            // EPOLLOUT 가능하도록 이벤트 상태 변경
            ev.events = EPOLLOUT | EPOLLIN;
            ev.data.fd = client.first;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client.first, &ev);
        }
    }
}

void Server::run()
{                                                        
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0); // 소켓 생성. 성공하면 파일 기술자 리턴.
    if (listen_socket == -1)
    {
        std::cerr << "Error opening socket" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (fcntl(listen_socket, F_SETFL, O_NONBLOCK) == -1) // 소켓을 논블로킹으로 설정
    {
        std::cerr << "Error setting socket to non-blocking" << std::endl;
        exit(EXIT_FAILURE);
    }

    // AF_INET의 소켓 도메인을 사용한다면 sockaddr_in 구조체를 사용해야 함. bind할 때만 sockaddr 구조체로 캐스팅
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    // 아래의 주소와 포트는 네트워크 바이트 순서인 빅 엔디안으로 저장되어야 함
    server_addr.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY는 모든 ip주소를 의미
    server_addr.sin_port = htons(8080); // host to network short. 2바이트 short형 데이터를 호스트 바이트 순서에서 네트워크 바이트 순서로 변환.

    // bind 작업. 빈 소켓이 시스템 장치와 통신할 수 있도록 이름 부여
    if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        std::cerr << "Error binding socket" << std::endl;
        exit(EXIT_FAILURE);
    };

    printf("bind 완료\n");

    // listen, 연결 요청 대기 큐 생성
    if (listen(listen_socket, SOMAXCONN) == -1)
    { // SOMAXCONN은 net.core.somaxconn 커널 파라미터의 값. 이 값은 시스템에 따라 다름. 대부분 128
        std::cerr << "Error listening socket" << std::endl;
        exit(EXIT_FAILURE);
    }
    printf("listen 완료\n");

    // epoll 생성.
    int epollfd = epoll_create1(0);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_socket;
    // epoll에 소켓 등록
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_socket, &ev);

    struct epoll_event events[MAX_EVENTS];
    int conn_sock, nfds;

    while (1)
    {
        // nfds : number of file descriptors
        printf("epoll_wait 시작\n");
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        printf("%d\n", events->events);
        if (nfds == -1)
        {
            std::cerr << "Error epoll_wait" << std::endl;
            exit(EXIT_FAILURE);
        }
        printf("epoll_wait 완료\n");

        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == listen_socket) // 리스닝 소캣인 경우
            {
                // accept의 두번째와 세번째 인수를 위해 주소 구조체 선언
                struct sockaddr_in client_addr = {};
                socklen_t client_addr_size = sizeof(client_addr);

                // accept
                conn_sock = accept(listen_socket, (struct sockaddr *)&client_addr, &client_addr_size);
                if (conn_sock == -1 && errno != EAGAIN) // EAGAIN은 논블로킹 소켓에서 더 이상 읽을 데이터가 없을 때(백로그가 비어 있는 경우) 리턴되는 에러 코드
                {
                    std::cerr << "Error accept" << std::endl;
                    exit(EXIT_FAILURE);
                }

                ev.events = EPOLLIN; // 엣지 트리거 제거
                ev.data.fd = conn_sock;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev);
            }
            else if (events[i].events & EPOLLIN)
            {
                printf("EPOLLIN\n");
                char buf[1024] = {};
                int recv_bytes = recv(events[i].data.fd, buf, sizeof(buf), 0);
                if (recv_bytes > 0)
                {
                    // 닉네임이 비어있으면 닉네임 설정
                    if (nicknames[events[i].data.fd].empty())
                    {
                        printf("닉네임 설정\n");
                        nicknames[events[i].data.fd] = std::string(buf, recv_bytes);
                        std::string welcome = nicknames[events[i].data.fd] + "님이 입장하셨습니다.\n";
                        printf("%s\n", welcome.c_str());
                        broadcasting(events[i].data.fd, epollfd, welcome.c_str());
                    }
                    else
                    {
                        std::string msg = nicknames[events[i].data.fd] + " : " + std::string(buf, recv_bytes); // 닉네임 + 입력한 메세지를 합쳐서 msg에 저장.
                        broadcasting(events[i].data.fd, epollfd, msg.c_str());
                    }
                }
                else
                { // 클라이언트 퇴장 시 메세지 출력, epoll에서 제거, 소켓 닫기, 맵에서 제거
                    // TCP는 연결 지향형이므로, 연결이 끊어지면 recv의 리턴값이 0이 됨
                    std::string msg = nicknames[events[i].data.fd] + " 님이 퇴장하셨습니다\n";
                    broadcasting(events[i].data.fd, epollfd, msg.c_str());
                    close(events[i].data.fd); // 소켓 닫기
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                    nicknames.erase(events[i].data.fd);
                }
            }

            else if (events[i].events & EPOLLOUT)
            {
                printf("EPOLLOUT\n");
                // int client_fd = events[i].data.fd; // 편하게 볼려고 변수명 변경
                if (!send_queues[events[i].data.fd].empty())
                {
                    std::string msg = send_queues[events[i].data.fd].front(); // 큐의 맨 앞에 있는 메시지를 가져옴
                    printf("%s\n", msg.c_str());                              // 클라이언트에게 보내질 메세지를 서버에서 출력해보기
                    printf("클라이언트들에게 메세지 전송\n");
                    send(events[i].data.fd, msg.c_str(), msg.size(), 0);
                    send_queues[events[i].data.fd].pop();//전송 완료시 삭제
                }
                else
                {
                    // EPOLLIN으로 이벤트 상태 변경
                    ev.events = EPOLLIN;
                    ev.data.fd = events[i].data.fd;
                    epoll_ctl(epollfd, EPOLL_CTL_MOD, events[i].data.fd, &ev);
                }
            }
        }
    }
}

int main()
{
    Server server;
    server.run();
    return 0;
}