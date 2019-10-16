/* (c) KeksTW    */
#include "fng2solo.h"
#include "../entities/character.h"
#include "../player.h"
#include "../fng2define.h"
#include <engine/shared/config.h>
#include <string.h>
#include <stdio.h>

CGameControllerFNG2Solo::CGameControllerFNG2Solo(class CGameContext *pGameServer)
: IGameController((class CGameContext*)pGameServer)
{
	m_pGameType = "fng2";
	
	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;
}

CGameControllerFNG2Solo::CGameControllerFNG2Solo(class CGameContext *pGameServer, CConfiguration& pConfig)
: IGameController((class CGameContext*)pGameServer, pConfig)
{
	m_pGameType = "fng2";
	
	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;
}

void CGameControllerFNG2Solo::Tick()
{
	// do warmup
	if(!GameServer()->m_World.m_Paused && m_Warmup)
	{
		if(m_Config.m_SvTournamentMode){
		} else {
			m_Warmup--;
			if(!m_Warmup)
				StartRound();
		}
	}

	if(m_GameOverTick != -1)
	{
		// game over.. wait for restart
		if(Server()->Tick() > m_GameOverTick+Server()->TickSpeed()*10)
		{
			if(m_Config.m_SvTournamentMode){
			} else {
				CycleMap();
				StartRound();
				m_RoundCount++;
			}
		}
	}
	else if(GameServer()->m_World.m_Paused && m_UnpauseTimer)
	{
		--m_UnpauseTimer;
		if(!m_UnpauseTimer)
			GameServer()->m_World.m_Paused = false;
	}

	
	// game is Paused
	if(GameServer()->m_World.m_Paused) {		
		if (m_GameOverTick == -1) {
		}
		if(GameServer()->m_World.m_Paused) ++m_RoundStartTick;
	}

	// do team-balancing
	if(IsTeamplay() && m_UnbalancedTick != -1 && Server()->Tick() > m_UnbalancedTick+m_Config.m_SvTeambalanceTime*Server()->TickSpeed()*60 && !m_Config.m_SvTournamentMode)
	{
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "Balancing teams");

		int aT[2] = {0,0};
		float aTScore[2] = {0,0};
		float aPScore[MAX_CLIENTS] = {0.0f};
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			{
				aT[GameServer()->m_apPlayers[i]->GetTeam()]++;
				aPScore[i] = GameServer()->m_apPlayers[i]->m_Score*Server()->TickSpeed()*60.0f/
					(Server()->Tick()-GameServer()->m_apPlayers[i]->m_ScoreStartTick);
				aTScore[GameServer()->m_apPlayers[i]->GetTeam()] += aPScore[i];
			}
		}

		// are teams unbalanced?
		if(absolute(aT[0]-aT[1]) >= 2)
		{
			int M = (aT[0] > aT[1]) ? 0 : 1;
			int NumBalance = absolute(aT[0]-aT[1]) / 2;

			do
			{
				CPlayer *pP = 0;
				float PD = aTScore[M];
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!GameServer()->m_apPlayers[i] || !CanBeMovedOnBalance(i))
						continue;
					// remember the player who would cause lowest score-difference
					if(GameServer()->m_apPlayers[i]->GetTeam() == M && (!pP || absolute((aTScore[M^1]+aPScore[i]) - (aTScore[M]-aPScore[i])) < PD))
					{
						pP = (CPlayer*)GameServer()->m_apPlayers[i];
						PD = absolute((aTScore[M^1]+aPScore[i]) - (aTScore[M]-aPScore[i]));
					}
				}

				// move the player to the other team
				int Temp = pP->m_LastActionTick;
				pP->SetTeam(M^1);
				pP->m_LastActionTick = Temp;

				pP->Respawn();
				pP->m_ForceBalanced = true;
			} while (--NumBalance);

			m_ForceBalanced = true;
		}
		m_UnbalancedTick = -1;
	}

	// check for inactive players
	if(m_Config.m_SvInactiveKickTime > 0 && !m_Config.m_SvTournamentMode)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !Server()->IsAuthed(i))
			{
				if(Server()->Tick() > GameServer()->m_apPlayers[i]->m_LastActionTick+m_Config.m_SvInactiveKickTime*Server()->TickSpeed()*60)
				{
					switch(m_Config.m_SvInactiveKick)
					{
					case 0:
						{
							// move player to spectator
							((CPlayer*)GameServer()->m_apPlayers[i])->SetTeam(TEAM_SPECTATORS);
						}
						break;
					case 1:
						{
							// move player to spectator if the reserved slots aren't filled yet, kick him otherwise
							int Spectators = 0;
							for(int j = 0; j < MAX_CLIENTS; ++j)
								if(GameServer()->m_apPlayers[j] && GameServer()->m_apPlayers[j]->GetTeam() == TEAM_SPECTATORS)
									++Spectators;
							if(Spectators >= m_Config.m_SvSpectatorSlots)
								Server()->Kick(i, "Kicked for inactivity");
							else
								((CPlayer*)GameServer()->m_apPlayers[i])->SetTeam(TEAM_SPECTATORS);
						}
						break;
					case 2:
						{
							// kick the player
							Server()->Kick(i, "Kicked for inactivity");
						}
					}
				}
			}
		}
	}

	DoWincheck();
}

void CGameControllerFNG2Solo::DoWincheck()
{
	if(m_GameOverTick == -1 && !m_Warmup && !GameServer()->m_World.m_ResetRequested)
	{
		if(IsTeamplay())
		{
			// check score win condition
			if((m_Config.m_SvScorelimit > 0 && (m_aTeamscore[TEAM_RED] >= m_Config.m_SvScorelimit || m_aTeamscore[TEAM_BLUE] >= m_Config.m_SvScorelimit)) ||
				(m_Config.m_SvTimelimit > 0 && (Server()->Tick()-m_RoundStartTick) >= m_Config.m_SvTimelimit*Server()->TickSpeed()*60))
			{
				if(m_Config.m_SvTournamentMode){
				} else {
					if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE])
						EndRound();
					else
						m_SuddenDeath = 1;
				}
			}			
		}
		else
		{
			// gather some stats
			int Topscore = 0;
			int TopscoreCount = 0;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i])
				{
					if(GameServer()->m_apPlayers[i]->m_Score > Topscore)
					{
						Topscore = GameServer()->m_apPlayers[i]->m_Score;
						TopscoreCount = 1;
					}
					else if(GameServer()->m_apPlayers[i]->m_Score == Topscore)
						TopscoreCount++;
				}
			}

			// check score win condition
			if((m_Config.m_SvScorelimit > 0 && Topscore >= m_Config.m_SvScorelimit) ||
				(m_Config.m_SvTimelimit > 0 && (Server()->Tick()-m_RoundStartTick) >= m_Config.m_SvTimelimit*Server()->TickSpeed()*60))
			{
				if(TopscoreCount == 1)
					EndRound();
				else
					m_SuddenDeath = 1;
			}
		}
	}
}

void CGameControllerFNG2Solo::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);
}

void CGameControllerFNG2Solo::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_RIFLE, -1);
}
	
int CGameControllerFNG2Solo::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	// do scoreing
	if(!pKiller || Weapon == WEAPON_GAME)
		return 0;
	if(pKiller == pVictim->GetPlayer())
		pVictim->GetPlayer()->m_Stats.m_Selfkills++; // suicide
	else
	{
		if (Weapon == WEAPON_RIFLE || Weapon == WEAPON_GRENADE){
			if(IsTeamplay() && pVictim->GetPlayer()->GetTeam() == pKiller->GetTeam())
				pKiller->m_Stats.m_Teamkills++; // teamkill
			else {
				pKiller->m_Stats.m_Kills++; // normal kill
				pVictim->GetPlayer()->m_Stats.m_Hits++; //hits by oponent
				m_aTeamscore[pKiller->GetTeam()]++; //make this config.?
			}
		} else if(Weapon == WEAPON_SPIKE_NORMAL){
			if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeNormal);
			pKiller->m_Stats.m_GrabsNormal++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		} else if(Weapon == WEAPON_SPIKE_RED || Weapon == WEAPON_SPIKE_BLUE){
			pKiller->m_Stats.m_GrabsTeam++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;
			if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeTeam);
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		} else if(Weapon == WEAPON_SPIKE_GOLD){
			pKiller->m_Stats.m_GrabsGold++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
			if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeGold);
		} else if(Weapon == WEAPON_HAMMER){ //only called if team mate unfroze you
			pKiller->m_Stats.m_Unfreezes++;
		}
	}
	if(Weapon == WEAPON_SELF){
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.75f;		
	} else if (Weapon == WEAPON_WORLD)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.75f;
	return 0;
}

void CGameControllerFNG2Solo::PostReset(){
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->Respawn();
			GameServer()->m_apPlayers[i]->m_Score = 0;
			GameServer()->m_apPlayers[i]->ResetStats();
			GameServer()->m_apPlayers[i]->m_ScoreStartTick = Server()->Tick();
			GameServer()->m_apPlayers[i]->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
		}
	}
}

void CGameControllerFNG2Solo::EndRound()
{
	IGameController::EndRound();
	GameServer()->SendRoundStats();
}
