#include <iostream>
#include "RingBuffer.h"

CRingBuffer::CRingBuffer(void)
{
	this->CRingBuffer::CRingBuffer(DEFAULT_BUFFER_SIZE);
}
CRingBuffer::CRingBuffer(int iBufferSize)
{
	this->ringBuffer = (char*)malloc(iBufferSize);
	this->useSize = 0;
	this->bufferSize = iBufferSize;
	this->readPos = 0;
	this->writePos = 0;
}
CRingBuffer::~CRingBuffer()
{
	free(this->ringBuffer);
}

int	CRingBuffer::GetBufferSize(void)
{
	return this->bufferSize;
}

/////////////////////////////////////////////////////////////////////////
// 현재 사용중인 용량 얻기.
//
// Parameters: 없음.
// Return: (int)사용중인 용량.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::GetUseSize(void)
{
	return this->useSize;
}

/////////////////////////////////////////////////////////////////////////
// 현재 버퍼에 남은 용량 얻기. 
//
// Parameters: 없음.
// Return: (int)남은용량.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::GetFreeSize(void)
{
	return bufferSize - useSize - 1;
}

/////////////////////////////////////////////////////////////////////////
// 버퍼 포인터로 외부에서 한방에 읽고, 쓸 수 있는 길이.
// (끊기지 않은 길이)
//
// 원형 큐의 구조상 버퍼의 끝단에 있는 데이터는 끝 -> 처음으로 돌아가서
// 2번에 데이터를 얻거나 넣을 수 있음. 이 부분에서 끊어지지 않은 길이를 의미
//
// Parameters: 없음.
// Return: (int)사용가능 용량.
////////////////////////////////////////////////////////////////////////
int	CRingBuffer::DirectEnqueueSize(void)
{
	//시작 - READ - WRITE - 끝
	if (writePos >= readPos)
	{
		return bufferSize - writePos;// - 1;
	}
	//시작 - WRITE - READ - 끝
	else
	{
		return readPos - writePos - 1;
	}
}
int	CRingBuffer::DirectDequeueSize(void)
{
	//시작 - READ - WRITE - 끝
	if (writePos >= readPos)
	{
		return writePos - readPos;
	}
	//시작 - WRITE - READ - 끝
	else
	{
		return bufferSize - readPos;// - 1;
	}
}


/////////////////////////////////////////////////////////////////////////
// WritePos 에 데이타 넣음.
//
// Parameters: (char *)데이타 포인터. (int)크기. 
// Return: (int)넣은 크기.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Enqueue(char* chpData, int iSize)
{
	int directEnqueueSize = DirectEnqueueSize();
	//FreeSize 초과하면 오류
	if (iSize > GetFreeSize())
	{
		return -1;
	}
	//넣기
	//한번에 넣을 수 있는지 확인
	//한번에 넣을 수 있다면 그냥 넣기
	if (directEnqueueSize >= iSize)
	{
		memcpy_s(&ringBuffer[writePos], iSize, chpData, iSize);
	}
	//두번에 넣을 수 있으면 나눠 넣기
	else
	{
		memcpy_s(&ringBuffer[writePos], directEnqueueSize, chpData, directEnqueueSize);
		memcpy_s(&ringBuffer[0], iSize - directEnqueueSize, chpData + directEnqueueSize, iSize - directEnqueueSize);
	}
	//변수 정리
	writePos = (writePos + iSize) % bufferSize;
	useSize += iSize;
	return iSize;
}

/////////////////////////////////////////////////////////////////////////
// ReadPos 에서 데이타 가져옴. ReadPos 이동.
//
// Parameters: (char *)데이타 포인터. (int)크기.
// Return: (int)가져온 크기.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Dequeue(char* chpDest, int iSize)
{
	int directDequeueSize = DirectDequeueSize();
	//UseSize 초과하면 에러
	if (iSize > GetUseSize())
	{
		return -1;
	}
	//빼기
	//한번에 뺄 수 있는 경우
	if (directDequeueSize >= iSize)
	{
		memcpy_s(chpDest, iSize, &ringBuffer[readPos], iSize);
	}
	//두번에 나눠 빼는 경우
	else
	{
		memcpy_s(chpDest, directDequeueSize, &ringBuffer[readPos], directDequeueSize);
		memcpy_s(chpDest + directDequeueSize, iSize - directDequeueSize, &ringBuffer[0], iSize - directDequeueSize);
	}
	//변수 정리
	readPos = (readPos + iSize) % bufferSize;
	useSize -= iSize;
	return iSize;
}

/////////////////////////////////////////////////////////////////////////
// ReadPos 에서 데이타 읽어옴. ReadPos 고정.
//
// Parameters: (char *)데이타 포인터. (int)크기.
// Return: (int)가져온 크기.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Peek(char* chpDest, int iSize)
{
	int directDequeueSize = DirectDequeueSize();
	int tempReadPos = readPos;
	//UseSize 초과하면 에러(임시)
	if (iSize > GetUseSize())
	{
		return -1;
	}
	//빼기
	//한번에 뺄 수 있는 경우
	if (directDequeueSize >= iSize)
	{
		memcpy_s(chpDest, iSize, &ringBuffer[tempReadPos], iSize);
	}
	//두번에 나눠 빼는 경우
	else
	{
		memcpy_s(chpDest, directDequeueSize, &ringBuffer[tempReadPos], directDequeueSize);
		memcpy_s(chpDest + directDequeueSize, iSize - directDequeueSize, &ringBuffer[0], iSize - directDequeueSize);
	}
	return iSize;
}


/////////////////////////////////////////////////////////////////////////
// 원하는 길이만큼 읽기위치 에서 삭제 / 쓰기 위치 이동
//
// Parameters: 없음.
// Return: (int)이동크기
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::MoveRear(int iSize)
{
	writePos = (writePos + iSize) % (bufferSize);
	useSize += iSize;
	return iSize;
}
int	CRingBuffer::MoveFront(int iSize)
{
	readPos = (readPos + iSize) % (bufferSize);
	useSize -= iSize;
	return iSize;
}

/////////////////////////////////////////////////////////////////////////
// 버퍼의 모든 데이타 삭제.
//
// Parameters: 없음.
// Return: 없음.
/////////////////////////////////////////////////////////////////////////
void	CRingBuffer::ClearBuffer(void)
{
	this->MoveFront(GetUseSize());
}


/////////////////////////////////////////////////////////////////////////
// 버퍼의 Front 포인터 얻음.
//
// Parameters: 없음.
// Return: (char *) 버퍼 포인터.
/////////////////////////////////////////////////////////////////////////
char* CRingBuffer::GetFrontBufferPtr(void)
{
	return ringBuffer + readPos;
}


/////////////////////////////////////////////////////////////////////////
// 버퍼의 RearPos 포인터 얻음.
//
// Parameters: 없음.
// Return: (char *) 버퍼 포인터.
/////////////////////////////////////////////////////////////////////////
char* CRingBuffer::GetRearBufferPtr(void)
{
	return ringBuffer + writePos;
}
