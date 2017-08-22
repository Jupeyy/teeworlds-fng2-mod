/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_EVENTHANDLER_H
#define GAME_SERVER_EVENTHANDLER_H

#include <cstring>

#ifndef QUADRO_MASK
#define QUADRO_MASK
struct QuadroMask {
	long long m_Mask[4];
	QuadroMask(long long mask) {
		memset(m_Mask, mask, sizeof(m_Mask));
	}
	QuadroMask() {}
	QuadroMask(long long Mask, int id) {
		memset(m_Mask, 0, sizeof(m_Mask));
		m_Mask[id] = Mask;
	}

	void operator|=(const QuadroMask& mask) {
		m_Mask[0] |= mask[0];
		m_Mask[1] |= mask[1];
		m_Mask[2] |= mask[2];
		m_Mask[3] |= mask[3];
	}

	long long& operator[](int id) {
		return m_Mask[id];
	}

	long long operator[](int id) const {
		return m_Mask[id];
	}

	QuadroMask operator=(long long mask) {
		memset(m_Mask, mask, sizeof(m_Mask));
		return *this;
	}

	long long operator & (const QuadroMask& mask){
		return (m_Mask[0] & mask[0]) | (m_Mask[1] & mask[1]) | (m_Mask[2] & mask[2]) | (m_Mask[3] & mask[3]);
	}	

	QuadroMask& operator^(long long mask) {
		m_Mask[0] ^= mask;
		m_Mask[1] ^= mask;
		m_Mask[2] ^= mask;
		m_Mask[3] ^= mask;
		return *this;
	}
	
	bool operator==(QuadroMask& q){
		return m_Mask[0] == q.m_Mask[0] && m_Mask[1] == q.m_Mask[1] && m_Mask[2] == q.m_Mask[2] && m_Mask[3] == q.m_Mask[3];
	}
	
	int Count() {
		int Counter = 0;
		for(int i = 0; i < 4; ++i){
			for(int n = 0; n < 64; ++n){
				if((m_Mask[i] & (1ll << n)) != 0) 
					++Counter;
			}		
		}
		
		return Counter;
	}
	
	int PositionOfNonZeroBit(int Offset){
		for(int i = (Offset/64); i < 4; ++i){
			for(int n = (Offset%64); n < 64; ++n){
				if((m_Mask[i] & (1ll << n)) != 0){
					return i * 64 + n;
				}
			}		
		}
		return -1;
	}
	
	void SetBitOfPosition(int Pos){
		m_Mask[Pos / 64] |= 1 << (Pos % 64);
	}
};
#endif
//
class CEventHandler
{
	static const int MAX_EVENTS = 128;
	static const int MAX_DATASIZE = 128*64;

	int m_aTypes[MAX_EVENTS]; // TODO: remove some of these arrays
	int m_aOffsets[MAX_EVENTS];
	int m_aSizes[MAX_EVENTS];
	QuadroMask m_aClientMasks[MAX_EVENTS];
	char m_aData[MAX_DATASIZE];

	class CGameContext *m_pGameServer;

	int m_CurrentOffset;
	int m_NumEvents;
public:
	CGameContext *GameServer() const { return m_pGameServer; }
	void SetGameServer(CGameContext *pGameServer);

	CEventHandler();
	void *Create(int Type, int Size, QuadroMask Mask = QuadroMask(-1ll));
	void Clear();
	void Snap(int SnappingClient);
};

#endif
