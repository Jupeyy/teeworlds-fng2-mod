/* (c) KeksTW    */
#include "fng2boomsolo.h"
#include "../entities/character.h"
#include "../player.h"
#include "../fng2define.h"
#include <engine/shared/config.h>
#include <string.h>
#include <stdio.h>

CGameControllerFNG2BoomSolo::CGameControllerFNG2BoomSolo(class CGameContext *pGameServer)
: CGameControllerFNG2Solo((class CGameContext*)pGameServer)
{
	m_pGameType = "fng2";	

	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;
	
	g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Ammoregentime = 1500;
}

CGameControllerFNG2BoomSolo::CGameControllerFNG2BoomSolo(class CGameContext *pGameServer, CConfiguration& pConfig)
: CGameControllerFNG2Solo((class CGameContext*)pGameServer, pConfig)
{
	m_pGameType = "fng2";	

	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;
	
	g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Ammoregentime = 1500;
}

void CGameControllerFNG2BoomSolo::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);
}

void CGameControllerFNG2BoomSolo::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_GRENADE, 5);
}
