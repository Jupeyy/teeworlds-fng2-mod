/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/server.h>
#include <engine/console.h>
#include <engine/shared/memheap.h>

#include <game/layers.h>
#include <game/voting.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "player.h"

#include <string>

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
		for (int i = 0; i < 4; ++i) {
			for (int n = 0; n < 64; ++n) {
				if ((m_Mask[i] & (1ll << n)) != 0)
					++Counter;
			}
		}

		return Counter;
	}

	int PositionOfNonZeroBit(int Offset) {
		for (int i = (Offset / 64); i < 4; ++i) {
			for (int n = (Offset % 64); n < 64; ++n) {
				if ((m_Mask[i] & (1ll << n)) != 0) {
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

//str_comp_nocase_whitespace
//IMPORTANT: the pArgs can not be accessed by null zero termination. they are not splited by 0, but by space... in case a function needs the whole argument at once. use functions above
typedef void (*ServerCommandExecuteFunc)(class CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum);

struct sServerCommand{
	const char* m_Cmd;
	const char* m_Desc;
	const char* m_ArgFormat;
	sServerCommand* m_NextCommand;
	ServerCommandExecuteFunc m_Func;
	
	sServerCommand(const char* pCmd, const char* pDesc, const char* pArgFormat, ServerCommandExecuteFunc pFunc) : m_Cmd(pCmd), m_Desc(pDesc), m_ArgFormat(pArgFormat), m_NextCommand(0), m_Func(pFunc) {}
	
	void ExecuteCommand(class CGameContext* pContext, int pClientID, const char* pArgs){	
		const char* m_Args[128/2];
		int m_ArgCount = 0;
		
		const char* c = pArgs;
		const char* s = pArgs;
		while(c && *c){
			if(is_whitespace(*c)){
				m_Args[m_ArgCount++] = s;
				s = c + 1;
			}
			++c;
		}
		if (s) {
			m_Args[m_ArgCount++] = s;
		}
		
		m_Func(pContext, pClientID, m_Args, m_ArgCount);
	}
};

/*
	Tick
		Game Context (CGameContext::tick)
			Game World (GAMEWORLD::tick)
				Reset world if requested (GAMEWORLD::reset)
				All entities in the world (ENTITY::tick)
				All entities in the world (ENTITY::tick_defered)
				Remove entities marked for deletion (GAMEWORLD::remove_entities)
			Game Controller (GAMECONTROLLER::tick)
			All players (CPlayer::tick)


	Snap
		Game Context (CGameContext::snap)
			Game World (GAMEWORLD::snap)
				All entities in the world (ENTITY::snap)
			Game Controller (GAMECONTROLLER::snap)
			Events handler (EVENT_HANDLER::snap)
			All players (CPlayer::snap)

*/
class CGameContext : public IGameServer
{
	IServer *m_pServer;
	class IConsole *m_pConsole;
	CLayers m_Layers;
	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;

	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDump(IConsole::IResult *pResult, void *pUserData);
	static void ConPause(IConsole::IResult *pResult, void *pUserData);
	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData);
	static void ConSwapTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConShuffleTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConLockTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData);
	static void ConForceVote(IConsole::IResult *pResult, void *pUserData);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	CGameContext(int Resetting);
	CGameContext(int Resetting, CConfiguration* pConfig);
	void Construct(int Resetting);

	bool m_Resetting;
	
	sServerCommand* FindCommand(const char* pCmd);
	void AddServerCommandSorted(sServerCommand* pCmd);
public:
	sServerCommand* m_FirstServerCommand;
	void AddServerCommand(const char* pCmd, const char* pDesc, const char* pArgFormat, ServerCommandExecuteFunc pFunc);
	void ExecuteServerCommand(int pClientID, const char* pLine);
	
	static void CmdStats(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum);
	static void CmdWhisper(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum);
	static void CmdConversation(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum);
	static void CmdHelp(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum);
	static void CmdEmote(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum);
	
	IServer *Server() const { return m_pServer; }
	class IConsole *Console() { return m_pConsole; }
	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }

	CGameContext();
	~CGameContext();

	void Clear();

	CEventHandler m_Events;
	CPlayer *m_apPlayers[MAX_CLIENTS];

	IGameController *m_pController;
	CGameWorld m_World;

	// helper functions
	class CCharacter *GetPlayerChar(int ClientID);

	int m_LockTeams;

	// voting
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason);
	void EndVote();
	void SendVoteSet(int ClientID);
	void SendVoteStatus(int ClientID, int Total, int Yes, int No);
	void AbortVoteKickOnDisconnect(int ClientID);

	int m_VoteCreator;
	int64 m_VoteCloseTime;
	bool m_VoteUpdate;
	int m_VotePos;
	char m_aVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_NumVoteOptions;
	int m_VoteEnforce;
	enum
	{
		VOTE_ENFORCE_UNKNOWN=0,
		VOTE_ENFORCE_NO,
		VOTE_ENFORCE_YES,
	};
	CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;

	// helper functions
	void MakeLaserTextPoints(vec2 pPos, int pOwner, int pPoints);
	
	void CreateDamageInd(vec2 Pos, float AngleMod, int Amount, int Team, int FromPlayerID = -1);
	void CreateSoundTeam(vec2 Pos, int Sound, int TeamID, int FromPlayerID = -1);

	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage);
	void CreateHammerHit(vec2 Pos);
	void CreatePlayerSpawn(vec2 Pos);
	void CreateDeath(vec2 Pos, int Who);
	void CreateSound(vec2 Pos, int Sound, QuadroMask Mask=QuadroMask(-1ll));
	void CreateSoundGlobal(int Sound, int Target=-1);


	enum
	{
		CHAT_ALL=-2,
		CHAT_SPEC=-1,
		CHAT_RED=0,
		CHAT_BLUE=1,
		//for ddnet client only
		CHAT_WHISPER_SEND=2,
		CHAT_WHISPER_RECV=3,
	};

	// network
	void SendChatTarget(int To, const char *pText);
	void SendChat(int ClientID, int Team, const char *pText, int To = -1);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendBroadcast(const char *pText, int ClientID);


	//
	void CheckPureTuning();
	void SendTuningParams(int ClientID);
	void SendFakeTuningParams(int ClientID);

	//
	void SwapTeams();

	// engine events
	virtual void OnInit();
	virtual void OnInit(class IKernel *pKernel, class IMap* pMap, struct CConfiguration* pConfigFile = 0);
	virtual void OnConsoleInit();
	virtual void OnShutdown();

	virtual int PreferedTeamPlayer(int ClientID);

	virtual void OnTick();
	virtual void OnPreSnap();
	virtual void OnSnap(int ClientID);
	virtual void OnPostSnap();

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID);

	virtual void OnClientConnected(int ClientID, int PreferedTeam = -2);
	virtual void OnClientEnter(int ClientID);
	virtual bool OnClientDrop(int ClientID, const char *pReason, bool Force);
	virtual void OnClientDirectInput(int ClientID, void *pInput);
	virtual void OnClientPredictedInput(int ClientID, void *pInput);

	virtual bool IsClientReady(int ClientID);
	virtual bool IsClientPlayer(int ClientID);

	virtual const char *GameType();
	virtual const char *Version();
	virtual const char *NetVersion();

	void SendRoundStats();
	void SendRandomTrivia();

	template<class T>
	int SendPackMsg(T *pMsg, int Flags)
	{
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p) continue;
			Server()->SendPackMsg(pMsg, Flags, i);
		}
		return 0;
	}

	int SendPackMsg(CNetMsg_Sv_KillMsg *pMsg, int Flags);

	int SendPackMsg(CNetMsg_Sv_Emoticon *pMsg, int Flags);

	int SendPackMsg(CNetMsg_Sv_Chat *pMsg, int Flags);

	int SendPackMsg(CNetMsg_Sv_Chat *pMsg, int Flags, int ClientID);
};

inline QuadroMask CmaskAll() { return QuadroMask(-1); }
inline QuadroMask CmaskOne(int ClientID) { return QuadroMask(1ll<<(ClientID%(sizeof(long long)*8)), (ClientID/(sizeof(long long)*8))); }
inline QuadroMask CmaskAllExceptOne(int ClientID) { return CmaskOne(ClientID)^0xffffffffffffffffll; }
inline bool CmaskIsSet(QuadroMask Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }
#endif
