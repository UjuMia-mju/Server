#include "pch.h"
#include "CoreTLS.h"

thread_local uint16					LthreadId = 0;
thread_local uint64					LEndTickCount = 0; // 틱이 완료되는 시간

thread_local stack<int32>			LLockStack;
thread_local SendBufferChunkRef		LSendBufferChunk;
thread_local JobQueue*				LCurrentJobQueue = nullptr;