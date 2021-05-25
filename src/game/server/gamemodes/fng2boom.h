/* (c) KeksTW. */
#ifndef GAME_SERVER_GAMEMODES_FNG2BOOM_H
#define GAME_SERVER_GAMEMODES_FNG2BOOM_H

#include <game/server/gamecontroller.h>
#include <base/vmath.h>

#include "fng2.h"

class CGameControllerFNG2Boom : public CGameControllerFNG2
{
public:
	CGameControllerFNG2Boom(class CGameContext* pGameServer);
	CGameControllerFNG2Boom(class CGameContext* pGameServer, CConfiguration& pConfig);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
};
#endif
