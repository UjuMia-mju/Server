#include "pch.h"
#include "Session.h"
#include "SocketUtils.h"
#include "Service.h"

/*----------------------
	Session
------------------------*/

Session::Session() : _recvBuffer(BUFFER_SIZE)
{
	_connected.store(false); // 초기값은 false
	_socket = SocketUtils::CreateSocket();
}

Session::~Session()
{
	SocketUtils::Close(_socket);
}

void Session::Send(SendBufferRef sendBuffer)
{
	if (IsConnected() == false)
	{
		return;
	} 

	bool registerSend = false;

	{
		// 현재 register send가 걸리지 않은 상태면 걸어준다..?
		WRITE_LOCK;
		_sendQueue.push(sendBuffer);
		// 변경전 값을 반환하게 된다.
		if (_sendRegistered.exchange(true) == false)
		{
			registerSend = true;
		}
	}

	if (registerSend)
	{
		RegisterSend();
	}
}

bool Session::Connect()
{
	return RegisterConnect();
}

void Session::Disconnect(const WCHAR* cause)
{
	if (_connected.exchange(false) == false)
	{
		return;
	}

	//wcout << L"Disconnected: " << cause << endl;
	cout << "Session Disconnected" << endl;
	RegisterDisConnect();
}

HANDLE Session::GetHandle()
{
	return reinterpret_cast<HANDLE>(_socket);
}

void Session::Dispatch(IocpEvent* iocpEvent, int32 numOfBytes)
{
	switch (iocpEvent->eventType)
	{
	case EventType::Connect:
		ProcessConnect();
		break;
	case EventType::DisConnect:
		ProcessDisConnect();
		break;
	case EventType::Recv:
		ProcessRecv(numOfBytes);
		break;
	case EventType::Send:
		ProcessSend(numOfBytes);
		break;
	default:
		break;
	}
}

bool Session::RegisterConnect()
{
	if (IsConnected())
	{
		return false;
	}

	// 클라이언트가 아니면 연결 X: 
	if (GetService()->GetServiceType() != ServiceType::Client)
	{
		return false;
	}

	if (SocketUtils::SetReuseAddr(_socket, true) == false)
	{
		return false;
	}

	if (SocketUtils::BindAnyAdress(_socket, 0/*0이면 남는거 아무거나*/) == false)
	{
		return false;
	}

	_connectEvent.Init();
	_connectEvent.eventType = EventType::Connect;
	_connectEvent.owner = shared_from_this(); // ADD_REF

	DWORD numOfBytes = 0;
	// GetService가 client 타입이면 NetAddress가 서버(내가 붙어야 하는) 주소임.
	SOCKADDR_IN sockAddr = GetService()->GetNetAddress().GetSockAddr();
	if (false == SocketUtils::ConnectEx(_socket, reinterpret_cast<SOCKADDR*>(&sockAddr), sizeof(sockAddr), nullptr, 0, OUT & numOfBytes, &_connectEvent))
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			HandleError(errorCode);
			_connectEvent.owner = nullptr; // RELEASE_REF
			return false;
		}
	}
	return true;
}

bool Session::RegisterDisConnect()
{
	cout << "123123123" << endl;
	_disConnectEvent.Init();
	_disConnectEvent.eventType = EventType::DisConnect;
	_disConnectEvent.owner = shared_from_this(); // ADD_REF

	if (false == SocketUtils::DisconnectEx(_socket, &_disConnectEvent, TF_REUSE_SOCKET, 0))
	{
		int32 errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			//HandleError(errorCode);
			_disConnectEvent.owner = nullptr; // RELEASE_REF

			ProcessDisConnect();
			return false;
		}
	}

	return true;
}

void Session::RegisterRecv()
{
	// 연결이 끊긴 상태라면 수신 등록하지 않음
	if (IsConnected() == false)
	{
		return;
	}

	_recvEvent.Init();
	_recvEvent.eventType = EventType::Recv;
	_recvEvent.owner = shared_from_this(); // ADD_REF

	WSABUF wsaBuf;
	wsaBuf.buf = reinterpret_cast<char*>(_recvBuffer.WritePos());
	wsaBuf.len = _recvBuffer.FreeSize();

	DWORD numOfBytes = 0;
	DWORD flags = 0;

	if (::WSARecv(_socket, &wsaBuf, 1, OUT &numOfBytes, OUT &flags, &_recvEvent, nullptr) == SOCKET_ERROR)
	{
		int32 errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			HandleError(errorCode);
			_recvEvent.owner = nullptr; // RELEASE_REF
		}
	}
}

void Session::RegisterSend()
{
	if (IsConnected() == false)
	{
		return;
	}

	_sendEvent.Init();
	_sendEvent.eventType = EventType::Send;
	_sendEvent.owner = shared_from_this(); // ADD_REF

	// 보낼 데이터를 sendEvent에 등록하기
	{
		WRITE_LOCK;

		int32 writeSize = 0;
		while (_sendQueue.empty() == false)
		{
			SendBufferRef sendBuffer = _sendQueue.front();

			writeSize += sendBuffer->WriteSize();
			//TODO : 너무 크면 나눠서 보내기

			_sendQueue.pop();
			_sendEvent.sendBuffers.push_back(sendBuffer);
		}

	}

	// Scatter-Gather (흩어져 있는 데이터를 뭉쳐서 한번에 보내기)
	xvector<WSABUF> wsaBufs;
	wsaBufs.reserve(_sendEvent.sendBuffers.size());
	for (SendBufferRef sendBuffer : _sendEvent.sendBuffers)
	{
		WSABUF wsaBuf;
		wsaBuf.buf = reinterpret_cast<char*>(sendBuffer->Buffer());
		wsaBuf.len = static_cast<LONG>(sendBuffer->WriteSize());
		wsaBufs.push_back(wsaBuf);
	}

	DWORD numOfBytes = 0;
	if (SOCKET_ERROR == ::WSASend(_socket, wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()), OUT &numOfBytes, 0, &_sendEvent, nullptr))
	{
		int32 errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			HandleError(errorCode);
			_sendEvent.owner = nullptr; // RELEASE_REF
			_sendEvent.sendBuffers.clear(); // 해제
			_sendRegistered.store(false);
		}
	}
}

void Session::ProcessConnect()
{
	_connectEvent.owner = nullptr; // 일단 릴리즈를 함.
	_connected.store(true);

	// 세션 등록
	GetService()->AddSession(GetSessionRef());

	// 컨텐츠 코드 호출
	OnConnected();

	// 수신 등록
	RegisterRecv();
}

void Session::ProcessDisConnect()
{
	_disConnectEvent.owner = nullptr; // 일단 릴리즈를 함.
	cout << "123123123" << endl;
	OnDisconnected();
	GetService()->ReleaseSession(GetSessionRef());
}

void Session::ProcessRecv(int32 numOfBytes)
{
	_recvEvent.owner = nullptr; // 일단 릴리즈를 함.
	if (numOfBytes == 0)
	{
		Disconnect(L"Process Recv: numOfBytes is 0");
		return;
	}

	if (_recvBuffer.OnWrite(numOfBytes) == false)
	{
		Disconnect(L"Process Recv: OnWrite Overflow");
		return;
	}

	int32 dataSize = _recvBuffer.DataSize();
	int32 processLen = OnRecv(_recvBuffer.ReadPos(), dataSize);

	if (processLen < 0 || dataSize < processLen || _recvBuffer.OnRead(processLen) == false)
	{
		Disconnect(L"OnRead Overflow");
		return;
	}

	// 커서 정리
	_recvBuffer.Clean();

	// 낚싯대를 다시 던져야 함. (수신을 다시 걸어야 다음 수신을 받음)
	RegisterRecv();
}

void Session::ProcessSend(int32 numOfBytes)
{
	_sendEvent.owner = nullptr;
	_sendEvent.sendBuffers.clear();

	if (numOfBytes == 0)
	{
		Disconnect(L"Process Send: numOfBytes is 0");
		return;
	}

	// 컨텐츠 코드에서 오버로딩
	OnSend(numOfBytes);

	WRITE_LOCK;
	if (_sendQueue.empty())
	{
		_sendRegistered.store(false);
	}
	else
	{
		RegisterSend();
	}
}

void Session::HandleError(int32 errorCode)
{
	switch (errorCode)
	{
	case WSAECONNRESET:	
	case WSAECONNABORTED:
		Disconnect(L"Handle Error");
		break;
	default:
		// TODO
		cout << "Unhandled error code: " << errorCode << endl;
		break;
	}
}



/*------------------------
	Packet Session
--------------------------*/

PacketSession::PacketSession()
{

}

PacketSession::~PacketSession()
{

}

// [size(2)][id(2)][data....]
int32 PacketSession::OnRecv(BYTE* buffer, int32 len)
{
	int32 processLen = 0;
	while (true)
	{
		int32 dataSize = len - processLen;
		// 최소한 패킷 헤더는 파싱해야함.
		if (dataSize < sizeof(PacketHeader))
		{
			break;
		}

		// 한번 까보기
		PacketHeader header = *(reinterpret_cast<PacketHeader*>(&buffer[processLen]));

		// [중요] 헤더의 size가 sizeof(PacketHeader)보다 작으면 비정상 패킷입니다.
		if (header.size < sizeof(PacketHeader) || header.size > 0xFFFF /* 최대 패킷 크기 설정 */)
		{
			// 비정상 패킷으로 간주하고 처리를 중단하거나 세션을 끊어야 함
			return -1; // -1을 반환하면 Session::ProcessRecv에서 세션을 Close하도록 유도
		}

		// 헤더에 기록된 패킷 크기를 파싱해야 한다.
		if (dataSize < header.size)
		{
			break;
		}

		// 패킷 조립 성공
		OnRecvPacket(&buffer[processLen], header.size);

		processLen += header.size;
	}


	return processLen;
}


