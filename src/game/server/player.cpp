/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include "player.h"


MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, int Team)
{
	m_pGameServer = pGameServer;
	m_RespawnTick = Server()->Tick();
	m_DieTick = Server()->Tick();
	m_ScoreStartTick = Server()->Tick();
	m_pCharacter = 0;
	m_ClientID = ClientID;
	m_Team = GameServer()->m_pController->ClampTeam(Team);
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();

	ResetStats();

	m_ChatSpamCount = 0;
	
	m_Emotion = EMOTE_NORMAL;
	m_EmotionDuration = 0;

	m_ClientVersion = eClientVersion::CLIENT_VERSION_NORMAL;
	
	m_UnknownPlayerFlag = 0;
	
	memset(m_SnappingClients, -1, sizeof(m_SnappingClients));
	m_SnappingClients[0].id = ClientID;
	m_SnappingClients[0].distance = 0;
	for (int i = 1; i < DDNET_CLIENT_MAX_CLIENTS; ++i) {
		m_SnappingClients[i].distance = INFINITY;
	}
}

CPlayer::~CPlayer()
{
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Tick()
{
#ifdef CONF_DEBUG
	if (!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS - g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	/*for (int i = 0; i < DDNET_CLIENT_MAX_CLIENTS; ++i) {
		if (m_ClientID == m_SnappingClients[i].id) {
			m_SnappingClients[i].distance = 0;
			m_SnappingClients[i].id = m_ClientID;
		}
		else m_SnappingClients[i].distance = INFINITY;
	}*/

	//calculate the current score based on all fng stats
	CalcScore();

	Server()->SetClientScore(m_ClientID, m_Score);

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick()%Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum/Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(!GameServer()->m_World.m_Paused)
	{
		if(!m_pCharacter && m_Team == TEAM_SPECTATORS && m_SpectatorID == SPEC_FREEVIEW)
			m_ViewPos -= vec2(clamp(m_ViewPos.x-m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y-m_LatestActivity.m_TargetY, -400.0f, 400.0f));

		if(!m_pCharacter && m_DieTick+Server()->TickSpeed()*GameServer()->m_Config->m_SvDieRespawnDelay <= Server()->Tick())
			m_Spawning = true;

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
			{
				m_ViewPos = m_pCharacter->m_Pos;
			}
			else
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else if(m_Spawning && m_RespawnTick <= Server()->Tick())
			TryRespawn();
		
		if(m_EmotionDuration != 0) --m_EmotionDuration;
	}
	else
	{
		++m_RespawnTick;
		++m_DieTick;
		++m_ScoreStartTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
		if(m_EmotionDuration != 0) ++m_EmotionDuration;
 	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators
	if(m_Team == TEAM_SPECTATORS && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID])
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->m_ViewPos;
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;
	
	int ClientID = m_ClientID;
	if (SnappingClient > -1 && GameServer()->m_apPlayers[SnappingClient] && !GameServer()->m_apPlayers[SnappingClient]->IsSnappingClient(GetCID(), GameServer()->m_apPlayers[SnappingClient]->m_ClientVersion, ClientID)) return;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, ClientID, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	if(GameServer()->m_pController->UseFakeTeams() && m_pCharacter && m_pCharacter->IsFrozen()){
		StrToInts(&pClientInfo->m_Skin0, 6, "pinky");
		pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
		pClientInfo->m_ColorBody = 0;
		pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;
	} 
	else 
	{
		StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
		pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
		pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
		pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;
	}
	
	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	pPlayerInfo->m_Local = 0;
	pPlayerInfo->m_ClientID = ClientID;
	pPlayerInfo->m_Score = m_Score + ((GameServer()->m_pController->UseFakeTeams() && !GameServer()->m_pController->IsGameOver()) ? (GetTeam() * 10000) : 0);
	pPlayerInfo->m_Team = (!GameServer()->m_pController->UseFakeTeams() || m_Team == TEAM_SPECTATORS) ? m_Team : 0;

	if(m_ClientID == SnappingClient)
		pPlayerInfo->m_Local = 1;

	if(m_ClientID == SnappingClient && m_Team == TEAM_SPECTATORS)
	{
		CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, ClientID, sizeof(CNetObj_SpectatorInfo)));
		if(!pSpectatorInfo)
			return;
		
		pSpectatorInfo->m_SpectatorID = m_SpectatorID;
		if(SnappingClient > -1 && !GameServer()->m_apPlayers[SnappingClient]->IsSnappingClient(m_SpectatorID, GameServer()->m_apPlayers[SnappingClient]->m_ClientVersion, pSpectatorInfo->m_SpectatorID))
			pSpectatorInfo->m_SpectatorID = m_SpectatorID;
		pSpectatorInfo->m_X = m_ViewPos.x;
		pSpectatorInfo->m_Y = m_ViewPos.y;
	}
}

void CPlayer::OnDisconnect(const char *pReason)
{
	KillCharacter(WEAPON_GAME, true);

	if(Server()->ClientIngame(m_ClientID))
	{
		if (!g_Config.m_SvTournamentMode || GameServer()->m_pController->IsGameOver()) {
			char aBuf[512];
			if (pReason && *pReason)
				str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(m_ClientID), pReason);
			else
				str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(m_ClientID));
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

			str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", m_ClientID, Server()->ClientName(m_ClientID));
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
		}
	}
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter)
		m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	//non standard client flag was used
	if (NewInput->m_PlayerFlags & 0xfffffff0) {
		m_UnknownPlayerFlag |= (NewInput->m_PlayerFlags & 0xfffffff0);
		Server()->SetClientUnknownFlags(m_ClientID, m_UnknownPlayerFlag);
	}

	//in tournaments every other flag is disallowed
	if (g_Config.m_SvTournamentMode == 1) {
		if (NewInput->m_PlayerFlags & 0xfffffff0) NewInput->m_PlayerFlags &= 0xf;
	}

	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
		// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
 		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter)
		m_pCharacter->OnDirectInput(NewInput);

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		m_Spawning = true;

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
	}
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::KillCharacter(int Weapon, bool forced)
{
	if(m_pCharacter && (!m_pCharacter->IsFrozen() || forced))
	{
		m_pCharacter->Die(m_ClientID, Weapon);
		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn()
{
	if(m_Team != TEAM_SPECTATORS)
		m_Spawning = true;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	// clamp the team
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	char aBuf[512];
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(m_ClientID), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	KillCharacter(WEAPON_GAME, true);

	m_Team = Team;
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;
	// we got to wait 0.5 secs before respawning
	m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
				GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
		}
	}
}

void CPlayer::SetTeamSilent(int Team)
{
	// clamp the team
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	m_Team = Team;
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;

	GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
				GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
		}
	}
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;

	if(!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos))
		return;

	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos);
}

void CPlayer::CalcScore(){
	if(g_Config.m_SvScoreDisplay == 0){
		m_Score = m_Stats.m_Kills + m_Stats.m_Unfreezes;
		//TODO: make this configurable
		m_Score += (m_Stats.m_GrabsNormal * g_Config.m_SvPlayerScoreSpikeNormal) + (m_Stats.m_GrabsTeam * g_Config.m_SvPlayerScoreSpikeTeam) + (m_Stats.m_GrabsFalse * g_Config.m_SvPlayerScoreSpikeFalse) + (m_Stats.m_GrabsGold * g_Config.m_SvPlayerScoreSpikeGold);
	} else {
		m_Score = m_Stats.m_Kills + m_Stats.m_Unfreezes - m_Stats.m_Hits;
		//TODO: make this configurable
		m_Score += (m_Stats.m_GrabsNormal * g_Config.m_SvPlayerScoreSpikeNormal) + (m_Stats.m_GrabsTeam * g_Config.m_SvPlayerScoreSpikeTeam) + (m_Stats.m_GrabsFalse * g_Config.m_SvPlayerScoreSpikeFalse) + (m_Stats.m_GrabsGold * g_Config.m_SvPlayerScoreSpikeGold) - (m_Stats.m_Deaths * g_Config.m_SvPlayerScoreSpikeNormal);
		m_Score -= m_Stats.m_Teamkills;	
	}
}

void CPlayer::ResetStats(){
	mem_zero(&m_Stats, sizeof(m_Stats));
}

bool CPlayer::AddSnappingClient(int RealID, float Distance, char ClientVersion, int& pId) {
	if (RealID == m_ClientID) {
		if ((ClientVersion == CLIENT_VERSION_NORMAL && MAX_CLIENTS > VANILLA_CLIENT_MAX_CLIENTS) || (MAX_CLIENTS > DDNET_CLIENT_MAX_CLIENTS)) pId = 0;
		return true;
	}
	else if (ClientVersion == CLIENT_VERSION_NORMAL && MAX_CLIENTS > VANILLA_CLIENT_MAX_CLIENTS) {
		int id = -1;
		float highestDistance = 0;
		// VANILLA_CLIENT_MAX_CLIENTS - 1 to allow chatting!!
		for (int i = 1; i < VANILLA_CLIENT_MAX_CLIENTS - 1; ++i) {
			if (m_SnappingClients[i].id == 0xFFFFFFFF || m_SnappingClients[i].id == RealID) {
				m_SnappingClients[i].id = RealID;
				m_SnappingClients[i].distance = Distance;
				pId = i;
				return true;
			}
			if (highestDistance < m_SnappingClients[i].distance || (!GameServer()->m_apPlayers[m_SnappingClients[i].id] || !GameServer()->m_apPlayers[m_SnappingClients[i].id]->GetCharacter())) {
				id = i;
				if (!GameServer()->m_apPlayers[m_SnappingClients[i].id] || !GameServer()->m_apPlayers[m_SnappingClients[i].id]->GetCharacter()) highestDistance = INFINITY;
				else highestDistance = m_SnappingClients[i].distance;
			}
		}
		
		if (id > -1 && highestDistance > Distance) {
			m_SnappingClients[id].id = RealID;
			m_SnappingClients[id].distance = Distance;
			pId = id;
			return false;
		}
	}
	else {
		if (MAX_CLIENTS > DDNET_CLIENT_MAX_CLIENTS) {
			int id = -1;
			float highestDistance = 0;
			for (int i = 1; i < DDNET_CLIENT_MAX_CLIENTS - 1; ++i) {
				if (m_SnappingClients[i].id == 0xFFFFFFFF || m_SnappingClients[i].id == RealID) {
					m_SnappingClients[i].id = RealID;
					m_SnappingClients[i].distance = Distance;
					pId = i;
					return true;
				}
				if (highestDistance < m_SnappingClients[i].distance || (!GameServer()->m_apPlayers[m_SnappingClients[i].id] || !GameServer()->m_apPlayers[m_SnappingClients[i].id]->GetCharacter())) {
					id = i;
					if (!GameServer()->m_apPlayers[m_SnappingClients[i].id] || !GameServer()->m_apPlayers[m_SnappingClients[i].id]->GetCharacter()) highestDistance = INFINITY;
					else highestDistance = m_SnappingClients[i].distance;
				}
			}
			
			if (id > -1 && highestDistance > Distance) {
				m_SnappingClients[id].id = RealID;
				m_SnappingClients[id].distance = Distance;
				pId = id;
				return false;
			}
		}
		else return true;
	}
	return false;
}

bool CPlayer::IsSnappingClient(int RealID, char ClientVersion, int& id) {
	if (RealID == -1) return false;
	else if (RealID == m_ClientID) {
		if ((ClientVersion == CLIENT_VERSION_NORMAL && MAX_CLIENTS > VANILLA_CLIENT_MAX_CLIENTS) || (MAX_CLIENTS > DDNET_CLIENT_MAX_CLIENTS)) id = 0;
		return true;
	}
	else if (ClientVersion == CLIENT_VERSION_NORMAL && MAX_CLIENTS > VANILLA_CLIENT_MAX_CLIENTS) {
		// VANILLA_CLIENT_MAX_CLIENTS - 1 to allow chatting!!
		for (int i = 0; i < VANILLA_CLIENT_MAX_CLIENTS - 1; ++i) {
			if (m_SnappingClients[i].id == 0xFFFFFFFF || m_SnappingClients[i].id == RealID) {
				m_SnappingClients[i].id = RealID;
				id = i;
				return true;
			}
		}
	}
	else {
		if (MAX_CLIENTS > DDNET_CLIENT_MAX_CLIENTS) {
			for (int i = 0; i < DDNET_CLIENT_MAX_CLIENTS - 1; ++i) {
				if (m_SnappingClients[i].id == 0xFFFFFFFF || m_SnappingClients[i].id == RealID) {
					m_SnappingClients[i].id = RealID;
					id = i;
					return true;
				}
			}
		}
		else return true;
	}
	return false;
}

int CPlayer::GetRealIDFromSnappingClients(int SnapID) {
	if(SnapID < 0 || SnapID >= DDNET_CLIENT_MAX_CLIENTS) return -1;
	if (m_ClientVersion == CLIENT_VERSION_NORMAL && MAX_CLIENTS > VANILLA_CLIENT_MAX_CLIENTS) {
		if (m_SnappingClients[SnapID].id != 0xFFFFFFFF) return m_SnappingClients[SnapID].id;
	}
	else {
		if (MAX_CLIENTS > DDNET_CLIENT_MAX_CLIENTS) {			
			if (m_SnappingClients[SnapID].id != 0xFFFFFFFF) return m_SnappingClients[SnapID].id;
		}
		else return SnapID;
	}
	return SnapID;
}

void CPlayer::FakeSnap(int PlayerID) {
	if ((m_ClientVersion == CLIENT_VERSION_NORMAL && MAX_CLIENTS > VANILLA_CLIENT_MAX_CLIENTS) || (MAX_CLIENTS > DDNET_CLIENT_MAX_CLIENTS))
	{
		int FakeID = PlayerID;

		CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, FakeID, sizeof(CNetObj_ClientInfo)));

		if (!pClientInfo)
			return;

		StrToInts(&pClientInfo->m_Name0, 4, " ");
		StrToInts(&pClientInfo->m_Clan0, 3, "");
		StrToInts(&pClientInfo->m_Skin0, 6, "default");
	}
}
