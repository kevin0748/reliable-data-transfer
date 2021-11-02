#include "pch.h"
#include "SenderSocket.h"

SenderSocket::SenderSocket()
{
    startAt = clock();

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

int SenderSocket::Open(const char* _targetHost, int _port, int senderWindow, LinkProperties *lp) {
    host = _targetHost;
    port = _port;

    if (isOpen) {
        return ALREADY_CONNECTED;
    }
    
    lp->bufferSize = senderWindow + MAX_SYN_ATTEMPTS;

    SenderDataHeader sdh;
    sdh.seq = 0;
    sdh.flags.SYN = 1;

    SenderSynHeader ssh;
    ssh.lp = *lp;
    ssh.sdh = sdh;

    int ret;
    ret = dnsLookup(host);
    if (ret == INVALID_NAME) {
        printf(" [ %.3f] --> target %s is invalid\n",
            (double)(clock() - startAt) / (double)CLOCKS_PER_SEC, host);
        return ret;
    }

    rto = 1;
    int attempts = 0;
    while (attempts++ < MAX_SYN_ATTEMPTS) {
        clock_t pktSendAt = clock() - startAt;
        printf(" [ %.3f] --> ", (double)pktSendAt / (double)CLOCKS_PER_SEC);
        
        ret = send((char*)&ssh, sizeof(ssh));
        if (ret != STATUS_OK) {
            printf("failed sendto with %d\n", WSAGetLastError());
            return ret;
        }
        printf("SYN %d (attempt %d of %d, RTO %.3f) to %s\n",
            sdh.seq, attempts, MAX_SYN_ATTEMPTS, rto, inet_ntoa(hostAddr));

        
       
        ReceiverHeader rh;
        ret = recv(floor(rto),(rto-floor(rto)) * 1e6, &rh);
        if (ret == TIMEOUT) {
            continue;
        }
        clock_t pktRecvAt;
        pktRecvAt = clock() - startAt;
        printf(" [ %.3f] <-- ", (double)pktRecvAt / (double)CLOCKS_PER_SEC);

        if (ret == STATUS_OK) {
            if (!isSYNACK(rh.flags)) {
                continue;
            }

            rto = 3 * (double)(pktRecvAt - pktSendAt) / (double)CLOCKS_PER_SEC;
            printf("SYN-ACK %d window %d; setting initial RTO to %.3f\n",
                rh.ackSeq, rh.recvWnd, rto);

            isOpen = true;
            return STATUS_OK;
        }
        else {
            printf("failed recvfrom with %d\n", WSAGetLastError());
            return ret;
        }
    }

    return TIMEOUT;
}

int SenderSocket::Send(char* buf, int bufSize) {
    return 0;

    if (!isOpen) {
        return NOT_CONNECTED;
    }

    // TODO: skip for hw1

    SenderDataHeader sdh;
    int pkt_seq = 0;
    sdh.seq = pkt_seq;

    int ret;

    int sendBufSize = sizeof(sdh) + bufSize;
    char* sendBuf = new char[sendBufSize];
    memcpy(sendBuf, (char*)&sdh, sizeof(sdh));
    memcpy(sendBuf + sizeof(sdh), buf, bufSize);

    ret = send(sendBuf, sendBufSize);
    if (ret != STATUS_OK) {
        return ret;
    }

    
    ReceiverHeader rh;
    ret = recv( 10,0, &rh);
    if (ret != STATUS_OK) {
        return ret;
    }

    printf("--------------\n");
    printf("seq: %d\n", pkt_seq);
    printf("flags: %x\n", rh.flags);
    printf("recvWindow: %lu\n", rh.recvWnd);
    printf("ackSeq: %lu\n", rh.ackSeq);
    
    pkt_seq = rh.ackSeq;
    return STATUS_OK;
}

int SenderSocket::Close() {
    if (!isOpen) {
        return NOT_CONNECTED;
    }

    SenderDataHeader sdh;
    sdh.seq = 0;
    sdh.flags.FIN = 1;

    int ret;
    int attempts = 0;
    clock_t timer;

    while (attempts++ < MAX_ATTEMPTS) {
        timer = clock() - startAt;
        printf(" [ %.3f] --> ", 
            (double)timer / (double)CLOCKS_PER_SEC);
        ret = send((char*)&sdh, sizeof(sdh));
        if (ret != STATUS_OK) {
            return ret;
        }

        printf("FIN %d (attempt %d of %d, RTO %.3f)\n",
            sdh.seq, attempts, MAX_ATTEMPTS, rto);

        ReceiverHeader rh;
        ret = recv(floor(rto), (rto - floor(rto)) * 1e6, &rh);
        if (ret == TIMEOUT) {
            continue;
        }
        timer = clock() - startAt;
        printf(" [ %.3f] <-- ", (double)timer / (double)CLOCKS_PER_SEC);

        if (ret == STATUS_OK) {
            if (!isFINACK(rh.flags)) {
                continue;
            }

            timer = clock() - startAt;
            printf("FIN-ACK %d window %d\n",
                rh.ackSeq, rh.recvWnd);
            return STATUS_OK;
        }
        else{
            printf("failed recvfrom with %d\n", WSAGetLastError());
            return ret;
        }
    }

    return TIMEOUT;
}

int SenderSocket::dnsLookup(const char* host) {
    struct hostent* remoteHost;
    DWORD IP = inet_addr(host);
    if (IP == INADDR_NONE)
    {
        // if not a valid IP, then do a DNS lookup
        if ((remoteHost = gethostbyname(host)) == NULL)
        {
            // printf("failed with %d\n", WSAGetLastError());
            // printf("Invalid string: neither FQDN, nor IP address\n");
            return INVALID_NAME;
        }
        else // take the first IP address and copy into sin_addr
            memcpy((char*)&(hostAddr), remoteHost->h_addr, remoteHost->h_length);
    }
    else {
        hostAddr.S_un.S_addr = IP;
    }
    
    return 0;
}

int SenderSocket::send(const char* msg, int msgLen) {
    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_addr = hostAddr; // server’s IP
    remote.sin_port = htons(port);
    if (sendto(sock, msg, msgLen, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
        return FAILED_SEND;
    }

    return STATUS_OK;
}

int SenderSocket::recv(long timeout_sec, long timeout_usec, ReceiverHeader *rh) {
    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);

    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = timeout_usec;

    struct sockaddr_in respAddr;
    memset(&respAddr, 0, sizeof(respAddr));
    int resplen = sizeof(respAddr);

    ULONG reqAddr = hostAddr.s_addr;
    u_short reqPort = htons(port);

    int ret;

    if ((ret = select(0, &rfd, nullptr, nullptr, &tv)) > 0) {
        int bytes = recvfrom(sock, (char*)rh, sizeof(ReceiverHeader), 0, (sockaddr*)&respAddr, &resplen);
        if (bytes == SOCKET_ERROR) {
            //printf("failed with %d on recv\n", WSAGetLastError());
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
        // printf("recv: timeout\n");
        return TIMEOUT;
    }
    else {
        return FAILED_RECV;
    }
}

bool isSYNACK(Flags flags) {
    if (flags.SYN == 1 && flags.ACK == 1 && flags.FIN == 0) {
        return true;
    }

    return false;
}

bool isFINACK(Flags flags) {
    if (flags.SYN == 0 && flags.ACK == 1 && flags.FIN == 1) {
        return true;
    }

    return false;
}