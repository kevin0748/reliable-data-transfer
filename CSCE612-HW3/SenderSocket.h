#pragma once
#define MAGIC_PORT    22345    // receiver listens on this port
#define MAX_PKT_SIZE (1500-28) // maximum UDP packet size accepted by receiver

#define MAX_SYN_ATTEMPTS 3
#define MAX_ATTEMPTS 5

// possible status codes from ss.Open, ss.Send, ss.Close
#define STATUS_OK 0 // no error
#define ALREADY_CONNECTED 1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED 2 // call to ss.Send()/Close() without ss.Open()
#define INVALID_NAME 3 // ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND 4 // sendto() failed in kernel
#define TIMEOUT 5 // timeout after all retx attempts are exhausted
#define FAILED_RECV 6 // recvfrom() failed in kernel

#define FORWARD_PATH 0
#define RETURN_PATH 1

#define MAGIC_PROTOCOL 0x8311AA

#pragma pack(push,1)  // sets struct padding/alignment to 1 byte
class Flags {
public:
	DWORD reserved : 5; // must be zero
	DWORD SYN : 1;
	DWORD ACK : 1;
	DWORD FIN : 1;
	DWORD magic : 24;

	Flags() {
		memset(this, 0, sizeof(*this)); magic = MAGIC_PROTOCOL;
	}
};
#pragma pack(pop)  // restores old packing

#pragma pack(push,1)  // sets struct padding/alignment to 1 byte
class SenderDataHeader {
public:
	Flags flags;
	DWORD seq; // must begin from 0
};
#pragma pack(pop)  // restores old packing


#pragma pack(push,1)  // sets struct padding/alignment to 1 byte
class LinkProperties {
public:
	// transfer parameters
	float RTT; // propagation RTT (in sec)
	float speed; // bottleneck bandwidth (in bits/sec)
	float pLoss[2]; // probability of loss in each direction
	DWORD bufferSize; // buffer size of emulated routers (in packets)

	LinkProperties() { memset(this, 0, sizeof(*this)); }
};
#pragma pack(pop)  // restores old packing

#pragma pack(push,1)  // sets struct padding/alignment to 1 byte
class SenderSynHeader {
public:
	SenderDataHeader sdh;
	LinkProperties lp;
};
#pragma pack(pop)  // restores old packing

#pragma pack(push,1)  // sets struct padding/alignment to 1 byte
class ReceiverHeader {
public:
	Flags flags;
	DWORD recvWnd; // receiver window for flow control (in pkts)
	DWORD ackSeq; // ack value = next expected sequence
};
#pragma pack(pop)  // restores old packing

class SenderSocket
{
public:
	SOCKET sock;       // socket handle
	
	const char* host;
	int port;
	in_addr hostAddr;

	double rto;		  // retransmit timeout

	clock_t startAt;  // timer when the constructor was called
	bool isOpen;      // SenderSocket is open


	SenderSocket();
	~SenderSocket();
	int Open(const char* targetHost, int port, int senderWindow, LinkProperties* lp);
	int Send(char* buf, int size);
	int Close();

private:
	int send(const char* msg, int msgLen);
	int recv(long timeout_sec, long timeout_usec, ReceiverHeader* rh);
	int dnsLookup(const char* host);
};

bool isSYNACK(Flags flags);
bool isFINACK(Flags flags);
