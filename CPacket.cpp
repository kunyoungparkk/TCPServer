#include <iostream>
#include "CPacket.h"
CPacket::CPacket()
{
	Init(eBUFFER_DEFAULT);
}

CPacket::CPacket(int iBufferSize)
{
	Init(iBufferSize);
}

void CPacket::Init(int bufferSize)
{
	m_chpBuffer = (char*)malloc(bufferSize);
	m_iBufferSize = bufferSize;
	m_iDataSize = 0;
	writePos = 0;
	readPos = 0;
}
int	CPacket::Resize()
{
	int newSize = m_iBufferSize * RESIZE_MULTIPLIER;
	char* newBuffer = (char*)malloc(newSize);
	if (newBuffer == NULL)
	{
		exit(1);
	}
	memcpy_s(newBuffer, newSize, m_chpBuffer, m_iDataSize);
	free(m_chpBuffer);
	m_iBufferSize = newSize;
	m_chpBuffer = newBuffer;
}

CPacket::~CPacket()
{
	free(m_chpBuffer);
}

void CPacket::Clear(void)
{
	readPos = 0;
	writePos = 0;
	m_iDataSize = 0;
}

int CPacket::MoveWritePos(int iSize)
{
	writePos += iSize;
	m_iDataSize += iSize;
	return iSize;
}

int CPacket::MoveReadPos(int iSize)
{
	readPos += iSize;
	m_iDataSize -= iSize;
	return iSize;
}

CPacket& CPacket::operator=(CPacket& clSrcPacket)
{
	free(m_chpBuffer);
	this->readPos = clSrcPacket.readPos;
	this->writePos = clSrcPacket.writePos;
	this->m_chpBuffer = clSrcPacket.m_chpBuffer;
	this->m_iBufferSize = clSrcPacket.m_iBufferSize;
	this->m_iDataSize = clSrcPacket.m_iDataSize;
	return *this;
}

CPacket& CPacket::operator<<(BYTE byValue)
{
	PutData((char*)&byValue, sizeof(BYTE));
	return *this;
}

CPacket& CPacket::operator<<(char chValue)
{
	PutData((char*)&chValue, sizeof(char));
	return *this;
}

CPacket& CPacket::operator<<(short shValue)
{
	PutData((char*)&shValue, sizeof(short));
	return *this;
}

CPacket& CPacket::operator<<(WORD wValue)
{
	PutData((char*)&wValue, sizeof(WORD));
	return *this;
}

CPacket& CPacket::operator<<(int iValue)
{
	PutData((char*)&iValue, sizeof(int));
	return *this;
}

CPacket& CPacket::operator<<(DWORD lValue)
{
	PutData((char*)&lValue, sizeof(DWORD));
	return *this;
}

CPacket& CPacket::operator<<(float fValue)
{
	PutData((char*)&fValue, sizeof(float));
	return *this;
}

CPacket& CPacket::operator<<(__int64 iValue)
{
	PutData((char*)&iValue, sizeof(__int64));
	return *this;
}

CPacket& CPacket::operator<<(double dValue)
{
	PutData((char*)&dValue, sizeof(double));
	return *this;
}

CPacket& CPacket::operator>>(BYTE& byValue)
{
	GetData((char*)&byValue, sizeof(BYTE));
	return *this;
}

CPacket& CPacket::operator>>(char& chValue)
{
	GetData((char*)&chValue, sizeof(char));
	return *this;
}

CPacket& CPacket::operator>>(short& shValue)
{
	GetData((char*)&shValue, sizeof(short));
	return *this;
}

CPacket& CPacket::operator>>(WORD& wValue)
{
	GetData((char*)&wValue, sizeof(WORD));
	return *this;
}

CPacket& CPacket::operator>>(int& iValue)
{
	GetData((char*)&iValue, sizeof(int));
	return *this;
}

CPacket& CPacket::operator>>(DWORD& dwValue)
{
	GetData((char*)&dwValue, sizeof(DWORD));
	return *this;
}

CPacket& CPacket::operator>>(float& fValue)
{
	GetData((char*)&fValue, sizeof(float));
	return *this;
}

CPacket& CPacket::operator>>(__int64& iValue)
{
	GetData((char*)&iValue, sizeof(__int64));
	return *this;
}

CPacket& CPacket::operator>>(double& dValue)
{
	GetData((char*)&dValue, sizeof(double));
	return *this;
}

int CPacket::GetData(char* chpDest, int iSize)
{
	if (iSize > m_iDataSize)
	{
		return -1;
	}
	//복사
	memcpy(chpDest, m_chpBuffer + readPos, iSize);
	readPos += iSize;
	m_iDataSize -= iSize;
	return iSize;
}

int CPacket::PutData(char* chpSrc, int iSrcSize)
{
	//넘치면 Resize
	while(iSrcSize > m_iBufferSize - writePos)
	{
		wprintf(L"!!!!RESIZED!!!!\n");
		Resize();
	}
	memcpy(m_chpBuffer + writePos, chpSrc, iSrcSize);
	writePos += iSrcSize;
	m_iDataSize += iSrcSize;
	return iSrcSize;
}


