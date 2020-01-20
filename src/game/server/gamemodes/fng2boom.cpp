/* (c) KeksTW    */
#include "fng2boom.h"
#include "../entities/character.h"
#include "../player.h"
#include "../fng2define.h"
#include <engine/shared/config.h>
#include <string.h>
#include <stdio.h>
#include <generated/server_data.h>

CGameControllerFNG2Boom::CGameControllerFNG2Boom(class CGameContext *pGameServer)
: CGameControllerFNG2((class CGameContext*)pGameServer)
{
	g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Ammoregentime = 1500;
}

void CGameControllerFNG2Boom::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_GRENADE, 5);
}
