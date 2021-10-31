// CSCE612-HW3.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#pragma comment(lib, "Ws2_32.lib")

int main()
{
    std::cout << "Hello World!\n";

    const char* targetHost = "s3.irl.cs.tamu.edu";
    int power = 10;
    int senderWindow = 50000;

    UINT64 dwordBufSize = (UINT64)1 << power;
    DWORD* dwordBuf = new DWORD[dwordBufSize];  // user-requested buffer
    for (UINT64 i = 0; i < dwordBufSize; i++) { // required initialization
        dwordBuf[i] = i;
    }

    SenderSocket ss;
    int status;

    LinkProperties lp;
    lp.RTT = 0.2;
    lp.speed = 1e6 * 1000.2;
    lp.pLoss[FORWARD_PATH] = 0.00001;
    lp.pLoss[RETURN_PATH] = 0.0001;
    if ((status = ss.Open(targetHost, MAGIC_PORT, senderWindow, &lp)) != STATUS_OK) {
        // error handling: print status and quit
    }

    char* charBuf = (char*)dwordBuf;  // this buffer goes into socket
    UINT64 byteBufferSize = dwordBufSize << 2; // convert to bytes

    UINT64 off = 0;
    while (off < byteBufferSize) {
        // decide the size of next chunk
        int bytes = min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
        // send chunk into socket
        if ((status = ss.Send(targetHost, MAGIC_PORT, charBuf + off, bytes)) != STATUS_OK) {
            // return -1;
        }
        off += bytes;
    }

    if ((status = ss.Close(targetHost, MAGIC_PORT)) != STATUS_OK) {
        // error handling: print status and quit
    }
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
