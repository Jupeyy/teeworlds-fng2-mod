/* (c) KeksTW. */
#ifndef GAME_SERVER_GAMEMODES_FNG2_H
#define GAME_SERVER_GAMEMODES_FNG2_H

#include <game/server/gamecontroller.h>
#include <base/vmath.h>

class CGameControllerFNG2 : public IGameController
{
public:
	CGameControllerFNG2(class CGameContext* pGameServer);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
		
	virtual void OnResetPlayer(int ClientID);
protected:
	virtual void EndMatch();
};
#endif
