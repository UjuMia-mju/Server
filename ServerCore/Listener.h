#pragma once
#include "IocpCore.h"
#include "NetAdress.h"

class AcceptEvent; // 전방 선언
class ServerService;

/*----------------------
	Listener

	역할: Listen 소켓 관리
	AcceptEx 등록/재등록
	새 Session을 생성하고, 연결이 완료되면 Session에게 바통을 넘김
------------------------*/

class Listener : public IocpObject
{
public:
	Listener() = default;
	~Listener();

public:
	bool StartAccept(ServerServiceRef server);
	void CloseAccept();

	virtual HANDLE GetHandel() override;
	virtual void Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override;

private:
	// 수신 관련
	void RegisterAccept(AcceptEvent* acceptEvent);
	void ProcessAccept(AcceptEvent* acceptEvent);

protected:
	SOCKET _listenSocket = INVALID_SOCKET;
	xvector<AcceptEvent*> _acceptEventPool;
	ServerServiceRef _serverService;
};

