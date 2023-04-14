#include "PacketDefine.h"

void SetPacket_CreateMyCharacter(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y, BYTE hp)
{
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_CREATE_MY_CHARACTER;
	header.byType = dfPACKET_SC_CREATE_MY_CHARACTER;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y << hp;
}

void SetPacket_CreateOtherCharacter(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y, BYTE hp)
{
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_CREATE_OTHER_CHARACTER;
	header.byType = dfPACKET_SC_CREATE_OTHER_CHARACTER;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y << hp;
}

void SetPacket_DeleteCharacter(CPacket* clpPacket, DWORD ID)
{
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_DELETE_CHARACTER;
	header.byType = dfPACKET_SC_DELETE_CHARACTER;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID;
}

void SetPacket_SC_MoveStart(CPacket* clpPacket, DWORD ID,BYTE Direction,WORD x,WORD y)
{
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_MOVE_START;
	header.byType = dfPACKET_SC_MOVE_START;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}

void SetPacket_SC_MoveStop(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y)
{
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_MOVE_STOP;
	header.byType = dfPACKET_SC_MOVE_STOP;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}

void SetPacket_SC_ATTACK1(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y)
{
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_ATTACK1;
	header.byType = dfPACKET_SC_ATTACK1;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}
void SetPacket_SC_ATTACK2(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y)
{
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_ATTACK2;
	header.byType = dfPACKET_SC_ATTACK2;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}
void SetPacket_SC_ATTACK3(CPacket* clpPacket, DWORD ID, BYTE Direction, WORD x, WORD y)
{
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_ATTACK3;
	header.byType = dfPACKET_SC_ATTACK3;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << ID << Direction << x << y;
}
void SetPacket_SC_DAMAGE(CPacket* clpPacket, DWORD AttackID, DWORD DamageID, BYTE DamageHP)
{
	stPACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = szPACKET_SC_DAMAGE;
	header.byType = dfPACKET_SC_DAMAGE;
	clpPacket->PutData((char*)&header, sizeof(header));
	*clpPacket << AttackID << DamageID << DamageHP;
}