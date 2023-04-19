#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <list>
#include <map>
#include "RingBuffer.h"
#include "CPacket.h"
#include "PacketDefine.h"

#define		SERVERPORT	20000
#define		FPS			25
#define		WIDTH		6400
#define		HEIGHT		6400
#define		BUFSIZE		10000

//-----------------------------------------------------------------
// 30초 이상이 되도록 아무런 메시지 수신도 없는경우 접속 끊음.
//-----------------------------------------------------------------
#define dfNETWORK_PACKET_RECV_TIMEOUT	30000

//화면 이동 영역
#define dfRANGE_MOVE_TOP	0
#define dfRANGE_MOVE_LEFT	0
#define dfRANGE_MOVE_RIGHT	6400
#define dfRANGE_MOVE_BOTTOM	6400

//이동 속도
#define		MOVE_SPEED_X	6
#define		MOVE_SPEED_Y	4

//이동 오류 체크 범위
#define		dfERROR_RANGE	50

#define		MAX_HP		100

//---------------------------------------------------------------
// 공격범위.
//---------------------------------------------------------------
#define dfATTACK1_RANGE_X		80
#define dfATTACK2_RANGE_X		90
#define dfATTACK3_RANGE_X		100
#define dfATTACK1_RANGE_Y		10
#define dfATTACK2_RANGE_Y		10
#define dfATTACK3_RANGE_Y		20

//---------------------------------------------------------------
// 공격 데미지.
//---------------------------------------------------------------
#define dfATTACK1_DAMAGE		1
#define dfATTACK2_DAMAGE		2
#define dfATTACK3_DAMAGE		3

//섹터 범위
#define	dfSECTOR_MAX_X 40
#define	dfSECTOR_MAX_Y 40

int		g_iLogLevel = 1;
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

struct st_SECTOR_POS
{
	int		iX;
	int		iY;
};
struct st_SECTOR_AROUND
{
	int				iCount;
	st_SECTOR_POS	Around[9];
};
struct Session
{
	SOCKET			sock;
	CRingBuffer* recvQueue;
	CRingBuffer* sendQueue;
	DWORD			dwSessionID;//=userID와 같게.
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

	st_SECTOR_POS	curSector;
	st_SECTOR_POS	oldSector;

	char	hp;
};
//세션 관리
std::map<SOCKET, Session*> sessionMap;
std::list<Session*> disconnectSessions;
//캐릭터 관리
std::map<int, USER*> userMap;
//월드맵 캐릭터 섹터
std::list<USER*> g_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
//섹터 칸 크기
const int sectorXSize = WIDTH / dfSECTOR_MAX_X;
const int sectorYSize = HEIGHT / dfSECTOR_MAX_Y;

int allocatingID = 0;
int retVal;

int acceptRetVal;
int recvRetVal;
int	sendRetVal;
int enqueueRetVal;
int dequeueRetVal;

int recvCount = 0;
int sendCount = 0;

//네트워크
void Disconnect(Session* session)
{
	disconnectSessions.push_back(session);
}
Session* FindSession(SOCKET sock)
{
	auto iter = sessionMap.find(sock);
	if (iter == sessionMap.end())
	{
		_LOG(dfLOG_LEVEL_ERROR, L"!!FindSession Failed. Session SOCKET: %d", sock);
		exit(1);
	}
	return iter->second;
}
void InsertSession(SOCKET sessionSock, Session* session)
{
	sessionMap.insert({ sessionSock, session });
}
void DeleteSession(SOCKET sessionSock)
{
	sessionMap.erase(sessionSock);
}
void SendUnicast(Session* session, CPacket* clpPacket);
//void SendBroadcast(Session* exceptSession, CPacket* clpPacket);
void SendPacket_SectorOne(int sectorX, int sectorY, CPacket* clpPacket, Session* pExceptSession);
void SendPacket_Around(Session* pSession, CPacket* clpPacket, bool isSendMe = false);
void NetworkProc(SOCKET listen_sock);
void NetSelectProc(SOCKET* sockets, fd_set* rset, fd_set* wset);
void AcceptProc(SOCKET listen_sock);
void RecvProc(SOCKET socket);
void SendProc(SOCKET socket);

//컨텐츠
USER* FindUser(int userID)
{
	//유저 알아내기
	auto iter = userMap.find(userID);
	if (iter == userMap.end())
	{
		_LOG(dfLOG_LEVEL_ERROR, L"!!FindUser Failed. USERID: %d", userID);
		exit(1);
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
//섹터 처리
void Sector_AddUser(USER* pUser);
void Sector_RemoveUser(USER* pUser);
bool Sector_UpdateUser(USER* pUser);
void GetSectorAround(int sectorX, int sectorY, st_SECTOR_AROUND* pSectorAround);
void GetUpdateSectorAround(USER* pUser, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector);
void UserSectorUpdatePacket(USER* pUser);

//수신 패킷 종류별 처리
void RecvPacketProc_MoveStart(Session* session, CPacket* clpPacket);
void RecvPacketProc_MoveStop(Session* session, CPacket* clpPacket);
void RecvPacketProc_Attack1(Session* session, CPacket* clpPacket);
void RecvPacketProc_Attack2(Session* session, CPacket* clpPacket);
void RecvPacketProc_Attack3(Session* session, CPacket* clpPacket);
void RecvPacketProc_Echo(Session* session, CPacket* clpPacket);
//송신 패킷 구성
void SetPacket_CreateMyCharacter(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y, BYTE hp);
void SetPacket_CreateOtherCharacter(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y, BYTE hp);
void SetPacket_DeleteCharacter(CPacket* clpPacket, DWORD ID);
void SetPacket_SC_MoveStart(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y);
void SetPacket_SC_MoveStop(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y);
void SetPacket_SC_ATTACK1(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y);
void SetPacket_SC_ATTACK2(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y);
void SetPacket_SC_ATTACK3(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y);
void SetPacket_SC_DAMAGE(CPacket* clpPacket, DWORD AttackID, DWORD DamageID, BYTE DamageHP);
void SetPacket_SC_Echo(CPacket* clpPacket, DWORD time);
void SetPacket_SC_Sync(CPacket* clpPacket, DWORD ID, WORD x, WORD y);

int wmain(int argc, WCHAR* argv[])
{
	timeBeginPeriod(1);
	//1초마다 타이머
	DWORD prevTime = timeGetTime();
	//프레임 시간 맞춰주는(20ms) 용도의 변수
	DWORD startTime = timeGetTime();
	DWORD currentTime;
	DWORD timeInterval;

	int FPSCount = 0;
	int networkCount = 0;

	//윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;
	else _LOG(dfLOG_LEVEL_SYSTEM, L"WSAStartup #");
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

	//논블락 소켓으로 만들기
	u_long isOn = 1;
	retVal = ioctlsocket(listen_sock, FIONBIO, &isOn);
	if (retVal == SOCKET_ERROR) return 1;

	//LINGER 옵션으로 closesocket 시 RST 보내게
	struct linger optVal;
	optVal.l_onoff = 1;
	optVal.l_linger = 0;
	setsockopt(listen_sock, SOL_SOCKET, SO_LINGER, (const char*)&optVal, sizeof(optVal));

	//LOOP
	while (1)
	{
		//네트워크 IO
		NetworkProc(listen_sock);
		networkCount++;
		//로직 - 50프레임 맞추기
		currentTime = timeGetTime();
		timeInterval = currentTime - startTime;
		if (timeInterval >= 1000 / FPS)
		{
			startTime = currentTime - (timeInterval - 1000/FPS);
			//로직 (이동)
			for (std::map<int, USER*>::iterator iter = userMap.begin(); iter != userMap.end(); iter++)
			{
				MoveUser(iter->second);
			}
			FPSCount++;
		}
		//1초마다 현재 상태 PRINT, 일정시간 이상 응답없는 클라이언트 끊기
		if (currentTime - prevTime >= 1000)
		{
			prevTime = currentTime;
			//임시
			if (FPSCount < 20)
			{
				exit(1);
			}
			_LOG(dfLOG_LEVEL_SYSTEM, L"[STATUS] FPS: %d / userCount: %d / netIOCount:%d / RTPS: %d / STPS: %d", FPSCount, sessionMap.size(), networkCount, recvCount, sendCount);
			FPSCount = 0;
			networkCount = 0;
			recvCount = 0;
			sendCount = 0;
			Session* curSession;
			//일정시간 이상 응답없는 클라이언트 끊기
			for (std::map<SOCKET, Session*>::iterator iter = sessionMap.begin(); iter != sessionMap.end(); iter++)
			{
				curSession = iter->second;
				if (currentTime - curSession->dwLastRecvTime > dfNETWORK_PACKET_RECV_TIMEOUT)
				{
					Disconnect(curSession);
				}
			}
		}
	}
	//종료
	closesocket(listen_sock);
	WSACleanup();
	return 0;
}

//네트워크
void NetworkProc(SOCKET listen_sock)
{
	Session* pSession;
	SOCKET	socketTable[FD_SETSIZE] = { INVALID_SOCKET, };
	int		socketCount = 0;

	//SELECT MODEL
	fd_set rset, wset;
	FD_ZERO(&rset);
	FD_ZERO(&wset);
	//리슨 소켓 넣기
	FD_SET(listen_sock, &rset);
	socketTable[socketCount] = listen_sock;
	socketCount++;
	//리슨 소켓 및 접속 중인 모든 클라이언트에 대해 SOCKET 체크

	//rset, wset에 소켓들 넣기
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
		//select 최대치 도달 시
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
		//죽은 사용자에게도 보냄
		//SendBroadcast(NULL, &cPacket);
		SendPacket_Around(*iter, &cPacket, true);
		_LOG(dfLOG_LEVEL_DEBUG, L"Disconnect # Session ID: %d", (*iter)->dwSessionID);
		//리스트에서 제거
		USER* user = FindUser((*iter)->dwSessionID);
		//섹터에서 제거
		g_Sector[user->curSector.iY][user->curSector.iX].remove(user);
		//유저 맵에서 제거
		DeleteUser(user->userID);
		free(user);
		//세션 맵에서 제거
		DeleteSession((*iter)->sock);
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
		//accept시에, 0번째는 항상 listen socket
		if (FD_ISSET(sockets[0], pRset))
		{
			AcceptProc(sockets[0]);
		}
		//RECV&SEND
		for (int i = 1; i < FD_SETSIZE; i++)
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
//특정 1명의 클라이언트에게 메시지 보내기
void SendUnicast(Session* session, CPacket* clpPacket)
{
	sendUnicastRet = session->sendQueue->Enqueue(clpPacket->GetBufferPtr(), clpPacket->GetDataSize());
	if (sendUnicastRet == -1)
	{
		_LOG(dfLOG_LEVEL_ERROR, L"SENDQUEUE ENQUEUE FAILED : SendUnicast, Session ID: %d", session->dwSessionID);
		Disconnect(session);
	}
	//안해도 된다(일회용)
	//clpPacket->Clear();
}
//브로드캐스팅 (시스템 메시지 외에 사용x)
//void SendBroadcast(Session* exceptSession, CPacket* clpPacket)
//{
//	Session* pSession;
//	for (auto iter = sessionMap.begin(); iter != sessionMap.end(); iter++)
//	{
//		pSession = iter->second;
//		if (pSession == exceptSession)
//			continue;
//		sendBroadcastRet = pSession->sendQueue->Enqueue(clpPacket->GetBufferPtr(), clpPacket->GetDataSize());
//		if (sendBroadcastRet == -1)
//		{
//			_LOG(dfLOG_LEVEL_ERROR, L"SENDQUEUE ENQUEUE FAILED : SendBroadcast, Session ID: %d, packetSize: %d",
//				pSession->dwSessionID, clpPacket->GetDataSize());
//			Disconnect(pSession);
//		}
//	}
//	//안해도 된다(일회용)
//	//clpPacket->Clear();
//}
//특정 섹터 1개에 있는 클라이언트들에게 메시지 보내기
int sendSectorOneRet;
void SendPacket_SectorOne(int sectorX, int sectorY, CPacket* clpPacket, Session* pExceptSession)
{
	Session* curSession;
	std::list<USER*>* pUserList = &g_Sector[sectorY][sectorX];
	for (std::list<USER*>::iterator iter = pUserList->begin(); iter != pUserList->end(); iter++)
	{
		curSession = (*iter)->pSession;
		if (curSession == pExceptSession)
		{
			continue;
		}
		sendSectorOneRet = curSession->sendQueue->Enqueue(clpPacket->GetBufferPtr(), clpPacket->GetDataSize());
		if (sendSectorOneRet == -1)
		{
			_LOG(dfLOG_LEVEL_ERROR, L"SENDQUEUE ENQUEUE FAILED : Send_SectorOne, Session ID: %d", curSession->dwSessionID);
			Disconnect(curSession);
		}
	}
	//안해도 된다(일회용)
	//clpPacket->Clear();
}
int sendSectorAroundRet;
//클라이언트 기준 주변 섹터에 메시지 보내기 (최대 9개 영역)
void SendPacket_Around(Session* pSession, CPacket* clpPacket, bool isSendMe)
{
	USER* user = FindUser(pSession->dwSessionID);
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(user->curSector.iX, user->curSector.iY, &sectorAround);
	Session* curSession;
	std::list<USER*>* pUserList;
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		pUserList = &g_Sector[sectorAround.Around[i].iY][sectorAround.Around[i].iX];
		for (std::list<USER*>::iterator iter = pUserList->begin(); iter != pUserList->end(); iter++)
		{
			curSession = (*iter)->pSession;
			if (isSendMe == false && curSession == pSession)
			{
				continue;
			}
			sendSectorAroundRet = curSession->sendQueue->Enqueue(clpPacket->GetBufferPtr(), clpPacket->GetDataSize());
			if (sendSectorAroundRet == -1)
			{
				_LOG(dfLOG_LEVEL_ERROR, L"SENDQUEUE ENQUEUE FAILED : Send_SectorAround, Session ID: %d", curSession->dwSessionID);
				Disconnect(curSession);
			}
		}
	}
	//안해도 된다(일회용)
	//clpPacket->Clear();
}
void AcceptProc(SOCKET listen_sock)
{
	SOCKADDR_IN clientaddr;
	WCHAR clientaddrStr[20] = { 0 };
	//신규 세션
	Session* session = (Session*)malloc(sizeof(Session));
	//신규 유저
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
	session->dwLastRecvTime = timeGetTime();
	user->userID = session->dwSessionID;
	user->pSession = session;
	user->x = 200 + ((user->userID % 30) * 200);
	user->y = 200 + ((user->userID / 30) * 200) % (HEIGHT - 400);
	user->Direction = dfPACKET_MOVE_DIR_LL;
	user->isMove = FALSE;
	user->hp = MAX_HP;
	user->curSector.iX = user->x / sectorXSize;
	user->curSector.iY = user->y / sectorYSize;
	//관리되는 맵/리스트에 추가
	Sector_AddUser(user);
	InsertUser(user->userID, user);
	InsertSession(session->sock, session);
	//신규 유저에게: 1) 신규 유저 할당(생성)
	CPacket cPacket;
	SetPacket_CreateMyCharacter(&cPacket, user->userID, user->Direction, user->x, user->y, user->hp);
	SendUnicast(session, &cPacket);
	_LOG(dfLOG_LEVEL_DEBUG, L"Create Character # SessionID:%d	X:%d	Y:%d", user->userID, user->x, user->y);
	//신규 유저에게: 2) 다른 캐릭터들 생성 (본인 섹터의 캐릭터만)
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(user->curSector.iX, user->curSector.iY, &sectorAround);
	USER* curUser;
	std::list<USER*>* pSectorUserList;
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		pSectorUserList = &g_Sector[sectorAround.Around[i].iY][sectorAround.Around[i].iX];
		for (std::list<USER*>::iterator iter = pSectorUserList->begin(); iter != pSectorUserList->end(); iter++)
		{
			curUser = *iter;
			//자기 자신은 제외하고 보내야한다.
			if (curUser == user)
				continue;
			SetPacket_CreateOtherCharacter(&cPacket, curUser->userID, curUser->Direction, curUser->x, curUser->y, curUser->hp);
			SendUnicast(session, &cPacket);
			//이미 이동 중인 유저인 경우는 알려줘야한다.
			if (curUser->isMove == TRUE)
			{
				SetPacket_SC_MoveStart(&cPacket, curUser->userID, curUser->Direction, curUser->x, curUser->y);
				SendUnicast(session, &cPacket);
				_LOG(dfLOG_LEVEL_DEBUG, L"# (To NewUser)PACKET_MOVESTART # SessionID:%d / Direction:%d / X:%d / Y:%d",
					curUser->userID, curUser->Direction, curUser->x, curUser->y);
			}
		}
	}
	//다른 유저들에게 : 신규 유저의 캐릭터 생성
	cPacket.Clear();
	SetPacket_CreateOtherCharacter(&cPacket, user->userID, user->Direction, user->x, user->y, user->hp);
	//SendBroadcast(session, &cPacket);
	SendPacket_Around(session, &cPacket, false);
}
void RecvProc(SOCKET socket)
{
	Session* session = FindSession(socket);
	if (session == NULL)
		return;
	session->dwLastRecvTime = timeGetTime();
	//링버퍼로 받기
	int directEnqueueSize = 0;
	do
	{
		//넣을 공간 부족하면 세션 제거
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
			//기타 오류 유저 삭제
			else
			{
				int errorCode = WSAGetLastError();
				if (errorCode == 10054)
				{
					_LOG(dfLOG_LEVEL_DEBUG, L"RECV - Session %d CLOSED itself / ERRORCODE: %d", session->dwSessionID, WSAGetLastError());
				}
				else
				{
					_LOG(dfLOG_LEVEL_ERROR, L"!!RECVERROR!! Session : %d, ERRORCODE: %d", session->dwSessionID, WSAGetLastError());
				}
				Disconnect(session);
				return;
			}
		}
		//종료시에
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
	//packet 당 처리
	while (session->recvQueue->GetUseSize() > 0)
	{
		//다 도착하지 않은 패킷 처리
		//헤더
		if (session->recvQueue->Peek((char*)&recvHeader, sizeof(recvHeader)) == -1)
			break;
		//byCode 검증 -> 실패시 세션 끊기
		if (recvHeader.byCode != dfPACKET_CODE)
		{
			_LOG(dfLOG_LEVEL_ERROR, L"!!HEADER BYTECODE ERROR, Session ID: %d", session->dwSessionID);
			Disconnect(session);
			return;
		}
		//bySize 검증
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
		case dfPACKET_CS_ECHO:
			RecvPacketProc_Echo(session, &cPacket);
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
				int errorCode = WSAGetLastError();
				if (errorCode == 10054)
				{
					_LOG(dfLOG_LEVEL_DEBUG, L"!!SEND SOCKETERROR!, Client is already disconnected > Disconnect");
				}
				else
				{
					_LOG(dfLOG_LEVEL_ERROR, L"!!SEND SOCKETERROR!, ERROR CODE: %d, Session: %d", errorCode, session->dwSessionID);
				}
				Disconnect(session);
				return;
			}
		}
		session->sendQueue->MoveFront(sendRetVal);
		//_LOG(dfLOG_LEVEL_DEBUG, L"[SENDPROC] SessionID: %d, SendRet: %d", session->user.userID, sendRetVal);
	}
}

//컨텐츠
void MoveUser(USER* user)
{
	//이동 가능한지 확인
	if (user->isMove == TRUE)
	{
		switch (user->Direction)
		{
		case dfPACKET_MOVE_DIR_LL:
			if (user->x - MOVE_SPEED_X < dfRANGE_MOVE_LEFT)
				return;
			user->x -= MOVE_SPEED_X;
			break;
		case dfPACKET_MOVE_DIR_LU:
			if (user->x - MOVE_SPEED_X < dfRANGE_MOVE_LEFT)
				return;
			if (user->y - MOVE_SPEED_Y < dfRANGE_MOVE_TOP)
				return;
			user->x -= MOVE_SPEED_X;
			user->y -= MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_UU:
			if (user->y - MOVE_SPEED_Y < dfRANGE_MOVE_TOP)
				return;
			user->y -= MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_RU:
			if (user->x + MOVE_SPEED_X > dfRANGE_MOVE_RIGHT)
				return;
			if (user->y - MOVE_SPEED_Y < dfRANGE_MOVE_TOP)
				return;
			user->x += MOVE_SPEED_X;
			user->y -= MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_RR:
			if (user->x + MOVE_SPEED_X > dfRANGE_MOVE_RIGHT)
				return;
			user->x += MOVE_SPEED_X;
			break;
		case dfPACKET_MOVE_DIR_RD:
			if (user->x + MOVE_SPEED_X > dfRANGE_MOVE_RIGHT)
				return;
			if (user->y + MOVE_SPEED_Y > dfRANGE_MOVE_BOTTOM)
				return;
			user->x += MOVE_SPEED_X;
			user->y += MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_DD:
			if (user->y + MOVE_SPEED_Y > dfRANGE_MOVE_BOTTOM)
				return;
			user->y += MOVE_SPEED_Y;
			break;
		case dfPACKET_MOVE_DIR_LD:
			if (user->x - MOVE_SPEED_X < dfRANGE_MOVE_LEFT)
				return;
			if (user->y + MOVE_SPEED_Y > dfRANGE_MOVE_BOTTOM)
				return;
			user->x -= MOVE_SPEED_X;
			user->y += MOVE_SPEED_Y;
			break;
		}
		if (Sector_UpdateUser(user))
		{
			//_LOG(dfLOG_LEVEL_ERROR, L"[UserID: %d] Section (%d, %d) -> Section (%d, %d)",
			//	user->userID, user->oldSector.iX, user->oldSector.iY, user->curSector.iX, user->curSector.iY);
			UserSectorUpdatePacket(user);
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
	//왼쪽인지
	//공격을 위해 좌/우 선택
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
	//대미지 , HP 깎인 유저 처리
	for (std::map<int, USER*>::iterator iter = userMap.begin(); iter != userMap.end(); iter++)
	{
		curUser = iter->second;
		//자기 자신은 제외
		if (curUser == atkUser)
			continue;
		//충돌했는가?
		if (IsHit(atkType, isLeft, atkUser->x, atkUser->y,
			curUser->x, curUser->y))
		{
			//HP가 0이하라면 삭제
			if (curUser->hp <= damage)
			{
				_LOG(dfLOG_LEVEL_DEBUG, L"#SESSION ID %d HP <= 0 -> DIE", curUser->userID);
				Disconnect(curUser->pSession);
			}
			//죽지 않았다면 대미지 처리
			else
			{
				curUser->hp -= damage;
			}
			//유저들에게 충돌 알리기
			CPacket cPacket;
			SetPacket_SC_DAMAGE(&cPacket, atkUser->userID, curUser->userID, curUser->hp);
			//대미지를 받은 캐릭터 입장에서 주위로 전송
			SendPacket_Around(curUser->pSession, &cPacket, true);
			//SendBroadcast(NULL, &cPacket);
			//_LOG(dfLOG_LEVEL_DEBUG, L"# DAMAGE # SessionID:%d -> SessionID:%d", atkUser->userID, curUser->userID);
		}
	}
}

void RecvPacketProc_MoveStart(Session* session, CPacket* clpPacket)
{
	USER* user = FindUser(session->dwSessionID);

	WORD newX;
	WORD newY;
	//isMove TRUE, direction, 좌표
	user->isMove = TRUE;
	*clpPacket >> user->Direction >> newX >> newY;
	//좌표 유효성 체크
	if (abs(user->x - newX) > dfERROR_RANGE || abs(user->y - newY) > dfERROR_RANGE)
	{
		//싱크 맞추기
		SetPacket_SC_Sync(clpPacket, user->userID, user->x, user->y);
		SendPacket_Around(session, clpPacket, true);
		//SendBroadcast(NULL, clpPacket);
		_LOG(dfLOG_LEVEL_ERROR, L"# !!SYNC # SessionID:%d / server (%d,%d) / client (%d,%d)",
			user->userID, user->x, user->y, newX, newY);
	}
	else
	{
		user->x = newX;
		user->y = newY;
	}
	//다른 유저들에게 알리기
	CPacket cPacket;
	SetPacket_SC_MoveStart(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendPacket_Around(session, &cPacket, false);
	//SendBroadcast(session, &cPacket);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_MOVESTART # SessionID:%d / Direction:%d / X:%d / Y:%d",
		user->userID, user->Direction, user->x, user->y);
}
void RecvPacketProc_MoveStop(Session* session, CPacket* clpPacket)
{
	//유저 알아내기
	USER* user = FindUser(session->dwSessionID);
	WORD newX;
	WORD newY;
	//isMove TRUE, direction, 좌표
	user->isMove = FALSE;
	*clpPacket >> user->Direction >> newX >> newY;
	//좌표 유효성 체크
	if (abs(user->x - newX) > dfERROR_RANGE || abs(user->y - newY) > dfERROR_RANGE)
	{
		//싱크 맞추기
		SetPacket_SC_Sync(clpPacket, user->userID, user->x, user->y);
		SendPacket_Around(session, clpPacket, true);
		//SendBroadcast(NULL, clpPacket);
		_LOG(dfLOG_LEVEL_ERROR, L"# !!SYNC # SessionID:%d / server (%d,%d) / client (%d,%d)",
			user->userID, user->x, user->y, newX, newY);
	}
	else
	{
		user->x = newX;
		user->y = newY;
	}
	//다른 유저들에게 알리기
	CPacket cPacket;
	SetPacket_SC_MoveStop(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendPacket_Around(session, &cPacket, false);
	//SendBroadcast(session, &cPacket);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_MOVESTOP # SessionID:%d / Direction:%d / X:%d / Y:%d", user->userID,
		user->Direction, user->x, user->y);
}
void RecvPacketProc_Attack1(Session* session, CPacket* clpPacket)
{
	//유저 알아내기
	USER* user = FindUser(session->dwSessionID);
	WORD newX;
	WORD newY;
	//isMove TRUE, direction, 좌표
	*clpPacket >> user->Direction >> newX >> newY;
	//좌표 유효성 체크
	if (abs(user->x - newX) > dfERROR_RANGE || abs(user->y - newY) > dfERROR_RANGE)
	{
		//싱크 맞추기
		SetPacket_SC_Sync(clpPacket, user->userID, user->x, user->y);
		SendPacket_Around(session, clpPacket, true);
		//SendBroadcast(NULL, clpPacket);
		_LOG(dfLOG_LEVEL_ERROR, L"# !!SYNC # SessionID:%d / server (%d,%d) / client (%d,%d)",
			user->userID, user->x, user->y, newX, newY);
	}
	else
	{
		user->x = newX;
		user->y = newY;
	}
	//공격 사실 알리기
	CPacket cPacket;
	SetPacket_SC_ATTACK1(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendPacket_Around(session, &cPacket, false);
	//SendBroadcast(session, &cPacket);
	//충돌 처리하기(통신포함)
	CollisionProc(dfPACKET_SC_ATTACK1, user);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_ATTACK1 # SessionID:%d / Direction:%d / X:%d / Y:%d", user->userID,
		user->Direction, user->x, user->y);
}
void RecvPacketProc_Attack2(Session* session, CPacket* clpPacket)
{
	//유저 알아내기
	USER* user = FindUser(session->dwSessionID);
	WORD newX;
	WORD newY;
	//isMove TRUE, direction, 좌표
	*clpPacket >> user->Direction >> newX >> newY;
	//좌표 유효성 체크
	if (abs(user->x - newX) > dfERROR_RANGE || abs(user->y - newY) > dfERROR_RANGE)
	{
		//싱크 맞추기
		SetPacket_SC_Sync(clpPacket, user->userID, user->x, user->y);
		SendPacket_Around(session, clpPacket, true);
		//SendBroadcast(NULL, clpPacket);
		_LOG(dfLOG_LEVEL_ERROR, L"# !!SYNC # SessionID:%d / server (%d,%d) / client (%d,%d)",
			user->userID, user->x, user->y, newX, newY);
	}
	else
	{
		user->x = newX;
		user->y = newY;
	}
	//공격 사실 알리기
	CPacket cPacket;
	SetPacket_SC_ATTACK2(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendPacket_Around(session, &cPacket, false);
	//SendBroadcast(session, &cPacket);
	//충돌 처리하기(통신포함)
	CollisionProc(dfPACKET_SC_ATTACK2, user);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_ATTACK2 # SessionID:%d / Direction:%d / X:%d / Y:%d", user->userID,
		user->Direction, user->x, user->y);
}
void RecvPacketProc_Attack3(Session* session, CPacket* clpPacket)
{
	//유저 알아내기
	USER* user = FindUser(session->dwSessionID);
	WORD newX;
	WORD newY;
	//isMove TRUE, direction, 좌표
	*clpPacket >> user->Direction >> newX >> newY;
	//좌표 유효성 체크
	if (abs(user->x - newX) > dfERROR_RANGE || abs(user->y - newY) > dfERROR_RANGE)
	{
		//싱크 맞추기
		SetPacket_SC_Sync(clpPacket, user->userID, user->x, user->y);
		SendPacket_Around(session, clpPacket, true);
		//SendBroadcast(NULL, clpPacket);
		_LOG(dfLOG_LEVEL_ERROR, L"# !!SYNC # SessionID:%d / server (%d,%d) / client (%d,%d)",
			user->userID, user->x, user->y, newX, newY);
	}
	else
	{
		user->x = newX;
		user->y = newY;
	}
	//공격 사실 알리기
	CPacket cPacket;
	SetPacket_SC_ATTACK3(&cPacket, user->userID, user->Direction, user->x, user->y);
	SendPacket_Around(session, &cPacket, false);
	//SendBroadcast(session, &cPacket);
	//충돌 처리하기(통신포함)
	CollisionProc(dfPACKET_SC_ATTACK3, user);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_ATTACK3 # SessionID:%d / Direction:%d / X:%d / Y:%d", user->userID,
		user->Direction, user->x, user->y);
}
void RecvPacketProc_Echo(Session* session, CPacket* clpPacket)
{
	//패킷 수신
	DWORD echoTime;
	*clpPacket >> echoTime;
	//해당 세션의 recvTime 갱신
	session->dwLastRecvTime = timeGetTime();
	//해당 세션에게 에코 돌려주기
	CPacket cPacket;
	SetPacket_SC_Echo(&cPacket, echoTime);
	SendUnicast(session, &cPacket);
	_LOG(dfLOG_LEVEL_DEBUG, L"# PACKET_ECHO # SessionID:%d / time: %d", session->dwSessionID, echoTime);
}

void SetPacket_CreateMyCharacter(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y, BYTE hp)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_CREATE_MY_CHARACTER;
	header.byType = dfPACKET_SC_CREATE_MY_CHARACTER;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y << hp;
}
void SetPacket_CreateOtherCharacter(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y, BYTE hp)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_CREATE_OTHER_CHARACTER;
	header.byType = dfPACKET_SC_CREATE_OTHER_CHARACTER;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y << hp;
}
void SetPacket_DeleteCharacter(CPacket* clpPacket, DWORD ID)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_DELETE_CHARACTER;
	header.byType = dfPACKET_SC_DELETE_CHARACTER;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID;
}
void SetPacket_SC_MoveStart(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_MOVE_START;
	header.byType = dfPACKET_SC_MOVE_START;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}
void SetPacket_SC_MoveStop(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_MOVE_STOP;
	header.byType = dfPACKET_SC_MOVE_STOP;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}
void SetPacket_SC_ATTACK1(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_ATTACK1;
	header.byType = dfPACKET_SC_ATTACK1;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}
void SetPacket_SC_ATTACK2(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_ATTACK2;
	header.byType = dfPACKET_SC_ATTACK2;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}
void SetPacket_SC_ATTACK3(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_ATTACK3;
	header.byType = dfPACKET_SC_ATTACK3;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}
void SetPacket_SC_DAMAGE(CPacket* clpPacket, DWORD AttackID, DWORD DamageID, BYTE DamageHP)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_DAMAGE;
	header.byType = dfPACKET_SC_DAMAGE;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << AttackID << DamageID << DamageHP;
}
void SetPacket_SC_Echo(CPacket* clpPacket, DWORD time)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_ECHO;
	header.byType = dfPACKET_SC_ECHO;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << time;
}
void SetPacket_SC_Sync(CPacket* clpPacket, DWORD ID, WORD x, WORD y)
{
	clpPacket->Clear();
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_SYNC;
	header.byType = dfPACKET_SC_SYNC;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << x << y;
}

//섹터 처리
//캐릭터의 현재 좌표로 섹터위치를 계산하여 해당 섹터에 넣음
void Sector_AddUser(USER* pUser)
{
	g_Sector[pUser->curSector.iY][pUser->curSector.iX].push_back(pUser);
}
//캐릭터의 oldSector에서 삭제
void Sector_RemoveUser(USER* pUser)
{
	g_Sector[pUser->oldSector.iY][pUser->oldSector.iX].remove(pUser);
}
//현재 위치한 섹터에서 삭제한 후, 현재의 좌표로 섹터를 새롭게 계산하여 넣음
bool Sector_UpdateUser(USER* pUser)
{
	int newSectorX = pUser->x / sectorXSize;
	int newSectorY = pUser->y / sectorYSize;
	if (newSectorX == pUser->curSector.iX && newSectorY == pUser->curSector.iY)
	{
		return false;
	}
	if (newSectorX >= dfSECTOR_MAX_X || newSectorY >= dfSECTOR_MAX_Y)
	{
		return false;
	}
	//1. CurSector -> OldSector 저장
	pUser->oldSector = pUser->curSector;
	//2. CurSector 섹터에서 캐릭터 삭제 RemoveUser
	Sector_RemoveUser(pUser);
	//3. 신규 섹터 계산 > CurSector 저장
	pUser->curSector.iX = newSectorX;
	pUser->curSector.iY = newSectorY;
	//4. CurSector 위치에 캐릭터 추가 AddCharacter
	Sector_AddUser(pUser);
	return true;
}
//특정 섹터 좌표 기준 주변 영향권 섹터 얻기
void GetSectorAround(int sectorX, int sectorY, st_SECTOR_AROUND* pSectorAround)
{
	int idx = 0;
	for (int yAdder = -1; yAdder <= 1; yAdder++)
	{
		if (sectorY + yAdder <0 || sectorY + yAdder >= dfSECTOR_MAX_Y)
			continue;
		for (int xAdder = -1; xAdder <= 1; xAdder++)
		{
			if (sectorX + xAdder <0 || sectorX + xAdder >= dfSECTOR_MAX_X)
				continue;
			pSectorAround->Around[idx].iX = sectorX + xAdder;
			pSectorAround->Around[idx].iY = sectorY + yAdder;
			idx++;
		}
	}
	pSectorAround->iCount = idx;
}
//섹터에서 섹터를 이동할 때 섹터 영향권에서 빠진 섹터, 새로 추가된 섹터의 정보를 구하는 함수
void GetUpdateSectorAround(USER* pUser, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector)
{
	st_SECTOR_AROUND oldSectorAround;
	st_SECTOR_AROUND curSectorAround;
	GetSectorAround(pUser->oldSector.iX, pUser->oldSector.iY, &oldSectorAround);
	GetSectorAround(pUser->curSector.iX, pUser->curSector.iY, &curSectorAround);
	pRemoveSector->iCount = 0;
	pAddSector->iCount = 0;
	//이전 섹터 정보 중, 신규 섹터에는 없는 정보를 찾아서 RemoveSector에 넣음.
	for (int i = 0; i < oldSectorAround.iCount; i++)
	{
		bool isFind = false;
		for (int j = 0; j < curSectorAround.iCount; j++)
		{
			if (oldSectorAround.Around[i].iX == curSectorAround.Around[j].iX &&
				oldSectorAround.Around[i].iY == curSectorAround.Around[j].iY)
			{
				isFind = true;
				break;
			}
		}
		if (isFind == false)
		{
			pRemoveSector->Around[pRemoveSector->iCount++] = oldSectorAround.Around[i];
		}
	}
	//현재 섹터 정보 중, 이전 섹터에는 없는 정보를 찾아서 AddSector에 넣음.
	for (int i = 0; i < curSectorAround.iCount; i++)
	{
		bool isFind = false;
		for (int j = 0; j < oldSectorAround.iCount; j++)
		{
			if (curSectorAround.Around[i].iX == oldSectorAround.Around[j].iX &&
				curSectorAround.Around[i].iY == oldSectorAround.Around[j].iY)
			{
				isFind = true;
				break;
			}
		}
		if (isFind == false)
		{
			pAddSector->Around[pAddSector->iCount++] = curSectorAround.Around[i];
		}
	}
}
void UserSectorUpdatePacket(USER* pUser)
{
	st_SECTOR_AROUND removeSector;
	st_SECTOR_AROUND addSector;
	GetUpdateSectorAround(pUser, &removeSector, &addSector);
	std::list<USER*>* pUserList;
	USER* curUser;
	CPacket packet;
	//1. 이전 섹터에서 없어진 부분에 ~ 캐릭터 삭제 메시지
	SetPacket_DeleteCharacter(&packet, pUser->userID);
	for (int i = 0; i < removeSector.iCount; i++)
	{
		SendPacket_SectorOne(removeSector.Around[i].iX, removeSector.Around[i].iY, &packet, NULL);
	}
	//2. 이동하는 캐릭터에게 이전 섹터에서 제외된 섹터의 캐릭터들 삭제 시키는 메시지
	for (int i = 0; i < removeSector.iCount; i++)
	{
		pUserList = &g_Sector[removeSector.Around[i].iY][removeSector.Around[i].iX];
		for (std::list<USER*>::iterator iter = pUserList->begin(); iter != pUserList->end(); iter++)
		{
			SetPacket_DeleteCharacter(&packet, (*iter)->userID);
			SendUnicast(pUser->pSession, &packet);
		}
	}
	//3. 새로 추가된 섹터에 - 캐릭터 생성 메시지 & 이동 메시지
	SetPacket_CreateOtherCharacter(&packet, pUser->userID, pUser->Direction, pUser->x, pUser->y, pUser->hp);
	for (int i = 0; i < addSector.iCount; i++)
	{
		SendPacket_SectorOne(addSector.Around[i].iX, addSector.Around[i].iY, &packet, NULL);
	}
	SetPacket_SC_MoveStart(&packet, pUser->userID, pUser->Direction, pUser->x, pUser->y);
	for (int i = 0; i < addSector.iCount; i++)
	{
		SendPacket_SectorOne(addSector.Around[i].iX, addSector.Around[i].iY, &packet, NULL);
	}

	//4. 이동하는 캐릭터에게 - 새로 진입한 섹터의 캐릭터들 생성 메시지
	for (int i = 0; i < addSector.iCount; i++)
	{
		pUserList = &g_Sector[addSector.Around[i].iY][addSector.Around[i].iX];
		for (std::list<USER*>::iterator iter = pUserList->begin(); iter != pUserList->end(); iter++)
		{
			curUser = *iter;
			//본인은 제외
			if (curUser == pUser)
				continue;
			SetPacket_CreateOtherCharacter(&packet, curUser->userID, curUser->Direction, curUser->x, curUser->y, curUser->hp);
			SendUnicast(pUser->pSession, &packet);
			//해당 캐릭터가 이동 중이라면
			if (curUser->isMove == TRUE)
			{
				SetPacket_SC_MoveStart(&packet, curUser->userID, curUser->Direction, curUser->x, curUser->y);
				SendUnicast(pUser->pSession, &packet);
			}
		}
	}
}
