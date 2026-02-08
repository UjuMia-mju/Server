#pragma once
#include <stack>

/*-----------------
	TLS(Thread Local Storage) - 스레드 별로 가지고 있는 영역
-------------------*/

//TLS
extern thread_local uint16				LthreadId;
extern thread_local uint64				LEndTickCount; // 틱이 완료되는 시간

extern thread_local std::stack<int32>	LLockStack;
extern thread_local SendBufferChunkRef	LSendBufferChunk;

// 현재 스레드가 실행중인 Job Queue가 있는지 관리하기 위한 변수
extern thread_local class JobQueue*		LCurrentJobQueue;