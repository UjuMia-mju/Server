#pragma once
#include "NetAdress.h"
#include "IocpCore.h"
#include "Listener.h"
#include <functional>

enum class ServiceType : uint8
{
	Server,
	Client
};

/*------------------
	Service
	역할: 서비스의 기본 클래스 (서버, 클라이언트 공통)
----------------*/

using SessionFactory = function<SessionRef(void)>;

class Service : public enable_shared_from_this<Service>
{
public:
	Service(ServiceType type, NetAddress address, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount = 1);
	virtual ~Service();

	virtual bool	Start() = 0;
	bool			CanStart() { return (_sessionFactory != nullptr); }

	virtual void	CloseService();
	void			SetSessionFactory(SessionFactory func) { _sessionFactory = func; };

	void			Brodcast(SendBufferRef sendBuffer);
	SessionRef		CreateSession();
	void			AddSession(SessionRef session);
	void			ReleaseSession(SessionRef session);
	int32			GetCurrentSessionCount() { return _sessionCount; }
	int32			GetMaxSessionCount() { return _maxSessionCount; }
public:
	ServiceType		GetServiceType() { return _serviceType; }
	NetAddress		GetNetAddress() { return _netAddress; }
	IocpCoreRef		GetIocpCore() { return _iocpCore; }

protected:
	USE_LOCK;

	ServiceType			_serviceType;
	NetAddress			_netAddress;
	IocpCoreRef			_iocpCore;
	
	xset<SessionRef>	_sessions; // 지금까지 연결된 세션들.
	int32 				_sessionCount = 0; // 현재 활성화된 세션 수
	int32 				_maxSessionCount = 0; // 최대 세션 수
	SessionFactory		_sessionFactory; // 세션을 생성하는 함수
};

/*---------------------
	Client Service
----------------------*/

class ClientService : public Service
{
public:
	ClientService(NetAddress targetAddress, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount = 1);
	virtual			~ClientService();

	virtual bool	Start() override;
};


/*---------------------
	Server Service
----------------------*/

class ServerService : public Service
{
public:
	ServerService(NetAddress targetAddress, IocpCoreRef core, SessionFactory factory, int32 maxSessionCount = 1);
	virtual			~ServerService();

	virtual bool	Start() override;
	virtual void	CloseService() override;
private:
	ListenerRef		_listener = nullptr;
};