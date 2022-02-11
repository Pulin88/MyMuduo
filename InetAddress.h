#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <iostream>

using namespace std;

class InetAddress
{
    public:
    explicit InetAddress(uint16_t port=0, string ip="127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
    :addr_(addr){}

    string toIp() const;
    string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const
    {return &addr_;}
    void setSockAddr(const sockaddr_in &addr)
     {addr_ = addr;}

    private:
    sockaddr_in addr_;
    
};