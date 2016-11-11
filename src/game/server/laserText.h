/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LASER_H
#define GAME_SERVER_ENTITIES_LASER_H

#include <game/server/entity.h>

class CLaserChar : public CEntity {
public:
	CLaserChar(CGameWorld *pGameWorld) : CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER) {
	}
public:
	int getID() { return m_ID; }
	vec2 m_Frompos;
};

class CLaserText : public CEntity
{
public:
	CLaserText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int pAliveTicks, char* pText, int pTextLen);
	CLaserText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int pAliveTicks, char* pText, int pTextLen, float pCharPointOffset, float pCharOffsetFactor);
	virtual ~CLaserText(){ 
		delete[] m_Text; 
		for(int i = 0; i < m_CharNum; ++i) {
			delete (CLaserChar*)m_Chars[i];
		}
		delete[] m_Chars;
	}

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

private:
	float m_PosOffsetCharPoints;
	float m_PosOffsetChars;

	void makeLaser(char pChar, int pCharOffset, int& charCount);

	int m_Owner;
	
	int m_AliveTicks;
	int m_CurTicks;
	int m_StartTick;
	
	char* m_Text;
	int m_TextLen;
	
	CLaserChar** m_Chars;
	int m_CharNum;
};

#endif
