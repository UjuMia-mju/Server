#pragma once

#include "NetAdress.h"

/*------------------
	 SocketUtils 
--------------------*/

class SocketUtils
{
public:
	// 정보: LPFN은 함수 주소를 저장하는 변수임. 
	// (dll에 있는 함수를 가져와야 해서 정적 링크는 안되고 런타임 환경에서 가져오게 됨.)
	static LPFN_CONNECTEX			ConnectEx;
	static LPFN_DISCONNECTEX		DisconnectEx;
	static LPFN_ACCEPTEX			AcceptEx;
public:
	static void Init();
	static void Clear();
	
	static bool BindWindowsFunction(SOCKET socket, GUID guid, LPVOID* functionPtr);
	static SOCKET CreateSocket();

	static bool SetLinger(SOCKET socket, uint16 onoff, uint16 linger);
	static bool SetReuseAddr(SOCKET socket, bool flag);
	static bool SetRecvBufferSize(SOCKET socket, int32 size);
	static bool SetSendBufferSize(SOCKET socket, int32 size);
	static bool SetTcpNoDelay(SOCKET socket, bool flag);
	static bool SetUpdateAcceptSocket(SOCKET socket, SOCKET listenSocket);

	static bool Bind(SOCKET socket, NetAddress adress);
	static bool BindAnyAdress(SOCKET socket, uint16 port);
	static bool Listen(SOCKET socket, int32 backlog = SOMAXCONN);
	static void Close(SOCKET& socket);
private:
};

template<typename T>
static inline bool SetSocketOpt(SOCKET socket, int32 level, int32 optName, T optValue)
{
	return SOCKET_ERROR != ::setsockopt(socket, level, optName, reinterpret_cast<char*>(&optValue), sizeof(T));
}

