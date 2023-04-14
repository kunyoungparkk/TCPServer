#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <list>
#include <map>
#include "RingBuffer.h"
#include "PacketDefine.h"

#define		SERVERPORT	5000 //20000
#define		FPS			25
#define		WIDTH		640	//6400
#define		HEIGHT		480	//6400
#define		BUFSIZE		10000

//-----------------------------------------------------------------
// 30�� �̻��� �ǵ��� �ƹ��� �޽��� ���ŵ� ���°�� ���� ����.
//-----------------------------------------------------------------
#define dfNETWORK_PACKET_RECV_TIMEOUT	30000

//ȭ�� �̵� ����
#define dfRANGE_MOVE_TOP	50//0
#define dfRANGE_MOVE_LEFT	10//0
#define dfRANGE_MOVE_RIGHT	630//6400
#define dfRANGE_MOVE_BOTTOM	470//6400

//�̵� �ӵ�
#define		MOVE_SPEED_X	6
#define		MOVE_SPEED_Y	4

//�̵� ���� üũ ����
#define		dfERROR_RANGE	50

#define		MAX_HP		100

//---------------------------------------------------------------
// ���ݹ���.
//---------------------------------------------------------------
#define dfATTACK1_RANGE_X		80
#define dfATTACK2_RANGE_X		90
#define dfATTACK3_RANGE_X		100
#define dfATTACK1_RANGE_Y		10
#define dfATTACK2_RANGE_Y		10
#define dfATTACK3_RANGE_Y		20

//---------------------------------------------------------------
// ���� ������.
//---------------------------------------------------------------
#define dfATTACK1_DAMAGE		1
#define dfATTACK2_DAMAGE		2
#define dfATTACK3_DAMAGE		3

int		g_iLogLevel = 0;
WCHAR	g_szLogBuff[1024];

#define dfLOG_LEVEL_DEBUG		0
#define dfLOG_LEVEL_ERROR		1
#define dfLOG_LEVEL_SYSTEM		2

void Log(WCHAR* szString, int iLogLevel)
{
	wprintf(L"%s\n", szString);
}
#define _LOG(LogLevel, fmt, ...)					\
do {												\
	if(g_iLogLevel <= LogLevel)						\
	{												\
		wsprintf(g_szLogBuff, fmt, ##__VA_ARGS__);	\
		Log(g_szLogBuff, LogLevel);					\
	}												\
} while (0);										\

struct Session
{
	SOCKET			sock;
	CRingBuffer*	recvQueue;
	CRingBuffer*	sendQueue;
	DWORD			dwSessionID;//=userID�� ����.
	DWORD			dwLastRecvTime;
};
struct USER
{
	Session* pSession;
	DWORD	userID;
	BOOL	isMove;
	BYTE	Direction;
	short	x;
	short	y;
	char	hp;
};
//���� ����Ʈ
std::map<int, Session*> sessionMap;
std::list<Session*> disconnectSessions;
//ĳ���� ����
std::map<int, USER*> userMap;

int allocatingID = 0;
int retVal;

int acceptRetVal;
int recvRetVal;
int	sendRetVal;
int enqueueRetVal;
int dequeueRetVal;

int FPSCount = 0;
int recvCount = 0; 
int sendCount = 0;
inline BOOL IsValidPosition(WORD prevX, WORD prevY, WORD nextX, WORD nextY)
{
	if (abs(prevX - nextX) > dfERROR_RANGE || abs(prevY - nextY) > dfERROR_RANGE)
	{
		return FALSE;
	}
	else return TRUE;
}

//��Ʈ��ũ
void Disconnect(Session* session)
{
	disconnectSessions.push_back(session);
}
Session* FindSession(SOCKET sock)
{
	for (auto iter = sessionMap.begin(); iter != sessionMap.end(); iter++)
	{
		if (iter->second->sock == sock)
		{
			return iter->second;
		}
	}
	return NULL;
}
void InsertSession(int sessionID, Session* session)
{
	sessionMap.insert({ sessionID, session });
}
void DeleteSession(int sessionID)
{
	sessionMap.erase(sessionID);
}
void SendUnicast(Session* session, CPacket* clpPacket);
void SendBroadcast(Session* exceptSession, CPacket* clpPacket);
void NetworkProc(SOCKET listen_sock);
void NetSelectProc(SOCKET* sockets, fd_set* rset, fd_set* wset);
void AcceptProc(SOCKET listen_sock);
void RecvProc(SOCKET socket);
void SendProc(SOCKET socket);

//������
USER* FindUser(int userID)
{
	//���� �˾Ƴ���
	auto iter = userMap.find(userID);
	if (iter == userMap.end())
	{
		_LOG(dfLOG_LEVEL_ERROR, L"!!FindUser Failed. USERID: %d", userID);
		return NULL;
	}
	return iter->second;
}
void InsertUser(int userID, USER* user)
{
	userMap.insert({ userID, user });
}
void DeleteUser(int userID)
{
	userMap.erase(userID);
}
void MoveUser(USER* user);
BOOL IsHit(BYTE atkType, BOOL isLeft, WORD attackerX, WORD attackerY, WORD targetX, WORD targetY);
void CollisionProc(BYTE atkType, USER* atkUser);

//���� ��Ŷ ������ ó��
void RecvPacketProc_MoveStart(Session* session, CPacket * clpPacket);
void RecvPacketProc_MoveStop(Session* session, CPacket* clpPacket);
void RecvPacketProc_Attack1(Session* session, CPacket* clpPacket);
void RecvPacketProc_Attack2(Session* session, CPacket* clpPacket);
void RecvPacketProc_Attack3(Session* session, CPacket* clpPacket);

int wmain(int argc, WCHAR* argv[])
{
	timeBeginPeriod(1);
	//1�ʸ��� Ÿ�̸�
	DWORD statusTimer = timeGetTime();
	//������ �ð� �����ִ�(20ms) �뵵�� ����
	DWORD startTime = timeGetTime();
	DWORD currentTime;
	
	//���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;
	else _LOG(dfLOG_LEVEL_SYSTEM,L"WSAStartup #");
	//listen socket
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	//bind
	SOCKADDR_IN serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	InetPton(AF_INET, L"0.0.0.0", &serveraddr.sin_addr);
	serveraddr.sin_port = htons(SERVERPORT);
	retVal = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retVal == SOCKET_ERROR) return 1;
	else _LOG(dfLOG_LEVEL_SYSTEM, L"BIND OK# PORT:%d", SERVERPORT);
	//listen
	retVal = listen(listen_sock, SOMAXCONN);
	if (retVal == SOCKET_ERROR) return 1;
	else _LOG(dfLOG_LEVEL_SYSTEM, L"LISTEN OK #");

	//���� �������� �����
	u_long isOn = 1;
	retVal = ioctlsocket(listen_sock, FIONBIO, &isOn);
	if(retVal == SOCKET_ERROR) return 1;

	//LINGER �ɼ����� closesocket �� RST ������
	struct linger optVal;
	optVal.l_onoff = 1;
	optVal.l_linger = 0;
	setsockopt(listen_sock, SOL_SOCKET, SO_LINGER, (const char*)&optVal, sizeof(optVal));

	//LOOP
	while (1)
	{
		//��Ʈ��ũ IO
		NetworkProc(listen_sock);
		
		//���� - 50������ ���߱�
		currentTime = timeGetTime();
		if (currentTime - startTime >= 1000 / FPS)
		{
			startTime = currentTime;
			//���� (�̵�)
			for (std::map<int, USER*>::iterator iter = userMap.begin(); iter != userMap.end(); iter++)
			{
				MoveUser(iter->second);
			}
			FPSCount++;
		}
		//1�ʸ��� ���� ���� PRINT
		if (currentTime - statusTimer >= 1000)
		{
			statusTimer = currentTime;
			_LOG(dfLOG_LEVEL_SYSTEM, L"[STATUS] FPS: %d / userCount: %d / RTPS: %d / STPS: %d", FPSCount, sessionMap.size(), recvCount, sendCount);
			FPSCount = 0;
			recvCount = 0;
			sendCount = 0;
		}
	}
	//����
	closesocket(listen_sock);
	WSACleanup();
	return 0;
}

//��Ʈ��ũ
void NetworkProc(SOCKET listen_sock)
{
	Session* pSession;
	SOCKET	socketTable[FD_SETSIZE] = { INVALID_SOCKET, };
	int		socketCount = 0;

	//SELECT MODEL
	fd_set rset, wset;
	FD_ZERO(&rset);
	FD_ZERO(&wset);
	//���� ���� �ֱ�
	FD_SET(listen_sock, &rset);
	socketTable[socketCount] = listen_sock;
	socketCount++;
	//���� ���� �� ���� ���� ��� Ŭ���̾�Ʈ�� ���� SOCKET üũ
	
	//rset, wset�� ���ϵ� �ֱ�
	for (auto iter = sessionMap.begin(); iter != sessionMap.end(); iter++)
	{
		pSession = iter->second;
		socketTable[socketCount] = pSession->sock;
		FD_SET(pSession->sock, &rset);
		if (pSession->sendQueue->GetUseSize() > 0)
		{
			FD_SET(pSession->sock, &wset);
		}
		socketCount++;
		//select �ִ�ġ ���� ��
		if (FD_SETSIZE <= socketCount)
		{
			NetSelectProc(socketTable, &rset, &wset);
			FD_ZERO(&rset);
			FD_ZERO(&wset);
			memset(socketTable, INVALID_SOCKET, sizeof(SOCKET) * FD_SETSIZE);
			FD_SET(listen_sock, &rset);
			socketTable[0] = listen_sock;
			socketCount = 1;
		}
	}
	if (socketCount > 0)
	{
		NetSelectProc(socketTable, &rset, &wset);
	}

	//DISCONNECT
	for (std::list<Session*>::iterator iter = disconnectSessions.begin(); iter != disconnectSessions.end(); iter++)
	{
		CPacket cPacket;
		SetPacket_DeleteCharacter(&cPacket, (*iter)->dwSessionID);
		//���� ����ڿ��Ե� ����
		SendBroadcast(NULL, &cPacket);
		_LOG(dfLOG_LEVEL_DEBUG, L"Disconnect # Session ID: %d", (*iter)->dwSessionID);
		//����Ʈ���� ����
		USER* user = FindUser((*iter)->dwSessionID);
		DeleteUser(user->userID);
		free(user);
		DeleteSession((*iter)->dwSessionID);

		closesocket((*iter)->sock);
		delete((*iter)->recvQueue);
		delete((*iter)->sendQueue);
		free(*iter);
	}
	disconnectSessions.clear();
}
void NetSelectProc(SOCKET* sockets, fd_set* pRset, fd_set* pWset)
{
	//Session* pSession;
	int selectCount;
	timeval timeZero;
	timeZero.tv_sec = 0;
	timeZero.tv_usec = 0;
	selectCount = select(0, pRset, pWset, NULL, &timeZero);
	if (selectCount > 0)
	{
		//accept�ÿ�, 0��°�� �׻� listen socket
		if (FD_ISSET(sockets[0], pRset))
		{
			AcceptProc(sockets[0]);
		}
		//RECV&SEND
		for (int i=1; i<FD_SETSIZE; i++)
		{
			if (sockets[i] == INVALID_SOCKET)
				break;
			if (FD_ISSET(sockets[i], pRset))
			{
				RecvProc(sockets[i]);
				recvCount++;
			}
			if (FD_ISSET(sockets[i], pWset))
			{
				SendProc(sockets[i]);
				sendCount++;
			}
		}
	}
	else if (selectCount == SOCKET_ERROR)
	{
		_LOG(dfLOG_LEVEL_ERROR, L"####SELECT ERROR -> ERROR CODE: %d", WSAGetLastError());
		exit(1);
	}
}
BOOL IsDisconnected(Session* session)
{
	for (std::list<Session*>::iterator iter = disconnectSessions.begin(); iter != disconnectSessions.end(); iter++)
	{
		if (session == *iter)
		{
			return TRUE;
		}
	}
	return FALSE;
}
int sendUnicastRet;
int sendBroadcastRet;
void SendUnicast(Session* session, CPacket * clpPacket)
{
	sendUnicastRet = session->sendQueue->Enqueue(clpPacket->GetBufferPtr(), clpPacket->GetDataSize());
	if (sendUnicastRet == -1)
	{
		_LOG(dfLOG_LEVEL_ERROR, L"SENDQUEUE ENQUEUE FAILED : SendUnicast, Session ID: %d", session->dwSessionID);
		Disconnect(session);
	}
	//���ص� �ȴ�(��ȸ��)
	clpPacket->MoveReadPos(sendUnicastRet);
}
void SendBroadcast(Session* exceptSession, CPacket * clpPacket)
{
	Session* pSession;
	for (auto iter = sessionMap.begin(); iter != sessionMap.end(); iter++)
	{
		pSession = iter->second;
		if (pSession == exceptSession)
			continue;
		sendBroadcastRet = pSession->sendQueue->Enqueue(clpPacket->GetBufferPtr(), clpPacket->GetDataSize());
		if (sendBroadcastRet == -1)
		{
			_LOG(dfLOG_LEVEL_ERROR, L"SENDQUEUE ENQUEUE FAILED : SendBroadcast, Session ID: %d, packetSize: %d", 
				pSession->dwSessionID, clpPacket->GetDataSize());
			Disconnect(pSession);
		}
	}
	//���ص� �ȴ�(��ȸ��)
	clpPacket->MoveReadPos(sendBroadcastRet);
}
void AcceptProc(SOCKET listen_sock)
{
	SOCKADDR_IN clientaddr;
	WCHAR clientaddrStr[20] = { 0 };
	//�ű� ����
	Session* session = (Session*)malloc(sizeof(Session));
	//�ű� ����
	USER* user = (USER*)malloc(sizeof(USER));
	if (session == NULL || user == NULL)
	{
		exit(1);
	}
	int clientAddrSize = sizeof(clientaddr);
	session->sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &clientAddrSize);
	InetNtop(AF_INET, (SOCKADDR*)&clientaddr.sin_addr, clientaddrStr, _countof(clientaddrStr));
	_LOG(dfLOG_LEVEL_DEBUG, L"CONNECT # IP: %s / SessionID: %d", clientaddrStr, allocatingID);
	if (session->sock == INVALID_SOCKET)
	{
		_LOG(dfLOG_LEVEL_ERROR, L"ACCEPT INVALID SOCKET : ERROR CODE = %d", WSAGetLastError());
		exit(1);
	}
	session->recvQueue = new CRingBuffer();
	session->sendQueue = new CRingBuffer();
	session->dwSessionID = allocatingID++;
	user->userID = session->dwSessionID;
	user->pSession = session;
	user->x = 320;
	user->y = 260;
	user->Direction = dfPACKET_MOVE_DIR_LL;
	user->isMove = FALSE;
	user->hp = MAX_HP;
	//�ű� ��������: 1) �ű� ���� �Ҵ�(����)
	CPacket cPacket;
	SetPacket_CreateMyCharacter(&cPacket, user->userID, user->Direction, user->x, user->y, user->hp);
	SendUnicast(session, &cPacket);
	_LOG(dfLOG_LEVEL_DEBUG, L"Create Character # SessionID:%d	X:%d	Y:%d", user->userID, user->x, user->y);
	//�ű� ��������: 2) �ٸ� ĳ���͵� ����
	USER* curUser;
	for (std::map<int, USER*>::iterator iter = userMap.begin(); iter != userMap.end(); iter++)
	{
		cPacket.Clear();
		curUser = iter->second;
		SetPacket_CreateOtherCharacter(&cPacket, curUser->userID, curUser->Direction, curUser->x, curUser->y, curUser->hp);
		SendUnicast(session, &cPacket);
		//�̹� �̵� ���� ������ ���� �˷�����Ѵ�.
		if (curUser->isMove == TRUE)
		{
			cPacket.Clear();
			SetPacket_SC_MoveStart(&cPacket, curUser->userID, curUser->Direction, curUser->x, curUser->y);
			SendUnicast(session, &cPacket);
			_LOG(dfLOG_LEVEL_DEBUG, L"# (To NewUser)PACKET_MOVESTART # SessionID:%d / Direction:%d / X:%d / Y:%d",
				curUser->userID, curUser->Direction, curUser->x, curUser->y);
		}
	}
	//�ٸ� �����鿡�� : �ű� ������ ĳ���� ����
	cPacket.Clear();
	SetPacket_CreateOtherCharacter(&cPacket, user->userID, user->Direction, user->x, user->y, user->hp);
	SendBroadcast(session, &cPacket);
	//��, ����Ʈ�� �߰�
	InsertUser(user->userID, user);
	InsertSession(session->dwSessionID, session);
}
void RecvProc(SOCKET socket)
{
	Session* session = FindSession(socket);
	if (session == NULL)
		return;
	session->dwLastRecvTime = timeGetTime();
	//�����۷� �ޱ�
	int directEnqueueSize = 0;
	do
	{
		//���� ���� �����ϸ� ���� ����
		if (session->recvQueue->GetFreeSize() == 0)
		{
			_LOG(dfLOG_LEVEL_ERROR, L"RECVQUEUE ENQUEUE FAILED : RecvProc, Session ID: %d", session->dwSessionID);
			Disconnect(session);
			return;
		}
		directEnqueueSize = session->recvQueue->DirectEnqueueSize();
		recvRetVal = recv(session->sock, session->recvQueue->GetRearBufferPtr(), directEnqueueSize, 0);
		if (recvRetVal == SOCKET_ERROR)
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				_LOG(dfLOG_LEVEL_ERROR, L"!!RECVERROR!! RECV-WSAEWOULDBLOCK!");
				return;
			}
			//��Ÿ ���� ���� ����
			else
			{
				_LOG(dfLOG_LEVEL_ERROR, L"!!RECVERROR!! Session : %d, ERRORCODE: %d", session->dwSessionID, WSAGetLastError());
				Disconnect(session);
				return;
			}
		}
		//����ÿ�
		else if (recvRetVal == 0)
		{
			_LOG(dfLOG_LEVEL_DEBUG, L"!!RECV FIN/RST!! Session : %d", session->dwSessionID);
			Disconnect(session);
			return;
		}
		session->recvQueue->MoveRear(recvRetVal);
		_LOG(dfLOG_LEVEL_DEBUG, L"[RECVPROC] SessionID: %d, RecvRet: %d", session->dwSessionID, recvRetVal);
	} while (recvRetVal == directEnqueueSize);
	stPACKET_HEADER recvHeader;
	//char packet_buf[100];
	CPacket cPacket;
	//packet �� ó��
	while (session->recvQueue->GetUseSize() > 0)
	{
		//�� �������� ���� ��Ŷ ó��
		//���
		if (session->recvQueue->Peek((char*)&recvHeader, sizeof(recvHeader)) == -1)
			break;
		//byCode ���� -> ���н� ���� ����
		if (recvHeader.byCode != dfPACKET_CODE)
		{
			_LOG(dfLOG_LEVEL_ERROR, L"!!HEADER BYTECODE ERROR, Session ID: %d", session->dwSessionID);
			Disconnect(session);
			return;
		}
		//bySize ����
		if ((DWORD)session->recvQueue->GetUseSize() < sizeof(recvHeader) + recvHeader.bySize)
		{
			break;
		}
		_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET SIZE:%d, BUFFERSIZE:%d", recvHeader.bySize, session->recvQueue->GetUseSize());
		session->recvQueue->MoveFront(sizeof(recvHeader));
		dequeueRetVal = session->recvQueue->Dequeue(cPacket.GetBufferPtr(), recvHeader.bySize);
		if (dequeueRetVal == -1)
			break;
		cPacket.MoveWritePos(dequeueRetVal);
		switch (recvHeader.byType)
		{
		case dfPACKET_CS_MOVE_START:
			RecvPacketProc_MoveStart(session, &cPacket);
			break;
		case dfPACKET_CS_MOVE_STOP:
			RecvPacketProc_MoveStop(session, &cPacket);
			break;
		case dfPACKET_CS_ATTACK1:
			RecvPacketProc_Attack1(session, &cPacket);
			break;
		case dfPACKET_CS_ATTACK2:
			RecvPacketProc_Attack2(session, &cPacket);
			break;
		case dfPACKET_CS_ATTACK3:
			RecvPacketProc_Attack3(session, &cPacket);
			break;
		}
	}
}
void SendProc(SOCKET socket)
{
	Session* session = FindSession(socket);
	if (IsDisconnected(session))
		return;
	while (session->sendQueue->GetUseSize() > 0)
	{
		sendRetVal = send(session->sock, session->sendQueue->GetFrontBufferPtr(), session->sendQueue->DirectDequeueSize(), 0);
		if (sendRetVal == SOCKET_ERROR)
		{
			if (sendRetVal == WSAEWOULDBLOCK)
			{
				_LOG(dfLOG_LEVEL_ERROR, L"!!SEND - WSAEWOULDBLOCK, ERROR CODE: %d, Session: %d", WSAGetLastError(), session->dwSessionID);
				continue;
			}
			else
			{
				_LOG(dfLOG_LEVEL_ERROR, L"!!SEND SOCKETERROR!, ERROR CODE: %d, Session: %d", WSAGetLastError(), session->dwSessionID);
				Disconnect(session);
				return;
			}
		}
		session->sendQueue->MoveFront(sendRetVal);
		//_LOG(dfLOG_LEVEL_DEBUG, L"[SENDPROC] SessionID: %d, SendRet: %d", session->user.userID, sendRetVal);
	}
}

//������
void MoveUser(USER* user)
{
	//�̵� �������� Ȯ��
	if (user->isMove == TRUE)
	{
		switch (user->Direction)
		{
		case dfPACKET_MOVE_DIR_LL:
			if (user->x <= dfRANGE_MOVE_LEFT)
				return;
			user->x -= MOVE_SPEED_X;
			break;
		case dfPACKET_MOVE_DIR_LU:
			if (user->x <= dfRANGE_MOVE_LEFT)
				return;
			if (user->y <= dfRANGE_MOVE_TOP)
				return;
			user->x -= MOVE_SPEED_X;
			user->y -= MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_UU:
			if (user->y <= dfRANGE_MOVE_TOP)
				return;
			user->y -= MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_RU:
			if (user->x >= dfRANGE_MOVE_RIGHT)
				return;
			if (user->y <= dfRANGE_MOVE_TOP)
				return;
			user->x += MOVE_SPEED_X;
			user->y -= MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_RR:
			if (user->x >= dfRANGE_MOVE_RIGHT)
				return;
			user->x += MOVE_SPEED_X;
			break;
		case dfPACKET_MOVE_DIR_RD:
			if (user->x >= dfRANGE_MOVE_RIGHT)
				return;
			if (user->y >= dfRANGE_MOVE_BOTTOM)
				return;
			user->x += MOVE_SPEED_X;
			user->y += MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_DD:
			if (user->y >= dfRANGE_MOVE_BOTTOM)
				return;
			user->y += MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_LD:
			if (user->x <= dfRANGE_MOVE_LEFT)
				return;
			if (user->y >= dfRANGE_MOVE_BOTTOM)
				return;
			user->x -= MOVE_SPEED_X;
			user->y += MOVE_SPEED_Y;
			break;
		}
		//_LOG(dfLOG_LEVEL_DEBUG, L"#SESSION %d MOVING / Direction: %d, X: %d, Y:%d", user->userID, user->Direction, user->x, user->y);
	}
}
BOOL IsHit(BYTE atkType, BOOL isLeft, WORD attackerX, WORD attackerY, WORD targetX, WORD targetY)
{
	int xRange, yRange;
	int minX, maxX, minY, maxY;
	switch (atkType)
	{
	case dfPACKET_SC_ATTACK1:
		xRange = dfATTACK1_RANGE_X;
		yRange = dfATTACK1_RANGE_Y;
		break;
	case dfPACKET_SC_ATTACK2:
		xRange = dfATTACK2_RANGE_X;
		yRange = dfATTACK2_RANGE_Y;
		break;
	case dfPACKET_SC_ATTACK3:
		xRange = dfATTACK3_RANGE_X;
		yRange = dfATTACK3_RANGE_Y;
		break;
	default:
		return FALSE;
	}
	if (isLeft == TRUE)
	{
		minX = attackerX - xRange;
		maxX = attackerX;
	}
	else
	{
		minX = attackerX;
		maxX = attackerX + xRange;
	}
	minY = attackerY - yRange;
	maxY = attackerY + yRange;

	if (targetX >= minX && targetX <= maxX && targetY >= minY && targetY <= maxY)
	{
		return TRUE;
	}
	else return FALSE;
}
void CollisionProc(BYTE atkType, USER* atkUser)
{
	int damage;
	BOOL isLeft;
	switch (atkType)
	{
	case dfPACKET_SC_ATTACK1:
		damage = dfATTACK1_DAMAGE;
		break;
	case dfPACKET_SC_ATTACK2:
		damage = dfATTACK2_DAMAGE;
		break;
	case dfPACKET_SC_ATTACK3:
		damage = dfATTACK3_DAMAGE;
		break;
	default:
		return;
	}
	//��������
	//������ ���� ��/�� ����
	if (atkUser->Direction == dfPACKET_MOVE_DIR_LL || atkUser->Direction == dfPACKET_MOVE_DIR_LU ||
		atkUser->Direction == dfPACKET_MOVE_DIR_LD)
	{
		isLeft = TRUE;
	}
	else if (atkUser->Direction == dfPACKET_MOVE_DIR_RR || atkUser->Direction == dfPACKET_MOVE_DIR_RU ||
		atkUser->Direction == dfPACKET_MOVE_DIR_RD)
	{
		isLeft = FALSE;
	}
	else
	{
		_LOG(dfLOG_LEVEL_ERROR, L"!!Collision ERROR###### Direction is UU OR DD");
		return;
	}
	USER* curUser;
	//����� , HP ���� ���� ó��
	for (std::map<int, USER*>::iterator iter = userMap.begin(); iter != userMap.end(); iter++)
	{
		curUser = iter->second;
		//�ڱ� �ڽ��� ����
		if (curUser == atkUser)
			continue;
		//�浹�ߴ°�?
		if (IsHit(atkType, isLeft, atkUser->x, atkUser->y,
			curUser->x, curUser->y))
		{
			//HP�� 0���϶�� ����
			if (curUser->hp <= damage)
			{
				_LOG(dfLOG_LEVEL_DEBUG, L"#SESSION ID %d HP <= 0 -> DIE", curUser->userID);
				Disconnect(curUser->pSession);
			}
			//���� �ʾҴٸ� ����� ó��
			else
			{
				curUser->hp -= damage;
			}
			//�����鿡�� �浹 �˸���
			CPacket cPacket;
			SetPacket_SC_DAMAGE(&cPacket, atkUser->userID, curUser->userID, curUser->hp);
			SendBroadcast(NULL, &cPacket);
			//_LOG(dfLOG_LEVEL_DEBUG, L"# DAMAGE # SessionID:%d -> SessionID:%d", atkUser->userID, curUser->userID);
		}
	}
}

void RecvPacketProc_MoveStart(Session* session, CPacket* clpPacket)
{
	USER* user = FindUser(session->dwSessionID);

	WORD prevX = user->x;
	WORD prevY = user->y;
	//isMove TRUE, direction, ��ǥ
	user->isMove = TRUE;
	*clpPacket >> user->Direction >> user->x >> user->y;
	//��ǥ ��ȿ�� üũ
	if (FALSE == IsValidPosition(prevX, prevY, user->x, user->y))
	{
		//SyncPos(clpPacket, user->userID, prevX, prevY);
		//����
		Disconnect(session);
		return;
	}
	//�ٸ� �����鿡�� �˸���
	CPacket cPacket;
	SetPacket_SC_MoveStart(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendBroadcast(session, &cPacket);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_MOVESTART # SessionID:%d / Direction:%d / X:%d / Y:%d", 
		user->userID, user->Direction, user->x, user->y);
}
void RecvPacketProc_MoveStop(Session* session, CPacket* clpPacket)
{
	//���� �˾Ƴ���
	USER* user = FindUser(session->dwSessionID);
	WORD prevX = user->x;
	WORD prevY = user->y;
	//isMove FALSE, direction, ��ǥ
	user->isMove = FALSE;
	*clpPacket >> user->Direction >> user->x >> user->y;
	//��ǥ ��ȿ�� üũ
	if (FALSE == IsValidPosition(prevX, prevY, user->x, user->y))
	{
		Disconnect(session);
		return;
	}
	//�ٸ� �����鿡�� �˸���
	CPacket cPacket;
	SetPacket_SC_MoveStop(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendBroadcast(session, &cPacket);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_MOVESTOP # SessionID:%d / Direction:%d / X:%d / Y:%d", user->userID,
		user->Direction, user->x, user->y);
}
void RecvPacketProc_Attack1(Session* session, CPacket* clpPacket)
{
	//���� �˾Ƴ���
	USER* user = FindUser(session->dwSessionID);
	WORD prevX = user->x;
	WORD prevY = user->y;
	//��ǥ ����
	*clpPacket >> user->Direction >> user->x >> user->y;
	//��ǥ ��ȿ�� üũ
	if (FALSE == IsValidPosition(prevX, prevY, user->x, user->y))
	{
		Disconnect(session);
		return;
	}
	//���� ��� �˸���
	CPacket cPacket;
	SetPacket_SC_ATTACK1(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendBroadcast(session, &cPacket);
	//�浹 ó���ϱ�(�������)
	CollisionProc(dfPACKET_SC_ATTACK1, user);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_ATTACK1 # SessionID:%d / Direction:%d / X:%d / Y:%d", user->userID,
		user->Direction, user->x, user->y);
}
void RecvPacketProc_Attack2(Session* session, CPacket* clpPacket)
{
	//���� �˾Ƴ���
	USER* user = FindUser(session->dwSessionID);
	WORD prevX = user->x;
	WORD prevY = user->y;
	//��ǥ ����
	*clpPacket >> user->Direction >> user->x >> user->y;
	//��ǥ ��ȿ�� üũ
	if (FALSE == IsValidPosition(prevX, prevY, user->x, user->y))
	{
		Disconnect(session);
		return;
	}
	//���� ��� �˸���
	CPacket cPacket;
	SetPacket_SC_ATTACK2(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendBroadcast(session, &cPacket);
	//�浹 ó���ϱ�(�������)
	CollisionProc(dfPACKET_SC_ATTACK2, user);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_ATTACK2 # SessionID:%d / Direction:%d / X:%d / Y:%d", user->userID,
		user->Direction, user->x, user->y);
}
void RecvPacketProc_Attack3(Session* session, CPacket* clpPacket)
{
	//���� �˾Ƴ���
	USER* user = FindUser(session->dwSessionID);
	WORD prevX = user->x;
	WORD prevY = user->y;
	//��ǥ ����
	*clpPacket >> user->Direction >> user->x >> user->y;
	//��ǥ ��ȿ�� üũ
	if (FALSE == IsValidPosition(prevX, prevY, user->x, user->y))
	{
		Disconnect(session);
		return;
	}
	//���� ��� �˸���
	CPacket cPacket;
	SetPacket_SC_ATTACK3(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendBroadcast(session, &cPacket);
	//�浹 ó���ϱ�(�������)
	CollisionProc(dfPACKET_SC_ATTACK3, user);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_ATTACK3 # SessionID:%d / Direction:%d / X:%d / Y:%d", user->userID,
		user->Direction, user->x, user->y);
}