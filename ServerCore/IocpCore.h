#pragma once

/*----------------------
	IocpObject
	iocpCore에 등록될 객체의 기본 클래스
------------------------*/

class IocpObject : public enable_shared_from_this<IocpObject>
{
public:
	virtual HANDLE	GetHandle() abstract;
	virtual void	Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) abstract;
};

/*----------------------
	IocpCore
	iocp 핵심 클래스
------------------------*/

class IocpCore
{
public:
	IocpCore();
	~IocpCore();

	HANDLE			GetHandle() { return _iocpHandle; }

	bool			Register(IocpObjectRef iocpObj); // 일감 대기 시키기.
	bool			Dispatch(uint32 timeoutMs = INFINITE); // 일감 처리할꺼 있는지 확인.

private:
	HANDLE			_iocpHandle;
};
