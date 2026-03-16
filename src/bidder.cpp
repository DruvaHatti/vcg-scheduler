#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include"../include/protocol.h"
int main(int argc,char* argv[])
{
    if(argc!=2)
    {
        std::cerr<<"Usage: ./bidder <bid_amount>"<<std::endl;
        return 1;
    }
    int sock=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(8080);
    serv_addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    Bid my_bid;
    my_bid.process_id=getpid();
    my_bid.value=std::atoi(argv[1]);
    sendto(sock,&my_bid,sizeof(my_bid),0,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
    std::cout<<"Bidder ["<<my_bid.process_id<<"] sent bid of "<<my_bid.value<<std::endl;
    while(true)
    {
        std::cout<<"Process "<<my_bid.process_id<<" is running..."<<std::endl;
        sleep(1);
    }
    return 0;
}