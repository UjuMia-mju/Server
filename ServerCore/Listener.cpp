#include "pch.h"
#include "Listener.h"
#include "SocketUtils.h"
#include "IocpEvent.h"
#include "Session.h"
#include "Service.h"

/*----------------------
	Listener
------------------------*/

Listener::~Listener()
{
	SocketUtils::Close(_listenSocket);

	for (AcceptEvent* acceptEvent : _acceptEventPool)
	{
		xdelete(acceptEvent);
	}
}

bool Listener::StartAccept(ServerServiceRef server)
{
	_serverService = server;
	if (_serverService == nullptr)
	{
		return false;
	}

	_listenSocket = SocketUtils::CreateSocket();
	if (_listenSocket == INVALID_SOCKET)
	{
		return false;
	}

	if (_serverService->GetIocpCore()->Register(shared_from_this()) == false)
	{
		return false;
	}

	if (SocketUtils::SetReuseAddr(_listenSocket, true) == false)
	{
		return false;
	}

	if (SocketUtils::SetLinger(_listenSocket, 0, 0) == false)
	{
		return false;
	}

	if (SocketUtils::Bind(_listenSocket, _serverService->GetNetAddress()) == false)
	{
		return false;
	}

	if (SocketUtils::Listen(_listenSocket) == false)
	{
		return false;
	}

	const int32 acceptCount = _serverService->GetMaxSessionCount();
	for (int32 i = 0; i < acceptCount; ++i)
	{
		AcceptEvent* acceptEvent = new AcceptEvent();
		acceptEvent->owner = shared_from_this();
		_acceptEventPool.push_back(acceptEvent);
		RegisterAccept(acceptEvent);
	}

	return true;
}

void Listener::CloseAccept()
{
	SocketUtils::Close(_listenSocket);
}

HANDLE Listener::GetHandel()
{
	return reinterpret_cast<HANDLE>(_listenSocket);
}

void Listener::Dispatch(IocpEvent* iocpEvent, int32 numOfBytes)
{
	ASSERT_CRASH(iocpEvent->eventType == EventType::Accept);

	AcceptEvent* acceptEvent = static_cast<AcceptEvent*>(iocpEvent);
	ProcessAccept(acceptEvent);
}

void Listener::RegisterAccept(AcceptEvent* acceptEvent)
{
	SessionRef newSession = _serverService->CreateSession(); // IOCP 관찰 대상 (_serverService를 등록)
	acceptEvent->Init();
	acceptEvent->session = newSession;

	DWORD bytesReceived = 0;
	if (SocketUtils::AcceptEx(_listenSocket, newSession->GetSocket(), newSession->_recvBuffer.WritePos(), 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		OUT & bytesReceived, static_cast<LPOVERLAPPED>(acceptEvent)) == false)
	{
		const int32 errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			// 일단 다시 accept 걸어준다.
			RegisterAccept(acceptEvent);
		}
	}
}

void Listener::ProcessAccept(AcceptEvent* acceptEvent)
{
	SessionRef session = acceptEvent->session;

	// Listen소켓의 특성을 Clientsocket에 그대로 적용한다.
	// 실패하면 다시 accept걸어준다.
	if (SocketUtils::SetUpdateAcceptSocket(session->GetSocket(), _listenSocket) == false)
	{
		// 일단 다시 accept 걸어준다.
		RegisterAccept(acceptEvent);
		return;
	}

	SOCKADDR_IN sockAddress;
	int32 addressLen = sizeof(sockAddress);
	// 클라이언트 주소 얻기 (sockAddress에 저장됨)
	if (SOCKET_ERROR == ::getpeername(session->GetSocket(),OUT reinterpret_cast<SOCKADDR*>(&sockAddress), &addressLen))
	{
		// 일단 다시 accept 걸어준다.
		RegisterAccept(acceptEvent);
		return;
	}

	cout << "[Listener] Accept New Connection : " << endl;

	session->SetNetAdress(NetAddress(sockAddress));
	session->ProcessConnect();

	RegisterAccept(acceptEvent);
}
