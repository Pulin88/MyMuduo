#include "InetAddress.h"
#include <string.h>

/*
inet_pton 本地const char* --> 网络uint
inet_ntop 网络uint --> 本地const char*
htons 本地short --> 网络short
ntohs 网络short --> 本地short
*/ 

InetAddress::InetAddress(uint16_t port, string ip)
{
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
}

string InetAddress::toIp() const
{
    char buf[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, 64);
    return buf;
}

string InetAddress::toIpPort() const
{
    char buf[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, 64);

    int end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf+end,":%u",port);
    return buf;
}

uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}

/*
int main()
{
    cout << InetAddress(22,"127.127.127.127").toIpPort() << endl;
    cout << InetAddress(22,"127.127.127.127").toPort() << endl;
    cout << InetAddress(22,"127.127.127.127").toIp() << endl;
}
*/
