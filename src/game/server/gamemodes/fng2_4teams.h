/* (c) KeksTW. */
#ifndef GAME_SERVER_GAMEMODES_FNG2_4TEAMS_H
#define GAME_SERVER_GAMEMODES_FNG2_4TEAMS_H

#include <game/server/gamecontroller.h>
#include <base/vmath.h>

class CGameControllerFNG24Teams : public IGameController
{
public:
	CGameControllerFNG24Teams(class CGameContext* pGameServer);
	CGameControllerFNG24Teams(class CGameContext* pGameServer, CConfiguration& pConfig);
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	
	virtual const char *GetTeamName(int Team);
	
	virtual void DoWincheck();
	
	virtual void PostReset();
	
	virtual void StartRound();
	
	virtual void OnPlayerInfoChange(class CPlayer *pP);
	virtual int GetAutoTeam(int NotThisID);
	virtual bool CanJoinTeam(int Team, int NotThisID);
	virtual bool CheckTeamBalance();
	virtual bool CanChangeTeam(CPlayer *pPplayer, int JoinTeam);
	virtual int ClampTeam(int Team);
	virtual void ShuffleTeams();
	virtual bool UseFakeTeams();
	
	virtual bool OnEntity(int Index, vec2 Pos);
	virtual bool CanSpawn(int Team, vec2 *pPos);
	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos);
	void EvaluateSpawnType(CSpawnEval *pEval, int Type);
	
	static void CmdJoinTeam(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum);

	virtual bool IsFalseSpike(int Team, int SpikeFlags);
protected:
	void EndRound();
	
	int m_a4Teamscore[4];
	int m_a4TeamLastScore[4];
	
	vec2 m_aa4TeamSpawnPoints[5][64];
	int m_a4TeamNumSpawnPoints[5];
};
#endif
