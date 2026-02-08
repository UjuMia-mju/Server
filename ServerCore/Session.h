#pragma once
#include "IocpCore.h"
#include "IocpEvent.h"
#include "NetAdress.h"
#include "RecvBuffer.h"


class Service;
/*----------------------
	Session
------------------------*/

class Session : public IocpObject
{
	friend class Listener;
	friend class IocpCore;
	friend class Service;

	enum
	{
		BUFFER_SIZE = 0x10000, // 64KB
	};

public:
	Session();
	virtual ~Session();

public:
	void				Send(SendBufferRef sendBuffer);
	bool				Connect();
	void				Disconnect(const WCHAR* cause);

	shared_ptr<Service>	GetService() { return _service.lock(); }
	void				SetService(shared_ptr<Service> service) { _service = service; }

public:
						/* 정보 관련 */
	void				SetNetAdress(NetAddress address) { _address = address; }
	NetAddress			GetNetAdress() { return _address; }
	SOCKET				GetSocket() { return _socket; }
	bool				IsConnected() { return _connected; }
						// 자기 자신의 shared_ptr 리턴.
	SessionRef			GetSessionRef() { return static_pointer_cast<Session>(shared_from_this()); }
private:
						/* 인터페이스 구현 */
	virtual HANDLE		GetHandel() override;
	virtual void		Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override;
private:
						/* 전송 관련 */
	bool				RegisterConnect();
	bool				RegisterDisConnect();
	void				RegisterRecv();
	void				RegisterSend();

	void				ProcessConnect();
	void				ProcessDisConnect();
	void				ProcessRecv(int32 numOfBytes);
	void				ProcessSend(int32 numOfBytes);

	void				HandleError(int32 errorCode);
protected:
						/* 컨텐츠 코드에서 오버로딩 관련 */
	virtual void		OnConnected() { }
	virtual int32		OnRecv(BYTE* buffer, int32 len) { return len; }
	virtual void		OnSend(int32 len) { }
	virtual void		OnDisconnected() { }

private:
	weak_ptr<Service>	_service;
	SOCKET				_socket = INVALID_SOCKET;
	NetAddress			_address = { };
	Atomic<bool>		_connected = false;
private:
	USE_LOCK;


	/* Recv 관련 */
	RecvBuffer				_recvBuffer;

	/* Send 관련 */
	xqueue<SendBufferRef>	_sendQueue;
	Atomic<bool>			_sendRegistered = false;
private:
							/* IocpEvent 재사용 */
	ConnectEvent			_connectEvent;
	DisConnectEvent			_disConnectEvent;
	RecvEvent				_recvEvent;
	SendEvent				_sendEvent;
};


/*------------------------
	Packet Session
--------------------------*/

// [size(2)][id(2)][data....]
struct PacketHeader
{
	// 총 4바이트
	uint16		size;
	uint16		id; // 프로토콜 ID (ex. 1 = 로그인, 2 = 이동요청)
};

class PacketSession : public Session
{
public:
	PacketSession();
	virtual ~PacketSession();

	PacketSessionRef GetPacketSessionRef() { return static_pointer_cast<PacketSession>(shared_from_this()); }

protected:
	virtual int32	OnRecv(BYTE* buffer, int32 len) sealed; // 상속 금지
	virtual void	OnRecvPacket(BYTE* buffer, int32 len) = 0; // 순수 가상 함수
};