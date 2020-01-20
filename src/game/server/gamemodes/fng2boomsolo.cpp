/* (c) KeksTW    */
#include "fng2boomsolo.h"
#include "../entities/character.h"
#include "../player.h"
#include "../fng2define.h"
#include <engine/shared/config.h>
#include <string.h>
#include <stdio.h>
#include <generated/server_data.h>

CGameControllerFNG2BoomSolo::CGameControllerFNG2BoomSolo(class CGameContext *pGameServer)
: CGameControllerFNG2Solo((class CGameContext*)pGameServer)
{
	g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Ammoregentime = 1500;
}

void CGameControllerFNG2BoomSolo::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_GRENADE, 5);
}
