/* (c) KeksTW    */
#include "fng2solo.h"
#include "../entities/character.h"
#include "../player.h"
#include "../fng2define.h"
#include <engine/shared/config.h>
#include <string.h>
#include <stdio.h>

CGameControllerFNG2Solo::CGameControllerFNG2Solo(class CGameContext *pGameServer)
: CGameControllerFNG2((class CGameContext*)pGameServer)
{
	m_pGameType = "fng2";
	m_GameFlags = 0;
}
