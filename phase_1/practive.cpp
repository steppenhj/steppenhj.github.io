#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>

struct ControlData{
    float throttle;
    float steering;
};


int main(){
    // 1. UDP 소켓 생성
    int sockfd = sockfd(AF_INET,  , 0);  //UDP타입

    // 2. 주소 구조체 설정
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = ____;
    servaddr.sin_port = htons(_____);

    bind(sockfd, (struct sockaddr *)&_____, sizeof(_____));

    ControlData data;
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);

    while(true){
        int n = recvfrom(sockfd, &_____, sizeof(_____), 
        0, (struct sockaddr *)&cliaddr, &len);

        if(n > 0){
            std::cout << "Throttle: " << data.throttle 
                        << "Steering: " << data.steering << std::endl;
        }
    }

    return 0;
}