#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<signal.h>
#include<unistd.h>
#include<vector>
#include<sys/select.h>
#include "../include/protocol.h"
#include<map>
#include<algorithm>

int main()
{
    int sock=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(8080);
    serv_addr.sin_addr.s_addr=INADDR_ANY;
    if(bind(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr))<0)
    {
        std::cerr<<"Binding failed!"<<std::endl;
        return 1;
    }
    std::cout<<"Auctioneer is active on port 8080..."<<std::endl;
    std::map<pid_t,int> accounts;
    int round_num=1;
    std::vector<Bid> active_jobs;
    pid_t currently_running_pid=-1;
    while(true)
    {
        std::cout<<"\n==================================="<<std::endl;
        std::cout<<"Starting Round "<<round_num++<<"..."<<std::endl;
        std::cout<<"Bidding window open for 5 seconds..."<<std::endl;
        
        struct timeval tv;
        tv.tv_sec=5;
        tv.tv_usec=0;
        fd_set readfds;

        //std::cout<<"Bidding window open for 5 seconds. Waiting for bidders..."<<std::endl;
        while(true)
        {
            FD_ZERO(&readfds);
            FD_SET(sock,&readfds);

            int activity=select(sock+1,&readfds,NULL,NULL,&tv);

            if(activity<0)
            {
                std::cerr<<"Select error occured!"<<std::endl;
                break;
            }
            else if(activity==0)
            {
                std::cout<<"\n--- Bidding Window Closed! ---"<<std::endl;
                break;
            }
            if(FD_ISSET(sock,&readfds))
            {
                Bid incoming;
                struct sockaddr_in client_addr;
                socklen_t addr_len=sizeof(client_addr);

                recvfrom(sock,&incoming,sizeof(incoming),0,(struct sockaddr*)&client_addr,&addr_len);
                bool is_new_job=true;
                for(const auto& job:active_jobs)
                {
                    if(job.process_id==incoming.process_id) is_new_job=false;
                }
                if(is_new_job)
                {
                    accounts[incoming.process_id]=1000;
                    active_jobs.push_back(incoming);
                    std::cout<<"New process entered. PID: "<<incoming.process_id<<" (Target Bid: "<<incoming.value<<")"<<std::endl;
                    if(kill(incoming.process_id,SIGSTOP)==0)
                    {
                        std::cout<<"Pausing PID "<<incoming.process_id<<" in Ready Queue."<<std::endl;
                    }
                }   
            }
        }
        std::cout << "Total active processes in pool: " << active_jobs.size() << std::endl;
        if(active_jobs.empty())
        {
            std::cout<<"No process in queue. Scheduler idling..."<<std::endl;
            continue;
        }
        int highest_bid=-1;
        int second_highest_bid=-1;
        pid_t winner_pid=-1;
        for(const auto&bid: active_jobs)
        {
            // Cap their bid if they don't have enough money in the bank
            int effective_bid=std::min(bid.value,accounts[bid.process_id]);
            if(effective_bid>highest_bid)
            {
                second_highest_bid=highest_bid;
                highest_bid=effective_bid;
                winner_pid=bid.process_id;
            }
            else if(effective_bid>second_highest_bid)
            {
                second_highest_bid=effective_bid;
            }
        }
        if(second_highest_bid==-1)
        {
            second_highest_bid=0;
        }
        accounts[winner_pid]-=second_highest_bid;

        if(active_jobs.size()>1)
        {
            int tax=second_highest_bid;
            if(tax==0)
            {
                tax=50;
                std::cout<<"[SYSTEM BAILOUT] VCG Price hit 0. Injecting 50 credits to distribute."<<std::endl;
            }
            int share=tax/(active_jobs.size()-1);
            for(const auto & job:active_jobs)
            {
                if(job.process_id!=winner_pid)
                {
                    accounts[job.process_id]+=share;
                }
            }
            std::cout << "-> Robin Hood redistributed " << share << " credits to all paused processes." << std::endl;
        }
        std::cout<<"\n=== AUCTION RESULTS ==="<<std::endl;
        std::cout<<"Winner PID: "<<winner_pid<<"(Bid: "<<highest_bid<<")"<<std::endl;
        std::cout<<"VCG Price charged: "<<second_highest_bid<<std::endl;
        std::cout<<"Winner's remaining balance: "<<accounts[winner_pid]<<std::endl;
        if(currently_running_pid!=-1 && currently_running_pid!=winner_pid)
        {
            std::cout<<"\n[CONTEXT SWITCH] Preempting old winner PID "<<currently_running_pid<<"..."<<std::endl;
            kill(currently_running_pid,SIGSTOP);
        }
        std::cout<<"\nResuming Winner PID "<<winner_pid<<"..."<<std::endl;
        if(kill(winner_pid,SIGCONT)==-1)
        {
            std::cerr<<"Failed to send SIGCONT to PID "<<winner_pid<<std::endl;
        }
        currently_running_pid=winner_pid;
    }
    return 0;
}