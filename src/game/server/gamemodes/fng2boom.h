/* (c) KeksTW. */
#ifndef GAME_SERVER_GAMEMODES_FNG2BOOM_H
#define GAME_SERVER_GAMEMODES_FNG2BOOM_H

#include <game/server/gamecontroller.h>
#include <base/vmath.h>

class CGameControllerFNG2Boom : public IGameController
{
public:
	CGameControllerFNG2Boom(class CGameContext* pGameServer);
	CGameControllerFNG2Boom(class CGameContext* pGameServer, CConfiguration& pConfig);
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	
	virtual void DoWincheck();
	
	virtual void PostReset();
protected:
	void EndRound();	
};
#endif
