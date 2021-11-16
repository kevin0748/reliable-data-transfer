// CSCE612-HW3.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

int main(int argc, char *argv[])
{
    if (argc != 8) {
        printf("[Usage] rdt.exe $host $buffer_size $sender_window $rtt $forward_loss_rate $return_loss_rate $speed \n");
        return -1;
    }

    char* targetHost = argv[1];
    int power = atoi(argv[2]);
    int senderWindow = atoi(argv[3]);
    float rtt = atof(argv[4]);
    float forward_loss_rate = atof(argv[5]);
    float return_loss_rate = atof(argv[6]);
    float speed = atof(argv[7]);

    printf("Main:\tsender W = %d, RTT %.3f sec, loss %g / %g, link %1.f Mbps\n",
        senderWindow, rtt, forward_loss_rate, return_loss_rate, speed);

    clock_t bufferInitTimer = clock();
    UINT64 dwordBufSize = (UINT64)1 << power;
    DWORD* dwordBuf = new DWORD[dwordBufSize];  // user-requested buffer
    for (UINT64 i = 0; i < dwordBufSize; i++) { // required initialization
        dwordBuf[i] = i;
    }

    bufferInitTimer = clock() - bufferInitTimer;
    printf("Main:\tinitializing DWORD array with 2^%d elements... done in %d ms\n",
        power, bufferInitTimer);

    SenderSocket ss;
    int status;

    clock_t senderSocketTimer = clock();
    LinkProperties lp;
    lp.RTT = rtt;
    lp.speed = 1e6 * speed;
    lp.pLoss[FORWARD_PATH] = forward_loss_rate;
    lp.pLoss[RETURN_PATH] = return_loss_rate;
    if ((status = ss.Open(targetHost, MAGIC_PORT, senderWindow, &lp)) != STATUS_OK) {
        printf("Main:\tconnected failed with status %d\n", status);
        return -1;
    }

    char* charBuf = (char*)dwordBuf;  // this buffer goes into socket
    UINT64 byteBufferSize = dwordBufSize << 2; // convert to bytes

    clock_t socketOpenAt = clock();
    printf("Main:\tconnected to %s in %.3f sec, pkt size %d bytes\n",
        targetHost, (double)(socketOpenAt - senderSocketTimer) / (double)CLOCKS_PER_SEC, MAX_PKT_SIZE);

    UINT64 off = 0;
    while (off < byteBufferSize) {
        // decide the size of next chunk
        int bytes = min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
        // send chunk into socket
        if ((status = ss.Send(charBuf + off, bytes)) != STATUS_OK) {
            // error handling: print status and quit
        }
        off += bytes;
    }

    EnterCriticalSection(&ss.queueMutex);
    ss.sentDone = true;
    LeaveCriticalSection(&ss.queueMutex);

    if ((status = ss.Close()) != STATUS_OK) {
        printf("Main:\tconnected failed with status %d\n", status);
        return -1;
    }

    double elapsedTime = (double)(ss.recvLastAckAt - ss.sendFirstPktAt)/ (double)CLOCKS_PER_SEC ;

    Checksum cs;
    DWORD check = cs.CRC32((unsigned char*)charBuf, byteBufferSize);

    // TODO: fix the timer
    printf("Main:\ttransfer finished in %.3f sec, %.2f Kbps, checksum %0X\n", 
        elapsedTime,
        (double)byteBufferSize * 8/elapsedTime/1000,
        check
    );
    printf("Main:\testRTT %.3f, ideal rate %.2f Kbps\n",ss.estRTT, (double)senderWindow * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader))/ ss.estRTT / 1000);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
