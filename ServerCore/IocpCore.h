#pragma once

/*----------------------
	IocpObject
------------------------*/

class IocpObject : public enable_shared_from_this<IocpObject>
{
public:
	virtual HANDLE GetHandel() abstract;
	virtual void Dispatch(class IocpEvent* iocpEvent, int32 numOfBytes = 0) abstract;
};

/*----------------------
	IocpCore
	iocp ÇÙ½É Å¬·¡½º
------------------------*/

class IocpCore
{
public:
	IocpCore();
	~IocpCore();

	HANDLE			GetHandle() { return _iocpHandle; }

	bool			Register(IocpObjectRef iocpObj);
	bool			Dispatch(uint32 timeoutMs = INFINITE);

private:
	HANDLE			_iocpHandle;
};
