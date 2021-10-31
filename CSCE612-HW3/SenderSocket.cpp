#include "pch.h"
#include "SenderSocket.h"

SenderSocket::SenderSocket()
{
    WSADATA wsaData;

    //Initialize WinSock; once per program run
    WORD wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
        printf("WSAStartup error %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    // open a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET)
    {
        printf("socket() generated error %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    // bind socket
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(0);
    if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
        printf("socket bind error %d\n", WSAGetLastError());
        return;
    }

    // create this buffer once, then possibly reuse for multiple connections in Part 3
    //buf = (char*)malloc(MAX_PKT_SIZE + 1);
    //allocatedSize = MAX_PKT_SIZE + 1;
}

SenderSocket::~SenderSocket() {
    //delete buf;

    // close the socket to this server; open again for the next one
    closesocket(sock);

    // call cleanup when done with everything and ready to exit program
    WSACleanup();
}

int SenderSocket::Open(const char* targetHost, int port, int senderWindow, LinkProperties *lp) {
    lp->bufferSize = senderWindow + MAX_SYN_ATTEMPTS;

    SenderDataHeader sdh;
    sdh.seq = 0;
    sdh.flags.SYN = 1;

    SenderSynHeader ssh;
    ssh.lp = *lp;
    ssh.sdh = sdh;

    int ret;
    
    ret = getIP(targetHost, &host_ip);
    if (ret != 0) {
        return ret;
    }

    ret = send(host_ip, port, (char*)&ssh, sizeof(ssh));
    if (ret != STATUS_OK) {
        return ret;
    }

    ReceiverHeader rh;
    ret = recv(host_ip, port, 10, &rh);
    if (ret != STATUS_OK) {
        return ret;
    }

    printf("flags: %x\n", rh.flags);
    printf("recvWindow: %lu\n", rh.recvWnd);
    printf("ackSeq: %lu\n", rh.ackSeq);

    return STATUS_OK;
}

int SenderSocket::Send(const char* targetHost, int port, char* buf, int size) {
    static int seq = 1;

    SenderDataHeader sdh;
    sdh.seq = seq++;

    int ret;

    int sendBufSize = sizeof(sdh) + size;
    char* sendBuf = new char[sendBufSize];
    memcpy(sendBuf, (char*)&sdh, sizeof(sdh));
    memcpy(sendBuf + sizeof(sdh), buf, size);

    ret = send(host_ip, port, sendBuf, sendBufSize);
    if (ret != STATUS_OK) {
        return ret;
    }

    
    ReceiverHeader rh;
    ret = recv(host_ip, port, 1, &rh);
    if (ret != STATUS_OK) {
        return ret;
    }

    printf("seq: %d\n", seq);
    printf("flags: %x\n", rh.flags);
    printf("recvWindow: %lu\n", rh.recvWnd);
    printf("ackSeq: %lu\n", rh.ackSeq);
    
    return STATUS_OK;
}

int SenderSocket::Close(const char* targetHost, int port) {
    SenderDataHeader sdh;
    sdh.seq = 0;
    sdh.flags.FIN = 1;

    int ret;
    ret = send(host_ip, port, (char*)&sdh, sizeof(sdh));
    if (ret != STATUS_OK) {
        return ret;
    }

    ReceiverHeader rh;
    ret = recv(host_ip, port, 10, &rh);
    if (ret != STATUS_OK) {
        return ret;
    }

    printf("flags: %x\n", rh.flags);
    printf("recvWindow: %lu\n", rh.recvWnd);
    printf("ackSeq: %lu\n", rh.ackSeq);

    return STATUS_OK;
}

int getIP(const char* host, DWORD *IP) {
    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;

    struct hostent* remoteHost;
    *IP = inet_addr(host);
    if (*IP == INADDR_NONE)
    {
        // if not a valid IP, then do a DNS lookup
        if ((remoteHost = gethostbyname(host)) == NULL)
        {
            // printf("failed with %d\n", WSAGetLastError());
            printf("Invalid string: neither FQDN, nor IP address\n");
            return INVALID_NAME;
        }
        else // take the first IP address and copy into sin_addr
            memcpy((char*)(IP), remoteHost->h_addr, remoteHost->h_length);
    }
    
    return 0;
}

int SenderSocket::send(DWORD ip, int port, const char* msg, int msgLen) {
    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = ip; // server’s IP
    remote.sin_port = htons(port);
    if (sendto(sock, msg, msgLen, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
        printf("socket send error %d\n", WSAGetLastError());
        return FAILED_SEND;
    }

    return STATUS_OK;
}

int SenderSocket::recv(DWORD ip, int port, int timeout, ReceiverHeader *rh) {
    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    struct sockaddr_in respAddr;
    memset(&respAddr, 0, sizeof(respAddr));
    int resplen = sizeof(respAddr);

    ULONG reqAddr = ip;
    u_short reqPort = htons(port);

    int ret;

    if ((ret = select(0, &rfd, nullptr, nullptr, &tv)) > 0) {
        int bytes = recvfrom(sock, (char*)rh, sizeof(ReceiverHeader), 0, (sockaddr*)&respAddr, &resplen);
        if (bytes == SOCKET_ERROR) {
            //printf("failed with %d on recv\n", WSAGetLastError());
            printf("socket error %d\n", WSAGetLastError());
            return FAILED_RECV;
        }

        if (bytes == 0) {
            printf("empty response\n");
            return FAILED_RECV;
        }

        // check if this packet match the query server
        if (respAddr.sin_addr.s_addr != reqAddr || respAddr.sin_port != reqPort) {
            printf("recv unmatched addr or port\n");
            return FAILED_RECV;
        }

        //bufSize = bytes;
        //buf[bytes] = NULL;
        return STATUS_OK;
    }
    else if (ret == 0) {
        // report timeout
        printf("recv: timeout\n");
        return TIMEOUT;
    }
    else {
        printf("recv error: %d\n", WSAGetLastError());
        return FAILED_RECV;
    }
}