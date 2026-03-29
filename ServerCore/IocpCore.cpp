#include "pch.h"
#include "IocpCore.h"
#include "IocpEvent.h"

/*----------------------
	IocpCore
------------------------*/

IocpCore::IocpCore()
{
	_iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	ASSERT_CRASH(_iocpHandle != INVALID_HANDLE_VALUE);
}

IocpCore::~IocpCore()
{
	::CloseHandle(_iocpHandle);
}

bool IocpCore::Register(IocpObjectRef iocpObj)
{
	return ::CreateIoCompletionPort(iocpObj->GetHandle(), _iocpHandle, /*key*/0, 0);
}

bool IocpCore::Dispatch(uint32 timeoutMs)
{
	DWORD numOfBytes = 0;
	ULONG_PTR key = 0;
	IocpEvent* iocpEvent = nullptr;

	if (::GetQueuedCompletionStatus(_iocpHandle, OUT &numOfBytes, OUT &key, OUT reinterpret_cast<LPOVERLAPPED*>(&iocpEvent), timeoutMs))
	{
		IocpObjectRef iocpObject = iocpEvent->owner;
		iocpObject->Dispatch(iocpEvent, numOfBytes);
	}
	else
	{
		int32 errorCode = ::WSAGetLastError(); // 경우에 따라 에러가 에러가 아닐 수 있음. (타임아웃이면 정상 처리)
		switch (errorCode)
		{
		case WAIT_TIMEOUT:
		{
			return false;
		}
		default:
			//ASSERT_CRASH(false);
			IocpObjectRef iocpObject = iocpEvent->owner;
			iocpObject->Dispatch(iocpEvent, numOfBytes);
			break;
		}
	}

	return true;
}
