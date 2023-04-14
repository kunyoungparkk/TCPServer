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
// ���� ������� �뷮 ���.
//
// Parameters: ����.
// Return: (int)������� �뷮.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::GetUseSize(void)
{
	return this->useSize;
}

/////////////////////////////////////////////////////////////////////////
// ���� ���ۿ� ���� �뷮 ���. 
//
// Parameters: ����.
// Return: (int)�����뷮.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::GetFreeSize(void)
{
	return bufferSize - useSize - 1;
}

/////////////////////////////////////////////////////////////////////////
// ���� �����ͷ� �ܺο��� �ѹ濡 �а�, �� �� �ִ� ����.
// (������ ���� ����)
//
// ���� ť�� ������ ������ ���ܿ� �ִ� �����ʹ� �� -> ó������ ���ư���
// 2���� �����͸� ��ų� ���� �� ����. �� �κп��� �������� ���� ���̸� �ǹ�
//
// Parameters: ����.
// Return: (int)��밡�� �뷮.
////////////////////////////////////////////////////////////////////////
int	CRingBuffer::DirectEnqueueSize(void)
{
	//���� - READ - WRITE - ��
	if (writePos >= readPos)
	{
		return bufferSize - writePos;// - 1;
	}
	//���� - WRITE - READ - ��
	else
	{
		return readPos - writePos - 1;
	}
}
int	CRingBuffer::DirectDequeueSize(void)
{
	//���� - READ - WRITE - ��
	if (writePos >= readPos)
	{
		return writePos - readPos;
	}
	//���� - WRITE - READ - ��
	else
	{
		return bufferSize - readPos;// - 1;
	}
}


/////////////////////////////////////////////////////////////////////////
// WritePos �� ����Ÿ ����.
//
// Parameters: (char *)����Ÿ ������. (int)ũ��. 
// Return: (int)���� ũ��.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Enqueue(char* chpData, int iSize)
{
	int directEnqueueSize = DirectEnqueueSize();
	//FreeSize �ʰ��ϸ� ����
	if (iSize > GetFreeSize())
	{
		return -1;
	}
	//�ֱ�
	//�ѹ��� ���� �� �ִ��� Ȯ��
	//�ѹ��� ���� �� �ִٸ� �׳� �ֱ�
	if (directEnqueueSize >= iSize)
	{
		memcpy_s(&ringBuffer[writePos], iSize, chpData, iSize);
	}
	//�ι��� ���� �� ������ ���� �ֱ�
	else
	{
		memcpy_s(&ringBuffer[writePos], directEnqueueSize, chpData, directEnqueueSize);
		memcpy_s(&ringBuffer[0], iSize - directEnqueueSize, chpData + directEnqueueSize, iSize - directEnqueueSize);
	}
	//���� ����
	writePos = (writePos + iSize) % bufferSize;
	useSize += iSize;
	return iSize;
}

/////////////////////////////////////////////////////////////////////////
// ReadPos ���� ����Ÿ ������. ReadPos �̵�.
//
// Parameters: (char *)����Ÿ ������. (int)ũ��.
// Return: (int)������ ũ��.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Dequeue(char* chpDest, int iSize)
{
	int directDequeueSize = DirectDequeueSize();
	//UseSize �ʰ��ϸ� ����
	if (iSize > GetUseSize())
	{
		return -1;
	}
	//����
	//�ѹ��� �� �� �ִ� ���
	if (directDequeueSize >= iSize)
	{
		memcpy_s(chpDest, iSize, &ringBuffer[readPos], iSize);
	}
	//�ι��� ���� ���� ���
	else
	{
		memcpy_s(chpDest, directDequeueSize, &ringBuffer[readPos], directDequeueSize);
		memcpy_s(chpDest + directDequeueSize, iSize - directDequeueSize, &ringBuffer[0], iSize - directDequeueSize);
	}
	//���� ����
	readPos = (readPos + iSize) % bufferSize;
	useSize -= iSize;
	return iSize;
}

/////////////////////////////////////////////////////////////////////////
// ReadPos ���� ����Ÿ �о��. ReadPos ����.
//
// Parameters: (char *)����Ÿ ������. (int)ũ��.
// Return: (int)������ ũ��.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Peek(char* chpDest, int iSize)
{
	int directDequeueSize = DirectDequeueSize();
	int tempReadPos = readPos;
	//UseSize �ʰ��ϸ� ����(�ӽ�)
	if (iSize > GetUseSize())
	{
		return -1;
	}
	//����
	//�ѹ��� �� �� �ִ� ���
	if (directDequeueSize >= iSize)
	{
		memcpy_s(chpDest, iSize, &ringBuffer[tempReadPos], iSize);
	}
	//�ι��� ���� ���� ���
	else
	{
		memcpy_s(chpDest, directDequeueSize, &ringBuffer[tempReadPos], directDequeueSize);
		memcpy_s(chpDest + directDequeueSize, iSize - directDequeueSize, &ringBuffer[0], iSize - directDequeueSize);
	}
	return iSize;
}


/////////////////////////////////////////////////////////////////////////
// ���ϴ� ���̸�ŭ �б���ġ ���� ���� / ���� ��ġ �̵�
//
// Parameters: ����.
// Return: (int)�̵�ũ��
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
// ������ ��� ����Ÿ ����.
//
// Parameters: ����.
// Return: ����.
/////////////////////////////////////////////////////////////////////////
void	CRingBuffer::ClearBuffer(void)
{
	this->MoveFront(GetUseSize());
}


/////////////////////////////////////////////////////////////////////////
// ������ Front ������ ����.
//
// Parameters: ����.
// Return: (char *) ���� ������.
/////////////////////////////////////////////////////////////////////////
char* CRingBuffer::GetFrontBufferPtr(void)
{
	return ringBuffer + readPos;
}


/////////////////////////////////////////////////////////////////////////
// ������ RearPos ������ ����.
//
// Parameters: ����.
// Return: (char *) ���� ������.
/////////////////////////////////////////////////////////////////////////
char* CRingBuffer::GetRearBufferPtr(void)
{
	return ringBuffer + writePos;
}
