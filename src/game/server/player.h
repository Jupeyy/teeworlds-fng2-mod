/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

// this include should perhaps be removed
#include "entities/character.h"
#include "gamecontext.h"

#include <string.h>

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

public:
	CPlayer(CGameContext *pGameServer, int ClientID, int Team);
	virtual ~CPlayer();

	void Init(int CID);

	void TryRespawn();
	void Respawn();
	void SetTeam(int Team, bool DoChatMsg=true);
	
	//doesnt kill the character
	void SetTeamSilent(int Team);
	int GetTeam() const { return m_Team; };
	int GetCID() const { return m_ClientID; };

	void Tick();
	void PostTick();
	void Snap(int SnappingClient);

	void OnDirectInput(CNetObj_PlayerInput *NewInput);
	void OnPredictedInput(CNetObj_PlayerInput *NewInput);
	void OnDisconnect(const char *pReason);

	void KillCharacter(int Weapon, bool forced = false);
	CCharacter *GetCharacter();

	virtual void ResetStats();

	//client version
	char m_ClientVersion;
	int m_DDNetVersion;

	int m_UnknownPlayerFlag;
	
	struct sPlayerStats{
		//core stats
		int m_NumJumped;
		float m_NumTilesMoved;
		int m_NumHooks;
		float m_MaxSpeed;
		int m_NumTeeCollisions;
		//other stats
		int m_NumFreezeTicks;
		int m_NumEmotes;		
	
		//score things
		int m_Kills; //kills made by weapon
		int m_GrabsNormal; //kills made by grabbing oponents into spikes - normal spikes
		int m_GrabsTeam; //kills made by grabbing oponents into spikes - team spikes
		int m_GrabsFalse; //kills made by grabbing oponents into spikes - oponents spikes
		int m_GrabsGold; //kills made by grabbing oponents into spikes - gold spikes
		int m_Deaths; //death by spikes -- we don't make a difference of the spike types here
		int m_Hits; //hits by oponents weapon
		int m_Selfkills;
		int m_Teamkills;
		int m_Unfreezes; //number of actual unfreezes of teammates
		int m_UnfreezingHammerHits; //number of hammers to a frozen teammate
		
		int m_Shots; //the shots a player made
	} m_Stats;

	enum eClientVersion {
		CLIENT_VERSION_NORMAL,
		CLIENT_VERSION_DDNET,
	};

	enum {
		VANILLA_CLIENT_MAX_CLIENTS = 16,
		DDNET_CLIENT_MAX_CLIENTS = 64,
	};

	//adds or updates client this clients is snapping from
	bool AddSnappingClient(int RealID, float Distance, char ClientVersion, int& pId);
	//look if a snapped client is a client, this client is snapping from
	bool IsSnappingClient(int RealID, char ClientVersion, int& id);
	int GetRealIDFromSnappingClients(int SnapID);
	void FakeSnap(int PlayerID = (VANILLA_CLIENT_MAX_CLIENTS - 1));

	//the clients this clients is snapping from
	struct {
		float distance;
		int id;
	} m_SnappingClients[DDNET_CLIENT_MAX_CLIENTS];

	//A Player we are whispering to
	struct sWhisperPlayer {
		sWhisperPlayer() : PlayerID(-1) {
			memset(PlayerName, 0, sizeof(PlayerName));
		}
		int PlayerID;
		char PlayerName[16];
	} m_WhisperPlayer;

	//how often did we spam
	int m_ChatSpamCount;
	
	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;

	// states if the client is chatting, accessing a menu etc.
	int m_PlayerFlags;

	// used for snapping to just update latency if the scoreboard is active
	int m_aActLatency[MAX_CLIENTS];

	// used for spectator mode
	int m_SpectatorID;

	bool m_IsReady;
	
	int m_Emotion;
	long long m_EmotionDuration;

	//
	int m_Vote;
	int m_VotePos;
	//
	int m_LastVoteCall;
	int m_LastVoteTry;
	int m_LastChat;
	int m_LastSetTeam;
	int m_LastSetSpectatorMode;
	int m_LastChangeInfo;
	int m_LastEmote;
	int m_LastKill;

	// TODO: clean this up
	struct
	{
		char m_SkinName[64];
		int m_UseCustomColor;
		int m_ColorBody;
		int m_ColorFeet;
	} m_TeeInfos;

	int m_RespawnTick;
	int m_DieTick;
	int m_Score;
	int m_ScoreStartTick;
	bool m_ForceBalanced;
	int m_LastActionTick;
	int m_TeamChangeTick;
	struct
	{
		int m_TargetX;
		int m_TargetY;
	} m_LatestActivity;

	// network latency calculations
	struct
	{
		int m_Accum;
		int m_AccumMin;
		int m_AccumMax;
		int m_Avg;
		int m_Min;
		int m_Max;
	} m_Latency;

private:
	CCharacter *m_pCharacter;
	CGameContext *m_pGameServer;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	//
	bool m_Spawning;
	int m_ClientID;
	int m_Team;
	
	void CalcScore();
};

#endif
