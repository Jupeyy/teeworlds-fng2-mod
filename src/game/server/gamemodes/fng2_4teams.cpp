/* (c) KeksTW    */
#include "fng2_4teams.h"
#include "../entities/character.h"
#include "../player.h"
#include "../fng2define.h"
#include <engine/shared/config.h>
#include <string.h>
#include <stdio.h>

#include <time.h>

CGameControllerFNG24Teams::CGameControllerFNG24Teams(class CGameContext *pGameServer)
: IGameController((class CGameContext*)pGameServer)
{
	m_pGameType = "fng2";
	m_GameFlags = GAMEFLAG_TEAMS;
	
	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;	
	
	m_a4Teamscore[TEAM_RED] = 0;
	m_a4Teamscore[TEAM_BLUE] = 0;
	m_a4Teamscore[TEAM_GREEN] = 0;
	m_a4Teamscore[TEAM_PURPLE] = 0;
	
	m_a4TeamLastScore[TEAM_RED] = 0;
	m_a4TeamLastScore[TEAM_BLUE] = 0;
	m_a4TeamLastScore[TEAM_GREEN] = 0;
	m_a4TeamLastScore[TEAM_PURPLE] = 0;
	
	m_a4TeamNumSpawnPoints[0] = 0;
	m_a4TeamNumSpawnPoints[1] = 0;
	m_a4TeamNumSpawnPoints[2] = 0;
	m_a4TeamNumSpawnPoints[3] = 0;
	m_a4TeamNumSpawnPoints[4] = 0;
	
	pGameServer->AddServerCommand("join", "join the blue, red, green or purple team (e.g. /join r (or /join red) to join the red team)", 0, CmdJoinTeam);
}

CGameControllerFNG24Teams::CGameControllerFNG24Teams(class CGameContext *pGameServer, CConfiguration& pConfig)
: IGameController((class CGameContext*)pGameServer, pConfig)
{
	m_pGameType = "fng2";
	m_GameFlags = GAMEFLAG_TEAMS;
	
	if(m_Config.m_SvTournamentMode) m_Warmup = 60*Server()->TickSpeed();
	else m_Warmup = m_Config.m_SvWarmup;
	
	m_a4Teamscore[TEAM_RED] = 0;
	m_a4Teamscore[TEAM_BLUE] = 0;
	m_a4Teamscore[TEAM_GREEN] = 0;
	m_a4Teamscore[TEAM_PURPLE] = 0;
	
	m_a4TeamLastScore[TEAM_RED] = 0;
	m_a4TeamLastScore[TEAM_BLUE] = 0;
	m_a4TeamLastScore[TEAM_GREEN] = 0;
	m_a4TeamLastScore[TEAM_PURPLE] = 0;
	
	m_a4TeamNumSpawnPoints[0] = 0;
	m_a4TeamNumSpawnPoints[1] = 0;
	m_a4TeamNumSpawnPoints[2] = 0;
	m_a4TeamNumSpawnPoints[3] = 0;
	m_a4TeamNumSpawnPoints[4] = 0;
}

float CGameControllerFNG24Teams::EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos)
{
	float Score = 0.0f;
	CCharacter *pC = static_cast<CCharacter *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));
	for(; pC; pC = (CCharacter *)pC->TypeNext())
	{
		// team mates are not as dangerous as enemies
		float Scoremod = 1.0f;
		if(pEval->m_FriendlyTeam != -1 && pC->GetPlayer()->GetTeam() == pEval->m_FriendlyTeam)
			Scoremod = 0.5f;

		float d = distance(Pos, pC->m_Pos);
		Score += Scoremod * (d == 0 ? 1000000000.0f : 1.0f/d);
	}

	return Score;
}

void CGameControllerFNG24Teams::EvaluateSpawnType(CSpawnEval *pEval, int Type)
{
	// get spawn point
	for(int i = 0; i < m_a4TeamNumSpawnPoints[Type]; i++)
	{
		// check if the position is occupado
		CCharacter *aEnts[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(m_aa4TeamSpawnPoints[Type][i], 64, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		vec2 Positions[5] = { vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f) };	// start, left, up, right, down
		int Result = -1;
		for(int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			Result = Index;
			for(int c = 0; c < Num; ++c)
				if(GameServer()->Collision()->CheckPoint(m_aa4TeamSpawnPoints[Type][i]+Positions[Index]) ||
					distance(aEnts[c]->m_Pos, m_aa4TeamSpawnPoints[Type][i]+Positions[Index]) <= aEnts[c]->m_ProximityRadius)
				{
					Result = -1;
					break;
				}
		}
		if(Result == -1)
			continue;	// try next spawn point

		vec2 P = m_aa4TeamSpawnPoints[Type][i]+Positions[Result];
		float S = EvaluateSpawnPos(pEval, P);
		if(!pEval->m_Got || pEval->m_Score > S)
		{
			pEval->m_Got = true;
			pEval->m_Score = S;
			pEval->m_Pos = P;
		}
	}
}

bool CGameControllerFNG24Teams::CanSpawn(int Team, vec2 *pOutPos)
{
	CSpawnEval Eval;

	// spectators can't spawn
	if(Team == TEAM_SPECTATORS)
		return false;

	if(IsTeamplay())
	{
		Eval.m_FriendlyTeam = Team;

		// first try own team spawn, then normal spawn and then enemy
		EvaluateSpawnType(&Eval, 1+(Team&3));
		if(!Eval.m_Got)
		{
			EvaluateSpawnType(&Eval, 0);
			if(!Eval.m_Got)
			{
				for(int i = 0; i < 4; ++i){
					if(i != Team) EvaluateSpawnType(&Eval, 1+((i)&3));
				}
			}
		}
	}
	else
	{
		EvaluateSpawnType(&Eval, 0);
		EvaluateSpawnType(&Eval, 1);
		EvaluateSpawnType(&Eval, 2);
		EvaluateSpawnType(&Eval, 3);
		EvaluateSpawnType(&Eval, 4);
	}

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}


bool CGameControllerFNG24Teams::OnEntity(int Index, vec2 Pos)
{
	if(Index == ENTITY_SPAWN)
		m_aa4TeamSpawnPoints[0][m_a4TeamNumSpawnPoints[0]++] = Pos;
	else if(Index == ENTITY_SPAWN_RED)
		m_aa4TeamSpawnPoints[1][m_a4TeamNumSpawnPoints[1]++] = Pos;
	else if(Index == ENTITY_SPAWN_BLUE)
		m_aa4TeamSpawnPoints[2][m_a4TeamNumSpawnPoints[2]++] = Pos;
	else if(Index == ENTITY_SPAWN_GREEN)
		m_aa4TeamSpawnPoints[3][m_a4TeamNumSpawnPoints[3]++] = Pos;
	else if(Index == ENTITY_SPAWN_PURPLE)
		m_aa4TeamSpawnPoints[4][m_a4TeamNumSpawnPoints[4]++] = Pos;


	return false;
}

void CGameControllerFNG24Teams::Tick()
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
	
	if(mem_comp(m_a4Teamscore, m_a4TeamLastScore, sizeof(int)*4) != 0){		
		char buff[300];
		str_format(buff, sizeof(buff), "Red team: %d, blue team: %d, green team: %d, purple team: %d", m_a4Teamscore[0], m_a4Teamscore[1], m_a4Teamscore[2], m_a4Teamscore[3]);
		GameServer()->SendBroadcast(buff, -1);
		
		mem_copy(m_a4TeamLastScore, m_a4Teamscore, sizeof(int)*4);
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

		int aT[4] = {0,0,0,0};
		float aTScore[4] = {0,0,0,0};
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
		//we will use the team with most players against team with lowest... until it is balanced
		bool tryBalance = true;
		
		while(tryBalance){
			int TeamMax = -1;
			int TeamMin = -1;
			
			int TeamMaxCount = 0;
			int TeamMinCount = MAX_CLIENTS + 1;
			
			for(int i = 0; i < 4; ++i){
				if(TeamMaxCount < aT[i]){
					TeamMax = i;
					TeamMaxCount = aT[i];
				}
				if(TeamMinCount > aT[i]){
					TeamMin = i;
					TeamMinCount = aT[i];
				}
			}
			
			if(absolute(aT[TeamMax]-aT[TeamMin]) >= 2)
			{
				int M = TeamMax;
				int NumBalance = absolute(aT[TeamMax]-aT[TeamMin]) / 2;

				do
				{
					CPlayer *pP = 0;
					float PD = aTScore[M];
					for(int i = 0; i < MAX_CLIENTS; i++)
					{
						if(!GameServer()->m_apPlayers[i] || !CanBeMovedOnBalance(i))
							continue;
						// remember the player who would cause lowest score-difference
						if(GameServer()->m_apPlayers[i]->GetTeam() == M && (!pP || absolute((aTScore[TeamMin]+aPScore[i]) - (aTScore[M]-aPScore[i])) < PD))
						{
							pP = (CPlayer*)GameServer()->m_apPlayers[i];
							PD = absolute((aTScore[TeamMin]+aPScore[i]) - (aTScore[M]-aPScore[i]));
						}
					}

					// move the player to the other team
					int Temp = pP->m_LastActionTick;
					pP->SetTeam(TeamMin);
					pP->m_LastActionTick = Temp;

					pP->Respawn();
					pP->m_ForceBalanced = true;
					
					--aT[TeamMax];
					++aT[TeamMin];
				} while (--NumBalance);

				m_ForceBalanced = true;
			} else tryBalance = false;
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

void CGameControllerFNG24Teams::DoWincheck()
{
	if(m_GameOverTick == -1 && !m_Warmup && !GameServer()->m_World.m_ResetRequested)
	{
		if(IsTeamplay())
		{
			// check score win condition
			if((m_Config.m_SvScorelimit > 0 && (m_a4Teamscore[TEAM_RED] >= m_Config.m_SvScorelimit || m_a4Teamscore[TEAM_BLUE] >= m_Config.m_SvScorelimit || m_a4Teamscore[TEAM_GREEN] >= m_Config.m_SvScorelimit || m_a4Teamscore[TEAM_PURPLE] >= m_Config.m_SvScorelimit)) ||
				(m_Config.m_SvTimelimit > 0 && (Server()->Tick()-m_RoundStartTick) >= m_Config.m_SvTimelimit*Server()->TickSpeed()*60))
			{
				if(m_Config.m_SvTournamentMode){
				} else {
					EndRound();
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

void CGameControllerFNG24Teams::Snap(int SnappingClient)
{
	CNetObj_GameInfo *pGameInfoObj = (CNetObj_GameInfo *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = 0;//fake this for the client
	pGameInfoObj->m_GameStateFlags = 0;
	if(m_GameOverTick != -1)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_GAMEOVER;
	if(m_SuddenDeath)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;
	if(GameServer()->m_World.m_Paused)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_PAUSED;
	pGameInfoObj->m_RoundStartTick = m_RoundStartTick;
	pGameInfoObj->m_WarmupTimer = GameServer()->m_World.m_Paused ? m_UnpauseTimer : m_Warmup;

	pGameInfoObj->m_ScoreLimit = m_Config.m_SvScorelimit;
	pGameInfoObj->m_TimeLimit = m_Config.m_SvTimelimit;

	pGameInfoObj->m_RoundNum = (str_length(m_Config.m_SvMaprotation) && m_Config.m_SvRoundsPerMap) ? m_Config.m_SvRoundsPerMap : 0;
	pGameInfoObj->m_RoundCurrent = m_RoundCount+1;
}

void CGameControllerFNG24Teams::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_RIFLE, -1);
}
	
int CGameControllerFNG24Teams::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
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
				m_a4Teamscore[pKiller->GetTeam()]++; //make this config.?
			}
		} else if(Weapon == WEAPON_SPIKE_NORMAL){
			if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeNormal);
			pKiller->m_Stats.m_GrabsNormal++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;		
			m_a4Teamscore[pKiller->GetTeam()] += m_Config.m_SvTeamScoreSpikeNormal;
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		} else if(Weapon == WEAPON_SPIKE_RED){
			if(pKiller->GetTeam() == TEAM_RED) {
				pKiller->m_Stats.m_GrabsTeam++;
				pVictim->GetPlayer()->m_Stats.m_Deaths++;
				m_a4Teamscore[TEAM_RED] += m_Config.m_SvTeamScoreSpikeTeam;
				if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeTeam);
			} else {
				pKiller->m_Stats.m_GrabsFalse++;				
				m_a4Teamscore[pKiller->GetTeam()] += m_Config.m_SvTeamScoreSpikeFalse;
				if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeFalse);
			}
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		} else if(Weapon == WEAPON_SPIKE_BLUE){
			if(pKiller->GetTeam() == TEAM_BLUE) {
				pKiller->m_Stats.m_GrabsTeam++;
				pVictim->GetPlayer()->m_Stats.m_Deaths++;
				m_a4Teamscore[TEAM_BLUE] += m_Config.m_SvTeamScoreSpikeTeam;
				if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeTeam);
			} else {
				pKiller->m_Stats.m_GrabsFalse++;
				m_a4Teamscore[pKiller->GetTeam()] += m_Config.m_SvTeamScoreSpikeFalse;
				if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeFalse);
			}
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		} else if(Weapon == WEAPON_SPIKE_GREEN){
			if(pKiller->GetTeam() == TEAM_GREEN) {
				pKiller->m_Stats.m_GrabsTeam++;
				pVictim->GetPlayer()->m_Stats.m_Deaths++;
				m_a4Teamscore[TEAM_GREEN] += m_Config.m_SvTeamScoreSpikeTeam;
				if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeTeam);
			} else {
				pKiller->m_Stats.m_GrabsFalse++;
				m_a4Teamscore[pKiller->GetTeam()] += m_Config.m_SvTeamScoreSpikeFalse;
				if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeFalse);
			}
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		} else if(Weapon == WEAPON_SPIKE_PURPLE){
			if(pKiller->GetTeam() == TEAM_PURPLE) {
				pKiller->m_Stats.m_GrabsTeam++;
				pVictim->GetPlayer()->m_Stats.m_Deaths++;
				m_a4Teamscore[TEAM_PURPLE] += m_Config.m_SvTeamScoreSpikeTeam;
				if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeTeam);
			} else {
				pKiller->m_Stats.m_GrabsFalse++;
				m_a4Teamscore[pKiller->GetTeam()] += m_Config.m_SvTeamScoreSpikeFalse;
				if(pKiller->GetCharacter()) GameServer()->MakeLaserTextPoints(pKiller->GetCharacter()->m_Pos, pKiller->GetCID(), m_Config.m_SvPlayerScoreSpikeFalse);
			}
			pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*.5f;
		} else if(Weapon == WEAPON_SPIKE_GOLD){
			pKiller->m_Stats.m_GrabsGold++;
			pVictim->GetPlayer()->m_Stats.m_Deaths++;
			m_a4Teamscore[pKiller->GetTeam()] += m_Config.m_SvTeamScoreSpikeGold;
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

void CGameControllerFNG24Teams::PostReset(){
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

void CGameControllerFNG24Teams::EndRound()
{
	IGameController::EndRound();
	GameServer()->SendRoundStats();
	
	char buff[300];
	int BuffOffset = 0;
	
	int WinnerTeams = 0;
	int HighestScore = -1;
	
	for(int i = 0; i < 4; ++i){
		if(HighestScore < m_a4Teamscore[i]){
			HighestScore = m_a4Teamscore[i];
			WinnerTeams = 1 << i;
		} else if(m_a4Teamscore[i] == HighestScore){
			WinnerTeams |= 1 << i;
		}
	}
	
	int WinnerCount = 0;
	for(int i = 0; i < 4; ++i){
		if((1 << i) & WinnerTeams) {
			if(WinnerCount > 0){
				str_format((buff + BuffOffset), sizeof(buff) - BuffOffset, " and ");
				BuffOffset = str_length(buff);
			}
			str_format((buff + BuffOffset), sizeof(buff) - BuffOffset, GetTeamName(i));
			BuffOffset = str_length(buff);
			++WinnerCount;
		}
	}
	
	str_format((buff + BuffOffset), sizeof(buff) - BuffOffset, " wins");
	GameServer()->SendBroadcast(buff, -1);
	GameServer()->SendChat(-1, CGameContext::CHAT_ALL, buff);
}

const char *CGameControllerFNG24Teams::GetTeamName(int Team)
{
	if(IsTeamplay())
	{
		if(Team == TEAM_RED)
			return "red team";
		else if(Team == TEAM_BLUE)
			return "blue team";
		else if(Team == TEAM_GREEN)
			return "green team";
		else if(Team == TEAM_PURPLE)
			return "purple team";
	}
	else
	{
		if(Team == 0)
			return "game";
	}

	return "spectators";
}

void CGameControllerFNG24Teams::StartRound()
{
	ResetGame();

	m_RoundStartTick = Server()->Tick();
	m_SuddenDeath = 0;
	m_GameOverTick = -1;
	GameServer()->m_World.m_Paused = false;
	m_a4Teamscore[TEAM_RED] = 0;
	m_a4Teamscore[TEAM_BLUE] = 0;
	m_a4Teamscore[TEAM_GREEN] = 0;
	m_a4Teamscore[TEAM_PURPLE] = 0;
	
	m_a4TeamLastScore[TEAM_RED] = 0;
	m_a4TeamLastScore[TEAM_BLUE] = 0;
	m_a4TeamLastScore[TEAM_GREEN] = 0;
	m_a4TeamLastScore[TEAM_PURPLE] = 0;
	m_ForceBalanced = false;
	Server()->DemoRecorder_HandleAutoStart();
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "start round type='%s' teamplay='%d'", m_pGameType, m_GameFlags&GAMEFLAG_TEAMS);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
}

void CGameControllerFNG24Teams::OnPlayerInfoChange(class CPlayer *pP)
{
	const int aTeamColors[4] = {65387, 10223467, 5308160,12648192};
	if(IsTeamplay())
	{
		pP->m_TeeInfos.m_UseCustomColor = 1;
		if(pP->GetTeam() >= TEAM_RED && pP->GetTeam() <= TEAM_PURPLE)
		{
			pP->m_TeeInfos.m_ColorBody = aTeamColors[pP->GetTeam()];
			pP->m_TeeInfos.m_ColorFeet = aTeamColors[pP->GetTeam()];
		}
		else
		{
			pP->m_TeeInfos.m_ColorBody = 12895054;
			pP->m_TeeInfos.m_ColorFeet = 12895054;
		}
	}
}

int CGameControllerFNG24Teams::GetAutoTeam(int NotThisID)
{
	// this will force the auto balancer to work overtime aswell
	if(m_Config.m_DbgStress)
		return 0;

	int aNumplayers[4] = {0,0,0,0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && i != NotThisID)
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() >= TEAM_RED && GameServer()->m_apPlayers[i]->GetTeam() <= TEAM_PURPLE)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}

	int Team = 0;
	if(IsTeamplay()){
		int MinTeam = MAX_CLIENTS + 1;
		for(int i = 0; i < 4; ++i){
			if(MinTeam > aNumplayers[i]){
				MinTeam = aNumplayers[i];
				Team = i;
			}
		}
	}

	if(CanJoinTeam(Team, NotThisID))
		return Team;
	return -1;
}

bool CGameControllerFNG24Teams::CanJoinTeam(int Team, int NotThisID)
{
	if(m_Config.m_SvTournamentMode) return false;
	
	if(Team == TEAM_SPECTATORS || (GameServer()->m_apPlayers[NotThisID] && GameServer()->m_apPlayers[NotThisID]->GetTeam() != TEAM_SPECTATORS))
		return true;

	int aNumplayers[4] = {0,0,0,0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && i != NotThisID)
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() >= TEAM_RED && GameServer()->m_apPlayers[i]->GetTeam() <= TEAM_PURPLE)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}

	return (aNumplayers[0] + aNumplayers[1] + aNumplayers[2] + aNumplayers[3]) < Server()->MaxClients()-m_Config.m_SvSpectatorSlots;
}


bool CGameControllerFNG24Teams::CheckTeamBalance()
{
	if(!IsTeamplay() || !m_Config.m_SvTeambalanceTime)
		return true;

	int aT[4] = {0,0,0,0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pP = GameServer()->m_apPlayers[i];
		if(pP && pP->GetTeam() != TEAM_SPECTATORS)
			aT[pP->GetTeam()]++;
	}
	
	int TeamMax = -1;
	int TeamMin = -1;
	
	int TeamMaxCount = 0;
	int TeamMinCount = MAX_CLIENTS + 1;
	
	for(int i = 0; i < 4; ++i){
		if(TeamMaxCount < aT[i]){
			TeamMax = i;
			TeamMaxCount = aT[i];
		}
		if(TeamMinCount > aT[i]){
			TeamMin = i;
			TeamMinCount = aT[i];
		}
	}
	
	char aBuf[256];
	if(absolute(aT[TeamMax]-aT[TeamMin]) >= 2)
	{
		str_format(aBuf, sizeof(aBuf), "Teams are NOT balanced (red=%d blue=%d green=%d purple=%d)", aT[0], aT[1], aT[2], aT[3]);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
		if(m_UnbalancedTick == -1)
			m_UnbalancedTick = Server()->Tick();
		return false;
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "Teams are balanced (red=%d blue=%d green=%d purple=%d)", aT[0], aT[1], aT[2], aT[3]);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
		m_UnbalancedTick = -1;
		return true;
	}
}

bool CGameControllerFNG24Teams::CanChangeTeam(CPlayer *pPlayer, int JoinTeam)
{
	if (m_Config.m_SvTournamentMode) {
		GameServer()->SendChatTarget(pPlayer->GetCID(), "You can't change Teams in Tournaments.");
		return false;
	}
	
	int aT[4] = {0,0,0,0};

	if (!IsTeamplay() || JoinTeam == TEAM_SPECTATORS || !m_Config.m_SvTeambalanceTime)
		return true;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pP = GameServer()->m_apPlayers[i];
		if(pP && pP->GetTeam() != TEAM_SPECTATORS)
			aT[pP->GetTeam()]++;
	}

	// simulate what would happen if changed team
	aT[JoinTeam]++;
	if (pPlayer->GetTeam() != TEAM_SPECTATORS)
		aT[pPlayer->GetTeam()]--;
	
	int SmallestTeam = 0;
	int SmallestTeamSize = MAX_CLIENTS + 1;
	//get the smallest team here
	for(int i = 0; i < 4; ++i)
	{
		if(aT[i] < SmallestTeamSize)
		{
			SmallestTeamSize = aT[i];
			SmallestTeam = i;
		}
	}

	// there is a player-difference of at least 2
	if(absolute(aT[JoinTeam]-aT[SmallestTeam]) >= 2)
	{
		// player wants to join team with less players
		if (aT[JoinTeam] < aT[SmallestTeam])
			return true;
		else
			return false;
	}
	else
		return true;
}

int CGameControllerFNG24Teams::ClampTeam(int Team)
{
	if(Team < 0)
		return TEAM_SPECTATORS;
	if(IsTeamplay())
		return Team&3;
	return 0;
}

void CGameControllerFNG24Teams::ShuffleTeams()
{
	srand (time(NULL));
	QuadroMask TeamPlayers[4];
	QuadroMask TeamPlayersAfterShuffle[4];
	do{
		int CounterRed = 0;
		int CounterBlue = 0;
		int CounterGreen = 0;
		int CounterPurple = 0;
		int PlayerTeam = 0;
		TeamPlayers[0] = 0;
		TeamPlayers[1] = 0;
		TeamPlayers[2] = 0;
		TeamPlayers[3] = 0;
		TeamPlayersAfterShuffle[0] = 0;
		TeamPlayersAfterShuffle[1] = 0;
		TeamPlayersAfterShuffle[2] = 0;
		TeamPlayersAfterShuffle[3] = 0;
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS){
				++PlayerTeam;
				TeamPlayers[GameServer()->m_apPlayers[i]->GetTeam()].SetBitOfPosition(i);
			}
		PlayerTeam = (PlayerTeam+3)/4;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			{
				bool ExcludeTeam[4] = { false,false,false,false };
				if(CounterRed == PlayerTeam)
					ExcludeTeam[0] = true;
				if(CounterBlue == PlayerTeam)
					ExcludeTeam[1] = true;
				if(CounterGreen == PlayerTeam)
					ExcludeTeam[2] = true;
				if(CounterPurple == PlayerTeam)
					ExcludeTeam[3] = true;
				
				int rnd = (rand() % 4);
				
				bool found = false;
				while(!found){
					if(rnd == 0)
					{
						if(ExcludeTeam[1]) ++rnd;
						else {
							GameServer()->m_apPlayers[i]->SetTeam(TEAM_BLUE, false);
							++CounterBlue;
							found = true;
						}
					}
					if(rnd == 1)
					{
						if(ExcludeTeam[0]) ++rnd;
						else {
							GameServer()->m_apPlayers[i]->SetTeam(TEAM_RED, false);
							++CounterRed;
							found = true;
						}
					}
					if(rnd == 2)
					{
						if(ExcludeTeam[2]) ++rnd;
						else {
							GameServer()->m_apPlayers[i]->SetTeam(TEAM_GREEN, false);
							++CounterGreen;
							found = true;
						}
					}
					if(rnd == 3)
					{
						if(ExcludeTeam[3]) rnd = 0;
						else {
							GameServer()->m_apPlayers[i]->SetTeam(TEAM_PURPLE, false);
							++CounterPurple;
							found = true;
						}
					}
				}
				
				TeamPlayersAfterShuffle[GameServer()->m_apPlayers[i]->GetTeam()].SetBitOfPosition(i);
			}
		}
	} while((TeamPlayers[0].Count() > 1 || TeamPlayers[1].Count() > 1 || TeamPlayers[2].Count() > 1 || TeamPlayers[3].Count() > 1) && (TeamPlayers[0] == TeamPlayersAfterShuffle[0] || TeamPlayers[0] == TeamPlayersAfterShuffle[1] || TeamPlayers[0] == TeamPlayersAfterShuffle[2] || TeamPlayers[0] == TeamPlayersAfterShuffle[3]));
}

bool CGameControllerFNG24Teams::UseFakeTeams(){
	return true;
}


void CGameControllerFNG24Teams::CmdJoinTeam(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum){	
	if(ArgNum >= 1){		
		int Team = -1;
		if(str_comp_nocase(pArgs[0], "blue") == 0 || str_comp_nocase(pArgs[0], "b") == 0){
			Team = TEAM_BLUE;
		} else if(str_comp_nocase(pArgs[0], "red") == 0 || str_comp_nocase(pArgs[0], "r") == 0){
			Team = TEAM_RED;
		} else if(str_comp_nocase(pArgs[0], "green") == 0 || str_comp_nocase(pArgs[0], "g") == 0){
			Team = TEAM_GREEN;
		} else if(str_comp_nocase(pArgs[0], "purple") == 0 || str_comp_nocase(pArgs[0], "p") == 0){
			Team = TEAM_PURPLE;
		} else if(str_comp_nocase(pArgs[0], "spec") == 0 || str_comp_nocase(pArgs[0], "spectator") == 0 || str_comp_nocase(pArgs[0], "s") == 0){
			Team = TEAM_SPECTATORS;
		}
		
		CPlayer* pPlayer = pContext->m_apPlayers[pClientID];
		if ((pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsFrozen()) || (pPlayer->GetTeam() == Team || (pContext->m_pController->GetConfig()->m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + pContext->Server()->TickSpeed() * 3 > pContext->Server()->Tick())))
			return;
		
		if(pPlayer->m_TeamChangeTick > pContext->Server()->Tick())
		{
			pPlayer->m_LastSetTeam = pContext->Server()->Tick();
			int TimeLeft = (pPlayer->m_TeamChangeTick - pContext->Server()->Tick())/pContext->Server()->TickSpeed();
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Time to wait before changing team: %02d:%02d", TimeLeft/60, TimeLeft%60);
			pContext->SendBroadcast(aBuf, pClientID);
			return;
		}

		// Switch team on given client and kill/respawn him
		if(pContext->m_pController->CanJoinTeam(Team, pClientID))
		{
			if(pContext->m_pController->CanChangeTeam(pPlayer, Team))
			{
				pPlayer->m_LastSetTeam = pContext->Server()->Tick();
				if(pPlayer->GetTeam() == TEAM_SPECTATORS || Team == TEAM_SPECTATORS)
					pContext->m_VoteUpdate = true;
				pPlayer->SetTeam(Team);
				pContext->m_pController->CheckTeamBalance();
				pPlayer->m_TeamChangeTick = pContext->Server()->Tick();
			}
			else
				pContext->SendBroadcast("Teams must be balanced, please join other team", pClientID);
		}
		else
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed", pContext->Server()->MaxClients()-pContext->m_pController->GetConfig()->m_SvSpectatorSlots);
			pContext->SendBroadcast(aBuf, pClientID);
		}
	} else {
		pContext->SendChatTarget(pClientID, "Join commands are: r or red, b or blue, g or green, p or purple and s or spectator to join the team");
	}
}
