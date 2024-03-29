#include "pch.h"
#include "SenderSocket.h"

SenderSocket::SenderSocket()
{
    objStartAt = clock();

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
    CloseHandle(empty);
    CloseHandle(full);
    CloseHandle(eventQuit);
    CloseHandle(socketReceiveReady);
    CloseHandle(workerThread);
    CloseHandle(statsThread);

    DeleteCriticalSection(&queueMutex);

    delete[] pending_pkts;

    // close the socket to this server; open again for the next one
    closesocket(sock);

    // call cleanup when done with everything and ready to exit program
    WSACleanup();
}


DWORD WINAPI threadWorkerRun(LPVOID pParam) {
    SenderSocket* ss = ((SenderSocket*)pParam);

    int kernelBuffer = 20e6; // 20meg
    if (setsockopt(ss->sock, SOL_SOCKET, SO_RCVBUF, (char*)&kernelBuffer, sizeof(int)) == SOCKET_ERROR)
        printf("failed to setsockopt (%d)\n", WSAGetLastError());

    kernelBuffer = 20e6; // 20meg
    if (setsockopt(ss->sock, SOL_SOCKET, SO_SNDBUF, (char*)&kernelBuffer, sizeof(int)) == SOCKET_ERROR)
        printf("failed to setsockopt (%d)\n", WSAGetLastError());

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);


    ss->WorkerRun();
    return 0;
}

DWORD WINAPI threadStats(LPVOID pParam) {
    SenderSocket* ss = ((SenderSocket*)pParam);
    ss->Stats();
    return 0;
}

int SenderSocket::Open(const char* _targetHost, int _port, int senderWindow, LinkProperties *lp) {
    if (isOpen) {
        return ALREADY_CONNECTED;
    }
    
    host = _targetHost;
    port = _port;
    W = senderWindow;

    // Initial sending sequence
    nextSeq = 0;
    nextToSend = 0;
    senderBase = 0;
    newReleased = 0;
    lastReleased = 0;
    estRTT = 0;
    devRTT = 0;
    retxCnt = 0;
    dupAckCnt = 0;
    fastReTxCnt = 0;
    timeoutCnt = 0;

    // Initial semaphore
    empty = CreateSemaphore(NULL, 0, W, NULL);
    if (empty == NULL) {
        printf("CreateSemaphore error\n");
        return FAILED_INTERNAL_HANDLE;
    }
    full = CreateSemaphore(NULL, 0, W, NULL);
    if (full == NULL) {
        printf("CreateSemaphore error\n");
        return FAILED_INTERNAL_HANDLE;
    }
    eventQuit = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (eventQuit == NULL) {
        printf("CreateEvent error\n");
        return FAILED_INTERNAL_HANDLE;
    }
    socketReceiveReady = CreateEvent(NULL, FALSE, FALSE, NULL); // auto-reset
    if (socketReceiveReady == NULL) {
        printf("CreateEvent error\n");
        return FAILED_INTERNAL_HANDLE;
    }
    
    // Initial queue
    InitializeCriticalSection(&queueMutex);
    queueSize = 0;
    sentDone = false;

    pending_pkts = new Packet[W];
    
    lp->bufferSize = senderWindow + MAX_SYN_ATTEMPTS;

    SenderDataHeader sdh;
    sdh.seq = nextSeq;
    sdh.flags.SYN = 1;

    SenderSynHeader ssh;
    ssh.lp = *lp;
    ssh.sdh = sdh;

    int ret;
    ret = dnsLookup(host);
    if (ret == INVALID_NAME) {
        //printf(" [ %.3f] --> target %s is invalid\n",
        //    (double)(clock() - startAt) / (double)CLOCKS_PER_SEC, host);
        return ret;
    }

    rto = max(1, 2*lp->RTT);
    int attempts = 0;
    while (attempts++ < MAX_SYN_ATTEMPTS) {
        clock_t pktSendAt = clock() - objStartAt;
        //printf(" [ %.3f] --> ", (double)pktSendAt / (double)CLOCKS_PER_SEC);
        
        ret = send((char*)&ssh, sizeof(ssh));
        if (ret != STATUS_OK) {
            printf("failed sendto with %d\n", WSAGetLastError());
            return ret;
        }
        //printf("SYN %d (attempt %d of %d, RTO %.3f) to %s\n",
        //    sdh.seq, attempts, MAX_SYN_ATTEMPTS, rto, inet_ntoa(hostAddr));

        
       
        ReceiverHeader rh;
        ret = recv(floor(rto),(rto-floor(rto)) * 1e6, &rh);
        if (ret == TIMEOUT) {
            continue;
        }
        clock_t pktRecvAt;
        pktRecvAt = clock() - objStartAt;
        //printf(" [ %.3f] <-- ", (double)pktRecvAt / (double)CLOCKS_PER_SEC);

        if (ret == STATUS_OK) {
            if (!isSYNACK(rh.flags)) {
                continue;
            }

            rto = 3 * (double)(pktRecvAt - pktSendAt) / (double)CLOCKS_PER_SEC;
            //printf("SYN-ACK %d window %d; setting initial RTO to %.3f\n",
            //    rh.ackSeq, rh.recvWnd, rto);

            isOpen = true;
            lastReleased = min(W, rh.recvWnd);
            if (!ReleaseSemaphore(empty, lastReleased, NULL)) {
                printf("ReleaseSemaphore 'empty' error\n");
                return FAILED_INTERNAL_HANDLE;
            }

            // start workstatser thread
            statsThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadStats, this, 0, NULL);
            if (statsThread == NULL) {
                printf("failed to create stats thread\n");
                return FAILED_THREAD;
            }

            // start worker thread
            workerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadWorkerRun, this, 0, NULL);
            if (workerThread == NULL) {
                printf("failed to create worker thread\n");
                return FAILED_THREAD;
            }

            return STATUS_OK;
        }
        else {
            printf("failed recvfrom with %d\n", WSAGetLastError());
            return ret;
        }
    }

    return TIMEOUT;
}

void SenderSocket::Stats() {
    clock_t startAt = clock();
    clock_t cur, prev = startAt;
    int prevBase = 0;

    while (WaitForSingleObject(eventQuit, 2000) == WAIT_TIMEOUT) {
        cur = clock();
        printf("[%3d] B %7d (%3.1f MB) N %7d T %2d F %2d W %4d S %2.3f Mbps RTT %.3f\n",
                (cur-startAt)/CLOCKS_PER_SEC,
                senderBase,
                (double)senderBase * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) / 1e6,
                nextToSend, // n
                timeoutCnt, // T
                fastReTxCnt, //F
                effectiveWin, // W
                (double)(senderBase-prevBase) * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) / 1e6 / ((cur-prev)/CLOCKS_PER_SEC), // Speed
                estRTT
                );

        prevBase = senderBase;
        prev = cur;
    }
}

void SenderSocket::WorkerRun() {
    if (WSAEventSelect(sock, socketReceiveReady, FD_READ) == SOCKET_ERROR) {
        printf("WSAEventSelect failed: error %d\n", WSAGetLastError());
        return;
    }

    bool finished = false;

    HANDLE events[] = { socketReceiveReady, full};
    while (!finished) {
        long timeout = rto * 1000;
        DWORD y;
        int ret, ret2;
        ReceiverHeader rh;
        ret = WaitForMultipleObjects(2, events, false, timeout);
        switch (ret)
        {
        case WAIT_TIMEOUT:
           // printf("timeout\n");
            EnterCriticalSection(&queueMutex);
            finished = sentDone && (queueSize == 0);
            LeaveCriticalSection(&queueMutex);
            if (finished) continue;

            if (retxCnt > MAX_ATTEMPTS) {
                finished = true;
                continue;
            }

            // retransmit
            ret2 = send(pending_pkts[senderBase % W].pkt, pending_pkts[senderBase % W].size);
            if (ret2 != STATUS_OK) {
                printf("Failed to send (%d)\n", ret2);
                finished = true;
                continue;
            }
            retxCnt++;
            timeoutCnt++;

            pending_pkts[senderBase % W].txTime = clock();
            break;

        case WAIT_OBJECT_0: // socket
            ret2 = recvWOTimeout(&rh);
            if (ret2 != STATUS_OK) {
                finished = true;
                continue;
            }

            if (!isACK(rh.flags)) {
                continue;
            }

            recvLastAckAt = clock();

            /*
            printf("--------------\n");
            printf("flags: %x\n", rh.flags);
            printf("recvWindow: %lu\n", rh.recvWnd);
            printf("ackSeq: %lu\n", rh.ackSeq);
            */
            y = rh.ackSeq;
            if (y > senderBase) {
                EnterCriticalSection(&queueMutex);
                queueSize = queueSize - (y - senderBase);
                finished = sentDone && (queueSize == 0);
                LeaveCriticalSection(&queueMutex);

                if (retxCnt == 0) {
                    double sampleRTT = (clock() - pending_pkts[(y - 1) % W].txTime) / (double)CLOCKS_PER_SEC;
                    if (estRTT == 0) {
                        estRTT = sampleRTT;
                        devRTT = 0;
                    }
                    else {
                        estRTT = (1 - ALPHA) * estRTT + ALPHA * sampleRTT;
                        devRTT = (1 - BETA) * devRTT + BETA * abs(sampleRTT - estRTT);
                    }

                    rto = estRTT + 4 * max(devRTT, 0.010);
                }
                retxCnt = 0;     
                dupAckCnt = 0;

                senderBase = rh.ackSeq;
                effectiveWin = min(W, rh.recvWnd);
                
                newReleased = senderBase + effectiveWin - lastReleased;
                if (!ReleaseSemaphore(empty, newReleased, NULL)) {
                    printf("ReleaseSemaphore 'empty' error\n");
                }
                lastReleased += newReleased;
            }
            else if (y == senderBase) {
                dupAckCnt++;
                if (dupAckCnt == FAST_RETX_THRESHOLD) {
                    // retransmit
                    ret2 = send(pending_pkts[senderBase % W].pkt, pending_pkts[senderBase % W].size);
                    if (ret2 != STATUS_OK) {
                        printf("Failed to send (%d)\n", ret2);
                        finished = true;
                        continue;
                    }
                    retxCnt++;
                    fastReTxCnt++;
                    pending_pkts[senderBase % W].txTime = clock();
                }
            }

            break;

        case WAIT_OBJECT_0 + 1: // sender
            ret2 = send(pending_pkts[nextToSend % W].pkt, pending_pkts[nextToSend % W].size);
            if (ret2 != STATUS_OK) {
                printf("Failed to send (%d)\n", ret2);
                finished = true;
                continue;
            }

            if (nextToSend == 0) {
                sendFirstPktAt = clock();
            }

            pending_pkts[nextToSend % W].txTime = clock();
            nextToSend++;
            //printf("[Worker Run] Send %d\n", nextToSend-1);
            break;

        default:
            printf("[Worker Run] Failed wait\n");
            return;
        }
    }

    if (!SetEvent(eventQuit)) {
        printf("SetEvent failed (%d)\n", GetLastError());
        return;
    }

    return;
}

int SenderSocket::Send(char* buf, int bufSize) {
    if (!isOpen) {
        return NOT_CONNECTED;
    }

    HANDLE events[] = { eventQuit, empty };
    int slot;
    Packet* p;
    SenderDataHeader* sdh;

    int ret = WaitForMultipleObjects(2, events, false, INFINITE);
    switch (ret) {
    case WAIT_OBJECT_0: // eventQuit
        // printf("eventQuit\n");
        break;

    case WAIT_OBJECT_0 + 1: // empty
        // no need fur mutex as no shared variables are modeified
        slot = nextSeq % W;
        p = pending_pkts + slot;
        p->type = PKT_TYPE_DATA;
        p->size = sizeof(SenderDataHeader) + bufSize;
        p->txTime = clock();

        sdh = (SenderDataHeader*)p->pkt;
        sdh->flags.init();
        sdh->seq = nextSeq;

        memcpy(sdh + 1, buf, bufSize);
        nextSeq++;
        //printf("[Send] fill buffer at %d\n", nextSeq - 1);

        EnterCriticalSection(&queueMutex);
        queueSize += 1;
        LeaveCriticalSection(&queueMutex);

        if (!ReleaseSemaphore(full, 1, NULL)) {
            printf("ReleaseSemaphore 'full' error\n");
            return FAILED_INTERNAL_HANDLE;
        }
        break;
    default:
        printf("[Worker Run] failed wait\n");
        break;
    }

    return STATUS_OK;
}

int SenderSocket::Close() {
    if (!isOpen) {
        return NOT_CONNECTED;
    }

    DWORD waitResult = WaitForSingleObject(eventQuit, INFINITE);
    switch (waitResult) {
    case WAIT_OBJECT_0:
       // printf("recv eventQuit. Start sending FIN\n");
        break;
    default:
        printf("wait error\n");
        return FAILED_INTERNAL_HANDLE;
    }

    SenderDataHeader sdh;
    sdh.seq = nextSeq;
    sdh.flags.FIN = 1;

    int ret;
    int attempts = 0;
    clock_t timer;

    while (attempts++ < MAX_ATTEMPTS) {
        //timer = clock() - startAt;
        //printf(" [ %.3f] --> ", 
        //    (double)timer / (double)CLOCKS_PER_SEC);
        ret = send((char*)&sdh, sizeof(sdh));
        if (ret != STATUS_OK) {
            return ret;
        }

        //printf("FIN %d (attempt %d of %d, RTO %.3f)\n",
        //    sdh.seq, attempts, MAX_ATTEMPTS, rto);

        ReceiverHeader rh;
        ret = recv(floor(rto), (rto - floor(rto)) * 1e6, &rh);
        if (ret == TIMEOUT) {
            continue;
        }
        timer = clock() - objStartAt;
        printf("[ %.3f] <-- ", (double)timer / (double)CLOCKS_PER_SEC);

        if (ret == STATUS_OK) {
            if (!isFINACK(rh.flags)) {
                continue;
            }

            timer = clock() - objStartAt;
            printf("FIN-ACK %d window %0X\n",
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
    remote.sin_addr = hostAddr; // server�s IP
    remote.sin_port = htons(port);
    if (sendto(sock, msg, msgLen, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
        return FAILED_SEND;
    }

    return STATUS_OK;
}

int SenderSocket::recvWOTimeout(ReceiverHeader* rh) {
    struct sockaddr_in respAddr;
    memset(&respAddr, 0, sizeof(respAddr));
    int resplen = sizeof(respAddr);

    ULONG reqAddr = hostAddr.s_addr;
    u_short reqPort = htons(port);

    int bytes = recvfrom(sock, (char*)(rh), sizeof(ReceiverHeader), 0, (sockaddr*)&respAddr, &resplen);
    if (bytes == SOCKET_ERROR) {
        //printf("failed with %d on recv\n", WSAGetLastError());
        return FAILED_RECV;
    }

    if (bytes == 0) {
        printf("empty response\n");
        return FAILED_RECV;
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

bool isACK(Flags flags) {
    if (flags.SYN == 0 && flags.ACK == 1 && flags.FIN == 0) {
        return true;
    }

    return false;
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