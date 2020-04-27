/* (c) KeksTW    */
#include "fng2.h"
#include "../entities/character.h"
#include "../player.h"
#include "../fng2define.h"
#include <engine/shared/config.h>
#include <string.h>
#include <stdio.h>
#include "../gamecontext.h"

CGameControllerFNG2::CGameControllerFNG2(class CGameContext *pGameServer)
: IGameController((class CGameContext*)pGameServer)
{
	m_pGameType = "fng2";
	m_GameFlags = GAMEFLAG_TEAMS;
}

void CGameControllerFNG2::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_LASER, -1);
}
	
int CGameControllerFNG2::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	// do scoreing
	if(!pKiller || Weapon == WEAPON_GAME)
		return 0;
	if(pKiller == pVictim->GetPlayer())
		pVictim->GetPlayer()->m_Stats.m_Selfkills++; // suicide
	else
	{
		if (Weapon == WEAPON_LASER || Weapon == WEAPON_GRENADE)
		{
			if(IsTeamplay() && pVictim->GetPlayer()->GetTeam() == pKiller->GetTeam())
				pKiller->m_Stats.m_Teamkills++; // teamkill
			else {
				pKiller->m_Stats.m_Kills++; // normal kill
				pVictim->GetPlayer()->m_Stats.m_Hits++; //hits by oponent
				m_aTeamscore[pKiller->GetTeam()]++; //make this config.?
			}
		}
		else if(Weapon == WEAPON_SPIKE_NORMAL)
		{
			if(pKiller->GetCharacter())
				GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->GetPos(), pKiller->GetCID(), g_Config.m_SvPlayerScoreSpikeNormal);
			pKiller->m_Stats.m_GrabsNormal++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;		
			m_aTeamscore[pKiller->GetTeam()] += g_Config.m_SvTeamScoreSpikeNormal;
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		}
		else if(Weapon == WEAPON_SPIKE_RED)
		{
			// solo or team, would get scores
			if(m_GameFlags != GAMEFLAG_TEAMS || pKiller->GetTeam() == TEAM_RED)
			{
				pKiller->m_Stats.m_GrabsTeam++;
				pVictim->GetPlayer()->m_Stats.m_Deaths++;
				m_aTeamscore[TEAM_RED] += g_Config.m_SvTeamScoreSpikeTeam;
				if(pKiller->GetCharacter())
					GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->GetPos(), pKiller->GetCID(), g_Config.m_SvPlayerScoreSpikeTeam);
			}
			else
			{
				pKiller->m_Stats.m_GrabsFalse++;				
				m_aTeamscore[TEAM_BLUE] += g_Config.m_SvTeamScoreSpikeFalse;
				if(pKiller->GetCharacter())
					GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->GetPos(), pKiller->GetCID(), g_Config.m_SvPlayerScoreSpikeFalse);
			}
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		}
		else if(Weapon == WEAPON_SPIKE_BLUE)
		{
			// solo or team, would get scores
			if(m_GameFlags != GAMEFLAG_TEAMS || pKiller->GetTeam() == TEAM_BLUE)
			{
				pKiller->m_Stats.m_GrabsTeam++;
				pVictim->GetPlayer()->m_Stats.m_Deaths++;
				m_aTeamscore[TEAM_BLUE] += g_Config.m_SvTeamScoreSpikeTeam;
				if(pKiller->GetCharacter())
					GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->GetPos(), pKiller->GetCID(), g_Config.m_SvPlayerScoreSpikeTeam);
			}
			else
			{
				pKiller->m_Stats.m_GrabsFalse++;
				m_aTeamscore[TEAM_RED] += g_Config.m_SvTeamScoreSpikeFalse;
				if(pKiller->GetCharacter())
					GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->GetPos(), pKiller->GetCID(), g_Config.m_SvPlayerScoreSpikeFalse);
			}
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		}
		else if(Weapon == WEAPON_SPIKE_GOLD)
		{
			pKiller->m_Stats.m_GrabsGold++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;
			m_aTeamscore[pKiller->GetTeam()] += g_Config.m_SvTeamScoreSpikeGold;
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
			if(pKiller->GetCharacter())
				GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->GetPos(), pKiller->GetCID(), g_Config.m_SvPlayerScoreSpikeGold);
		}
		else if(Weapon == WEAPON_HAMMER)
		{
			//only called if team mate unfroze you
			pKiller->m_Stats.m_Unfreezes++;
		}
	}
	if(Weapon == WEAPON_SELF)
	{
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.75f;		
	}
	else if (Weapon == WEAPON_WORLD)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.75f;
	return 0;
}

void CGameControllerFNG2::OnResetPlayer(int ClientID){
	GameServer()->m_apPlayers[ClientID]->ResetStats();
}

void CGameControllerFNG2::EndMatch()
{
	IGameController::EndMatch();
	GameServer()->SendRoundStats();
}
