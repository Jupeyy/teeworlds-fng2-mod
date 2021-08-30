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
	
	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;
}

CGameControllerFNG2Solo::CGameControllerFNG2Solo(class CGameContext *pGameServer, CConfiguration& pConfig)
: CGameControllerFNG2((class CGameContext*)pGameServer, pConfig)
{
	m_pGameType = "fng2";
	m_GameFlags = 0;
	
	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;
}

void CGameControllerFNG2Solo::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);
}
