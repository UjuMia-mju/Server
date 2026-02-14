#pragma once

class Session;

enum class EventType : uint8
{
	Connect, DisConnect,
	Accept,
	//PreRecv,
	Recv,
	Send
};

/*----------------------
	IocpEvent
	어떤 이벤트인지 구분하기 위한 기본 클래스 (즉, 어떤 일감인지 나타냄.)
------------------------*/
// 중요!! OVERLAPPED를 상속받는 클래스는 절대로 "가상" 함수(소멸자 포함)를 가질 수 없다.
// 이유는 가상함수를 가지게 되면 vtable이 생성되는데, vtable은 offset 0에 위치하게 되고
// OVERLAPPED 구조체는 offset 0에 위치를 기대하기 때문에 꼬이게 된다.
class IocpEvent : public OVERLAPPED
{
public:
	IocpEvent(EventType type);

	void			Init();

public:
	EventType		eventType;
	IocpObjectRef	owner;
};


/*----------------------
	connectEvent
------------------------*/

class ConnectEvent : public IocpEvent
{
public:
	ConnectEvent() : IocpEvent(EventType::Connect) {}
};

/*----------------------
	disConnectEvent
------------------------*/

class DisConnectEvent : public IocpEvent
{
public:
	DisConnectEvent() : IocpEvent(EventType::DisConnect) {}
};

/*----------------------
	AcceptEvent
------------------------*/

class AcceptEvent : public IocpEvent
{
public:
	AcceptEvent() : IocpEvent(EventType::Accept) {}
public:
	SessionRef session = nullptr;
};

/*----------------------
	RecvEvent
------------------------*/

class RecvEvent : public IocpEvent
{
public:
	RecvEvent() : IocpEvent(EventType::Recv) {}
};

/*----------------------
	SendEvent
------------------------*/

class SendEvent : public IocpEvent
{
public:
	SendEvent() : IocpEvent(EventType::Send) {}

	xvector<SendBufferRef> sendBuffers;
};