/* (c) KeksTW    */
#include "fng2boom.h"
#include "../entities/character.h"
#include "../player.h"
#include "../fng2define.h"
#include <engine/shared/config.h>
#include <string.h>
#include <stdio.h>

CGameControllerFNG2Boom::CGameControllerFNG2Boom(class CGameContext *pGameServer)
: CGameControllerFNG2((class CGameContext*)pGameServer)
{
	m_pGameType = "fng2";
	m_GameFlags = GAMEFLAG_TEAMS;
	
	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;
	
	g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Ammoregentime = 1500;
}

CGameControllerFNG2Boom::CGameControllerFNG2Boom(class CGameContext *pGameServer, CConfiguration& pConfig)
: CGameControllerFNG2((class CGameContext*)pGameServer, pConfig)
{
	m_pGameType = "fng2";
	m_GameFlags = GAMEFLAG_TEAMS;
	
	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;
	
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
