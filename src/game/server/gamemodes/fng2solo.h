/* (c) KeksTW. */
#ifndef GAME_SERVER_GAMEMODES_FNG2_SOLO_H
#define GAME_SERVER_GAMEMODES_FNG2_SOLO_H

#include <game/server/gamecontroller.h>
#include <base/vmath.h>

#include "fng2.h"

class CGameControllerFNG2Solo : public CGameControllerFNG2
{
public:
	CGameControllerFNG2Solo(class CGameContext* pGameServer);
	CGameControllerFNG2Solo(class CGameContext* pGameServer, CConfiguration& pConfig);
	virtual void Snap(int SnappingClient);
};
#endif
