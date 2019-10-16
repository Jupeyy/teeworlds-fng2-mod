/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/map.h>
#include <engine/console.h>
#include "gamecontext.h"
#include <game/version.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include "gamemodes/fng2.h"
#include "gamemodes/fng2solo.h"
#include "gamemodes/fng2boom.h"
#include "gamemodes/fng2boomsolo.h"
#include "gamemodes/fng2_4teams.h"

//other gametypes(for modding without changing original sources)
#include "gamecontext_additional_gametypes_includes.h"

#include "laserText.h"
#include "gameserver_config.h"

#include <vector>
#include <time.h>

enum
{
	RESET,
	NO_RESET
};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = 0;
	m_pServer = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_apPlayers[i] = 0;

	m_pController = 0;
	m_VoteCloseTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LockTeams = 0;
	m_FirstServerCommand = 0;

	if(Resetting==NO_RESET)
		m_pVoteOptionHeap = new CHeap();
}

CGameContext::CGameContext(int Resetting)
{
	m_Config = &g_Config;
	Construct(Resetting);
}

CGameContext::CGameContext(int Resetting, CConfiguration* pConfig)
{
	m_Config = pConfig;
	Construct(Resetting);
}

CGameContext::CGameContext() 
{
	m_Config = &g_Config;
	Construct(NO_RESET);
}

CGameContext::~CGameContext()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		delete m_apPlayers[i];
	if(!m_Resetting)
		delete m_pVoteOptionHeap;
	
	sServerCommand* pCmd = m_FirstServerCommand;
	while(pCmd){
		sServerCommand* pThis = pCmd;
		pCmd = pCmd->m_NextCommand;
		delete pThis;
	}
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;
	
	CConfiguration* pConfig = m_Config;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new (this) CGameContext(RESET, pConfig);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
}


class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return 0;
	return m_apPlayers[ClientID]->GetCharacter();
}

void CGameContext::MakeLaserTextPoints(vec2 pPos, int pOwner, int pPoints){
	char text[10];
	if(pPoints >= 0) str_format(text, 10, "+%d", pPoints);
	else str_format(text, 10, "%d", pPoints);
	pPos.y -= 20.0 * 2.5;
	new CLaserText(&m_World, pPos, pOwner, Server()->TickSpeed() * 3, text, (int)(strlen(text)));
}

void CGameContext::CreateDamageInd(vec2 Pos, float Angle, int Amount, int Team, int FromPlayerID)
{
	float a = 3 * 3.14159f / 2 + Angle;
	float s = a - pi / 3;
	float e = a + pi / 3;

	if(m_pController->IsTeamplay()){
		QuadroMask mask = 0;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (m_apPlayers[i] && m_apPlayers[i]->GetTeam() == Team) mask |= CmaskOne(i);
		}

		for (int i = 0; i < Amount; i++)
		{
			float f = mix(s, e, float(i + 1) / float(Amount + 2));
			CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd), mask);
			if (pEvent)
			{
				pEvent->m_X = (int)Pos.x;
				pEvent->m_Y = (int)Pos.y;
				pEvent->m_Angle = (int)(f*256.0f);
			}
		}
	} else if(FromPlayerID != -1){
		for (int i = 0; i < Amount; i++)
		{
			float f = mix(s, e, float(i + 1) / float(Amount + 2));
			CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd), CmaskOne(FromPlayerID));
			if (pEvent)
			{
				pEvent->m_X = (int)Pos.x;
				pEvent->m_Y = (int)Pos.y;
				pEvent->m_Angle = (int)(f*256.0f);
			}
		}		
	}
}

void CGameContext::CreateSoundTeam(vec2 Pos, int Sound, int TeamID, int FromPlayerID)
{
	if (Sound < 0)
		return;

	//Only when teamplay is activated
	if (m_pController->IsTeamplay()) {
		QuadroMask mask = 0;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (m_apPlayers[i] && m_apPlayers[i]->GetTeam() == TeamID && (FromPlayerID != i)) mask |= CmaskOne(i);
		}

		// create a sound
		CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), mask);
		if (pEvent)
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_SoundID = Sound;
		}
	}
	//the player "causing" this events gets a global sound
	if (FromPlayerID != -1) CreateSoundGlobal(Sound, FromPlayerID);
}

void CGameContext::CreateHammerHit(vec2 Pos)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}


void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	if (!NoDamage)
	{
		// deal damage
		CCharacter *apEnts[MAX_CLIENTS];
		float Radius = 135.0f;
		float InnerRadius = 48.0f;
		int Num = m_World.FindEntities(Pos, Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++)
		{
			vec2 Diff = apEnts[i]->m_Pos - Pos;
			vec2 ForceDir(0,1);
			float l = length(Diff);
			if(l)
				ForceDir = normalize(Diff);
			l = 1-clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);
			float Dmg = 6 * l;
			if((int)Dmg)
				apEnts[i]->TakeDamage(ForceDir*Dmg*2, (int)Dmg, Owner, Weapon);
		}
	}
}

/*
void create_smoke(vec2 Pos)
{
	// create the event
	EV_EXPLOSION *pEvent = (EV_EXPLOSION *)events.create(EVENT_SMOKE, sizeof(EV_EXPLOSION));
	if(pEvent)
	{
		pEvent->x = (int)Pos.x;
		pEvent->y = (int)Pos.y;
	}
}*/

void CGameContext::CreatePlayerSpawn(vec2 Pos)
{
	// create the event
	CNetEvent_Spawn *ev = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn));
	if(ev)
	{
		ev->m_X = (int)Pos.x;
		ev->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientID)
{
	// create the event
	CNetEvent_Death *pEvent = (CNetEvent_Death *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = ClientID;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, QuadroMask Mask)
{
	if (Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGameContext::CreateSoundGlobal(int Sound, int Target)
{
	if (Sound < 0)
		return;

	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundID = Sound;
	int Flag = MSGFLAG_VITAL;
	if(Target != -1)
		Flag |= MSGFLAG_NORECORD;
	Server()->SendPackMsg(&Msg, Flag, Target);	
}


void CGameContext::SendChatTarget(int To, const char *pText)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
}


void CGameContext::SendChat(int ChatterClientID, int Team, const char *pText, int To)
{
	if(Team != CHAT_WHISPER_RECV && Team != CHAT_WHISPER_SEND){
		char aBuf[256];
		if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
			str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Team, Server()->ClientName(ChatterClientID), pText);
		else
			str_format(aBuf, sizeof(aBuf), "*** %s", pText);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, Team!=CHAT_ALL?"teamchat":"chat", aBuf);
	}
	
	if(Team == CHAT_ALL)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;
		SendPackMsg(&Msg, MSGFLAG_VITAL);
	}
	else if((Team == CHAT_WHISPER_RECV || Team == CHAT_WHISPER_SEND) && To != -1){
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = Team;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;
		SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, To);
	}
	else
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 1;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;

		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NOSEND, -1);

		// send to the clients
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() == Team)
				SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
		}
	}
}

void CGameContext::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CGameContext::SendWeaponPickup(int ClientID, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}


void CGameContext::SendBroadcast(const char *pText, int ClientID)
{
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

//
void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->m_Vote = 0;
			m_apPlayers[i]->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq()*25;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(-1);
	m_VoteUpdate = true;
}


void CGameContext::EndVote()
{
	m_VoteCloseTime = 0;
	SendVoteSet(-1);
}

void CGameContext::SendVoteSet(int ClientID)
{
	CNetMsg_Sv_VoteSet Msg;
	if(m_VoteCloseTime)
	{
		Msg.m_Timeout = (m_VoteCloseTime-time_get())/time_freq();
		Msg.m_pDescription = m_aVoteDescription;
		Msg.m_pReason = m_aVoteReason;
	}
	else
	{
		Msg.m_Timeout = 0;
		Msg.m_pDescription = "";
		Msg.m_pReason = "";
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendVoteStatus(int ClientID, int Total, int Yes, int No)
{
	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes+No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

}

void CGameContext::AbortVoteKickOnDisconnect(int ClientID)
{
	if(m_VoteCloseTime && ((!str_comp_num(m_aVoteCommand, "kick ", 5) && str_toint(&m_aVoteCommand[5]) == ClientID) ||
		(!str_comp_num(m_aVoteCommand, "set_team ", 9) && str_toint(&m_aVoteCommand[9]) == ClientID)))
		m_VoteCloseTime = -1;
}


void CGameContext::CheckPureTuning()
{
	// might not be created yet during start up
	if(!m_pController)
		return;

	if(	str_comp(m_pController->m_pGameType, "DM")==0 ||
		str_comp(m_pController->m_pGameType, "TDM")==0 ||
		str_comp(m_pController->m_pGameType, "CTF")==0)
	{
		CTuningParams p;
		if(mem_comp(&p, &m_Tuning, sizeof(p)) != 0)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "resetting tuning due to pure server");
			m_Tuning = p;
		}
	}
}

void CGameContext::SendTuningParams(int ClientID)
{
	CheckPureTuning();

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = (int *)&m_Tuning;
	for(unsigned i = 0; i < sizeof(m_Tuning)/sizeof(int); i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

//tune for frozen tees
void CGameContext::SendFakeTuningParams(int ClientID)
{
	static CTuningParams FakeTuning;
	
	FakeTuning.m_GroundControlSpeed = 0;
	FakeTuning.m_GroundJumpImpulse = 0;
	FakeTuning.m_GroundControlAccel = 0;
	FakeTuning.m_AirControlSpeed = 0;
	FakeTuning.m_AirJumpImpulse = 0;
	FakeTuning.m_AirControlAccel = 0;
	FakeTuning.m_HookDragSpeed = 0;
	FakeTuning.m_HookDragAccel = 0;
	FakeTuning.m_HookFireSpeed = 0;
	
	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = (int *)&FakeTuning;
	for(unsigned i = 0; i < sizeof(FakeTuning)/sizeof(int); i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SwapTeams()
{
	if(!m_pController->IsTeamplay())
		return;
	
	SendChat(-1, CGameContext::CHAT_ALL, "Teams were swapped");

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			m_apPlayers[i]->SetTeam(m_apPlayers[i]->GetTeam()^1, false);
	}

	(void)m_pController->CheckTeamBalance();
}

void CGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();
		}
	}

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteCloseTime == -1)
		{
			SendChat(-1, CGameContext::CHAT_ALL, "Vote aborted");
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
				for(int i = 0; i < MAX_CLIENTS; i++)
					if(m_apPlayers[i])
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
				bool aVoteChecked[MAX_CLIENTS] = {0};
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_apPlayers[i] || m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || aVoteChecked[i])	// don't count in votes by spectators
						continue;

					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i+1; j < MAX_CLIENTS; ++j)
					{
						if(!m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]))
							continue;

						aVoteChecked[j] = true;
						if(m_apPlayers[j]->m_Vote && (!ActVote || ActVotePos > m_apPlayers[j]->m_VotePos))
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}
					}

					Total++;
					if(ActVote > 0)
						Yes++;
					else if(ActVote < 0)
						No++;
				}

				if(Yes >= Total/2+1)
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if(No >= (Total+1)/2)
					m_VoteEnforce = VOTE_ENFORCE_NO;
			}

			if(m_VoteEnforce == VOTE_ENFORCE_YES)
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote();
				SendChat(-1, CGameContext::CHAT_ALL, "Vote passed");

				if(m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || time_get() > m_VoteCloseTime)
			{
				EndVote();
				SendChat(-1, CGameContext::CHAT_ALL, "Vote failed");
			}
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}


#ifdef CONF_DEBUG
	if(m_Config->m_DbgDummies)
	{
		for(int i = 0; i < m_Config->m_DbgDummies ; i++)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i&1)?-1:1;
			m_apPlayers[MAX_CLIENTS-i-1]->OnPredictedInput(&Input);
		}
	}
#endif
}

// Server hooks
void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientEnter(int ClientID)
{
	//world.insert_entity(&players[client_id]);
	m_apPlayers[ClientID]->Respawn();
	if(!m_Config->m_SvTournamentMode || m_pController->IsGameOver()) {
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientID), m_pController->GetTeamName(m_apPlayers[ClientID]->GetTeam()));
		SendChat(-1, CGameContext::CHAT_ALL, aBuf);

		str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), m_apPlayers[ClientID]->GetTeam());
		Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	m_VoteUpdate = true;
}

void CGameContext::OnClientConnected(int ClientID)
{
	// Check which team the player should be on
	const int StartTeam = m_pController->GetAutoTeam(ClientID);

	m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, StartTeam);
	//players[client_id].init(client_id);
	//players[client_id].client_id = client_id;

	(void)m_pController->CheckTeamBalance();

#ifdef CONF_DEBUG
	if(m_Config->m_DbgDummies)
	{
		if (ClientID >= MAX_CLIENTS - m_Config->m_DbgDummies) {
			m_apPlayers[ClientID]->m_Stats.m_Kills = ClientID;
			return;
		}
	}
#endif

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(ClientID);

	// send motd
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = m_Config->m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

bool CGameContext::OnClientDrop(int ClientID, const char *pReason, bool Force)
{
	if (m_apPlayers[ClientID]->GetCharacter() && m_apPlayers[ClientID]->GetCharacter()->IsFrozen() && !m_pController->IsGameOver() && !Force) return false;

	AbortVoteKickOnDisconnect(ClientID);
	m_apPlayers[ClientID]->OnDisconnect(pReason);
	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	(void)m_pController->CheckTeamBalance();
	m_VoteUpdate = true;

	// update spectator modes
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->m_SpectatorID == ClientID)
			m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
	}
	return true;
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	void *pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(!pRawMsg)
	{
		if(m_Config->m_Debug)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgID), MsgID, m_NetObjHandler.FailedMsgOn());
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}
		return;
	}

	if(Server()->ClientIngame(ClientID))
	{
		if(MsgID == NETMSGTYPE_CL_SAY)
		{
			if(m_Config->m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat+Server()->TickSpeed() > Server()->Tick())
				return;

			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;
			int Team = pMsg->m_Team ? pPlayer->GetTeam() : CGameContext::CHAT_ALL;
			
			if(m_Config->m_SvTournamentMode) {
				if(pPlayer->GetTeam() == TEAM_SPECTATORS && Team == CGameContext::CHAT_ALL && !m_pController->IsGameOver())
				{					
					SendChatTarget(ClientID, "Spectators aren't allowed to write to global chat during a tournament game.");
					return;
				}
			}
			
			// trim right and set maximum length to 128 utf8-characters
			int Length = 0;
			const char *p = pMsg->m_pMessage;
			const char *pEnd = 0;
			while(*p)
 			{
				const char *pStrOld = p;
				int Code = str_utf8_decode(&p);

				// check if unicode is not empty
				if(Code > 0x20 && Code != 0xA0 && Code != 0x034F && (Code < 0x2000 || Code > 0x200F) && (Code < 0x2028 || Code > 0x202F) &&
					(Code < 0x205F || Code > 0x2064) && (Code < 0x206A || Code > 0x206F) && (Code < 0xFE00 || Code > 0xFE0F) &&
					Code != 0xFEFF && (Code < 0xFFF9 || Code > 0xFFFC))
				{
					pEnd = 0;
				}
				else if(pEnd == 0)
					pEnd = pStrOld;

				if(++Length >= 256)
				{
					*(const_cast<char *>(p)) = 0;
					break;
				}
 			}
			if(pEnd != 0)
				*(const_cast<char *>(pEnd)) = 0;

			// drop empty and autocreated spam messages (more than 16 characters per second)
			if(Length == 0 || (m_Config->m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat+Server()->TickSpeed()*((15+Length)/16) > Server()->Tick()))
				return;

			//fair spam protection(no mute system needed)
			if (pPlayer->m_LastChat + Server()->TickSpeed()*((15 + Length) / 16 + 1) > Server()->Tick()) {
				++pPlayer->m_ChatSpamCount;
				if (++pPlayer->m_ChatSpamCount >= 5) {
					//"muted" for 2 seconds
					pPlayer->m_LastChat = Server()->Tick() + Server()->TickSpeed() * 2;
					pPlayer->m_ChatSpamCount = 0;
				}
				else pPlayer->m_LastChat = Server()->Tick();
			}
			else {
				pPlayer->m_LastChat = Server()->Tick();
				pPlayer->m_ChatSpamCount = 0;
			}

			if (pMsg->m_pMessage[0] == '/' && Length > 1) {
				ExecuteServerCommand(ClientID, pMsg->m_pMessage + 1);
				return;
			}

			SendChat(ClientID, Team, pMsg->m_pMessage);
		}
		else if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			if(m_Config->m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry+Server()->TickSpeed()*3 > Server()->Tick())
				return;

			int64 Now = Server()->Tick();
			pPlayer->m_LastVoteTry = Now;
			if(pPlayer->GetTeam() == TEAM_SPECTATORS)
			{
				SendChatTarget(ClientID, "Spectators aren't allowed to start a vote.");
				return;
			}

			if(m_VoteCloseTime)
			{
				SendChatTarget(ClientID, "Wait for current vote to end before calling a new one.");
				return;
			}

			int Timeleft = pPlayer->m_LastVoteCall + Server()->TickSpeed()*60 - Now;
			if(pPlayer->m_LastVoteCall && Timeleft > 0)
			{
				char aChatmsg[512] = {0};
				str_format(aChatmsg, sizeof(aChatmsg), "You must wait %d seconds before making another vote", (Timeleft/Server()->TickSpeed())+1);
				SendChatTarget(ClientID, aChatmsg);
				return;
			}

			char aChatmsg[512] = {0};
			char aDesc[VOTE_DESC_LENGTH] = {0};
			char aCmd[VOTE_CMD_LENGTH] = {0};
			CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
			const char *pReason = pMsg->m_Reason[0] ? pMsg->m_Reason : "No reason given";

			if(str_comp_nocase(pMsg->m_Type, "option") == 0)
			{
				CVoteOptionServer *pOption = m_pVoteOptionFirst;
				while(pOption)
				{
					if(str_comp_nocase(pMsg->m_Value, pOption->m_aDescription) == 0)
					{
						str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)", Server()->ClientName(ClientID),
									pOption->m_aDescription, pReason);
						str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);
						str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
						break;
					}

					pOption = pOption->m_pNext;
				}

				if(!pOption)
				{
					str_format(aChatmsg, sizeof(aChatmsg), "'%s' isn't an option on this server", pMsg->m_Value);
					SendChatTarget(ClientID, aChatmsg);
					return;
				}
			}
			else if(str_comp_nocase(pMsg->m_Type, "kick") == 0)
			{
				if(!m_Config->m_SvVoteKick)
				{
					SendChatTarget(ClientID, "Server does not allow voting to kick players");
					return;
				}

				if(m_Config->m_SvVoteKickMin)
				{
					int PlayerNum = 0;
					for(int i = 0; i < MAX_CLIENTS; ++i)
						if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
							++PlayerNum;

					if(PlayerNum < m_Config->m_SvVoteKickMin)
					{
						str_format(aChatmsg, sizeof(aChatmsg), "Kick voting requires %d players on the server", m_Config->m_SvVoteKickMin);
						SendChatTarget(ClientID, aChatmsg);
						return;
					}
				}

				int KickID = str_toint(pMsg->m_Value);
				KickID = pPlayer->GetRealIDFromSnappingClients(KickID);
				if(KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID])
				{
					SendChatTarget(ClientID, "Invalid client id to kick");
					return;
				}
				if(KickID == ClientID)
				{
					SendChatTarget(ClientID, "You can't kick yourself");
					return;
				}
				if(Server()->IsAuthed(KickID))
				{
					SendChatTarget(ClientID, "You can't kick admins");
					char aBufKick[128];
					str_format(aBufKick, sizeof(aBufKick), "'%s' called for vote to kick you", Server()->ClientName(ClientID));
					SendChatTarget(KickID, aBufKick);
					return;
				}

				str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to kick '%s' (%s)", Server()->ClientName(ClientID), Server()->ClientName(KickID), pReason);
				str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickID));
				if (!m_Config->m_SvVoteKickBantime)
					str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);
				else
				{
					char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
					Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
					str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, m_Config->m_SvVoteKickBantime);
				}
			}
			else if(str_comp_nocase(pMsg->m_Type, "spectate") == 0)
			{
				if(!m_Config->m_SvVoteSpectate)
				{
					SendChatTarget(ClientID, "Server does not allow voting to move players to spectators");
					return;
				}

				int SpectateID = str_toint(pMsg->m_Value);
				SpectateID = pPlayer->GetRealIDFromSnappingClients(SpectateID);
				if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
				{
					SendChatTarget(ClientID, "Invalid client id to move");
					return;
				}
				if(SpectateID == ClientID)
				{
					SendChatTarget(ClientID, "You can't move yourself");
					return;
				}

				str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to move '%s' to spectators (%s)", Server()->ClientName(ClientID), Server()->ClientName(SpectateID), pReason);
				str_format(aDesc, sizeof(aDesc), "move '%s' to spectators", Server()->ClientName(SpectateID));
				str_format(aCmd, sizeof(aCmd), "set_team %d -1 %d", SpectateID, m_Config->m_SvVoteSpectateRejoindelay);
			}

			if(aCmd[0])
			{
				SendChat(-1, CGameContext::CHAT_ALL, aChatmsg);
				StartVote(aDesc, aCmd, pReason);
				pPlayer->m_Vote = 1;
				pPlayer->m_VotePos = m_VotePos = 1;
				m_VoteCreator = ClientID;
				pPlayer->m_LastVoteCall = Now;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			if(!m_VoteCloseTime)
				return;

			if(pPlayer->m_Vote == 0)
			{
				CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
				if(!pMsg->m_Vote)
					return;

				pPlayer->m_Vote = pMsg->m_Vote;
				pPlayer->m_VotePos = ++m_VotePos;
				m_VoteUpdate = true;
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SETTEAM && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

			if ((pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsFrozen()) || (pPlayer->GetTeam() == pMsg->m_Team || (m_Config->m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + Server()->TickSpeed() * 3 > Server()->Tick())))
				return;

			if(pMsg->m_Team != TEAM_SPECTATORS && m_LockTeams)
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				SendBroadcast("Teams are locked", ClientID);
				return;
			}

			if(pPlayer->m_TeamChangeTick > Server()->Tick())
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				int TimeLeft = (pPlayer->m_TeamChangeTick - Server()->Tick())/Server()->TickSpeed();
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Time to wait before changing team: %02d:%02d", TimeLeft/60, TimeLeft%60);
				SendBroadcast(aBuf, ClientID);
				return;
			}

			// Switch team on given client and kill/respawn him
			if(m_pController->CanJoinTeam(pMsg->m_Team, ClientID))
			{
				if(m_pController->CanChangeTeam(pPlayer, pMsg->m_Team))
				{
					pPlayer->m_LastSetTeam = Server()->Tick();
					if(pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
						m_VoteUpdate = true;
					pPlayer->SetTeam(pMsg->m_Team);
					(void)m_pController->CheckTeamBalance();
					pPlayer->m_TeamChangeTick = Server()->Tick();
				}
				else
					SendBroadcast("Teams must be balanced, please join other team", ClientID);
			}
			else
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed", Server()->MaxClients()-m_Config->m_SvSpectatorSlots);
				SendBroadcast(aBuf, ClientID);
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			int RealID = pPlayer->GetRealIDFromSnappingClients(pMsg->m_SpectatorID);

			if(pPlayer->GetTeam() != TEAM_SPECTATORS || pPlayer->m_SpectatorID == RealID || ClientID == RealID ||
				(m_Config->m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode+Server()->TickSpeed()*3 > Server()->Tick()))
				return;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();

			if(RealID != SPEC_FREEVIEW && (!m_apPlayers[RealID] || m_apPlayers[RealID]->GetTeam() == TEAM_SPECTATORS) && !m_Config->m_SvTournamentMode)
				SendChatTarget(ClientID, "Invalid spectator id used");
			else
				pPlayer->m_SpectatorID = RealID;
		}
		else if (MsgID == NETMSGTYPE_CL_CHANGEINFO)
		{
			if(m_Config->m_SvSpamprotection && pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo+Server()->TickSpeed()*5 > Server()->Tick())
				return;

			CNetMsg_Cl_ChangeInfo *pMsg = (CNetMsg_Cl_ChangeInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set infos
			char aOldName[MAX_NAME_LENGTH];
			str_copy(aOldName, Server()->ClientName(ClientID), sizeof(aOldName));
			Server()->SetClientName(ClientID, pMsg->m_pName);
			if(str_comp(aOldName, Server()->ClientName(ClientID)) != 0)
			{
				char aChatText[256];
				str_format(aChatText, sizeof(aChatText), "'%s' changed name to '%s'", aOldName, Server()->ClientName(ClientID));
				SendChat(-1, CGameContext::CHAT_ALL, aChatText);
			}
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);
			str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
			pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			m_pController->OnPlayerInfoChange(pPlayer);
		}
		else if (MsgID == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
		{
			CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

			if(m_Config->m_SvSpamprotection && pPlayer->m_LastEmote && pPlayer->m_LastEmote+Server()->TickSpeed()*m_Config->m_SvEmoticonDelay > Server()->Tick())
				return;

			pPlayer->m_LastEmote = Server()->Tick();
			++pPlayer->m_Stats.m_NumEmotes;

			SendEmoticon(ClientID, pMsg->m_Emoticon); 
			
			int eicon = EMOTE_NORMAL;
			switch (pMsg->m_Emoticon) {
			case EMOTICON_OOP: eicon = EMOTE_PAIN; break;
			case EMOTICON_EXCLAMATION: eicon = EMOTE_SURPRISE; break;
			case EMOTICON_HEARTS: eicon = EMOTE_HAPPY; break;
			case EMOTICON_DROP: eicon = EMOTE_BLINK; break;
			case EMOTICON_DOTDOT: eicon = EMOTE_BLINK; break;
			case EMOTICON_MUSIC: eicon = EMOTE_HAPPY; break;
			case EMOTICON_SORRY: eicon = EMOTE_PAIN; break;
			case EMOTICON_GHOST: eicon = EMOTE_SURPRISE; break;
			case EMOTICON_SUSHI: eicon = EMOTE_PAIN; break;
			case EMOTICON_SPLATTEE: eicon = EMOTE_ANGRY;  break;
			case EMOTICON_DEVILTEE: eicon = EMOTE_ANGRY; break;
			case EMOTICON_ZOMG: eicon = EMOTE_ANGRY; break;
			case EMOTICON_ZZZ: eicon = EMOTE_BLINK; break;
			case EMOTICON_WTF: eicon = EMOTE_SURPRISE; break;
			case EMOTICON_EYES: eicon = EMOTE_HAPPY; break;
			case EMOTICON_QUESTION: eicon = EMOTE_SURPRISE; break;
			}
			if (pPlayer->GetCharacter()) pPlayer->GetCharacter()->SetEmote(eicon, Server()->Tick() + Server()->TickSpeed()*2.5f);
		}
		else if (MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
		{
			if((pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsFrozen()) || (pPlayer->m_LastKill && pPlayer->m_LastKill+Server()->TickSpeed()*m_Config->m_SvKillDelay > Server()->Tick()) || m_Config->m_SvKillDelay == -1)
				return;

			pPlayer->m_LastKill = Server()->Tick();
			pPlayer->KillCharacter(WEAPON_SELF);
		}
		else if (MsgID == NETMSGTYPE_CL_ISDDNET)
		{
			pPlayer->m_ClientVersion = CPlayer::CLIENT_VERSION_DDNET;
			int Version = pUnpacker->GetInt();
			pPlayer->m_DDNetVersion = Version;
			Server()->SetClientVersion(ClientID, Version);
			if (pUnpacker->Error() || Version < 217)
			{
				pPlayer->m_ClientVersion = CPlayer::CLIENT_VERSION_NORMAL;
			}
		}
	}
	else
	{
		if(MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			if(pPlayer->m_IsReady)
				return;

			CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set start infos
			Server()->SetClientName(ClientID, pMsg->m_pName);
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);
			str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
			pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			m_pController->OnPlayerInfoChange(pPlayer);

			// send vote options
			CNetMsg_Sv_VoteClearOptions ClearMsg;
			Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

			CNetMsg_Sv_VoteOptionListAdd OptionMsg;
			int NumOptions = 0;
			OptionMsg.m_pDescription0 = "";
			OptionMsg.m_pDescription1 = "";
			OptionMsg.m_pDescription2 = "";
			OptionMsg.m_pDescription3 = "";
			OptionMsg.m_pDescription4 = "";
			OptionMsg.m_pDescription5 = "";
			OptionMsg.m_pDescription6 = "";
			OptionMsg.m_pDescription7 = "";
			OptionMsg.m_pDescription8 = "";
			OptionMsg.m_pDescription9 = "";
			OptionMsg.m_pDescription10 = "";
			OptionMsg.m_pDescription11 = "";
			OptionMsg.m_pDescription12 = "";
			OptionMsg.m_pDescription13 = "";
			OptionMsg.m_pDescription14 = "";
			CVoteOptionServer *pCurrent = m_pVoteOptionFirst;
			while(pCurrent)
			{
				switch(NumOptions++)
				{
				case 0: OptionMsg.m_pDescription0 = pCurrent->m_aDescription; break;
				case 1: OptionMsg.m_pDescription1 = pCurrent->m_aDescription; break;
				case 2: OptionMsg.m_pDescription2 = pCurrent->m_aDescription; break;
				case 3: OptionMsg.m_pDescription3 = pCurrent->m_aDescription; break;
				case 4: OptionMsg.m_pDescription4 = pCurrent->m_aDescription; break;
				case 5: OptionMsg.m_pDescription5 = pCurrent->m_aDescription; break;
				case 6: OptionMsg.m_pDescription6 = pCurrent->m_aDescription; break;
				case 7: OptionMsg.m_pDescription7 = pCurrent->m_aDescription; break;
				case 8: OptionMsg.m_pDescription8 = pCurrent->m_aDescription; break;
				case 9: OptionMsg.m_pDescription9 = pCurrent->m_aDescription; break;
				case 10: OptionMsg.m_pDescription10 = pCurrent->m_aDescription; break;
				case 11: OptionMsg.m_pDescription11 = pCurrent->m_aDescription; break;
				case 12: OptionMsg.m_pDescription12 = pCurrent->m_aDescription; break;
				case 13: OptionMsg.m_pDescription13 = pCurrent->m_aDescription; break;
				case 14:
					{
						OptionMsg.m_pDescription14 = pCurrent->m_aDescription;
						OptionMsg.m_NumOptions = NumOptions;
						Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
						OptionMsg = CNetMsg_Sv_VoteOptionListAdd();
						NumOptions = 0;
						OptionMsg.m_pDescription1 = "";
						OptionMsg.m_pDescription2 = "";
						OptionMsg.m_pDescription3 = "";
						OptionMsg.m_pDescription4 = "";
						OptionMsg.m_pDescription5 = "";
						OptionMsg.m_pDescription6 = "";
						OptionMsg.m_pDescription7 = "";
						OptionMsg.m_pDescription8 = "";
						OptionMsg.m_pDescription9 = "";
						OptionMsg.m_pDescription10 = "";
						OptionMsg.m_pDescription11 = "";
						OptionMsg.m_pDescription12 = "";
						OptionMsg.m_pDescription13 = "";
						OptionMsg.m_pDescription14 = "";
					}
				}
				pCurrent = pCurrent->m_pNext;
			}
			if(NumOptions > 0)
			{
				OptionMsg.m_NumOptions = NumOptions;
				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
			}

			// send tuning parameters to client
			SendTuningParams(ClientID);

			// client is ready to enter
			pPlayer->m_IsReady = true;
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
		}
	}
}

sServerCommand* CGameContext::FindCommand(const char* pCmd){
	if(!pCmd) return NULL;
	sServerCommand* pFindCmd = m_FirstServerCommand;
	while(pFindCmd){
		if(str_comp_nocase_whitespace(pFindCmd->m_Cmd, pCmd) == 0) return pFindCmd;
		pFindCmd = pFindCmd->m_NextCommand;
	}
	return NULL;
}

void CGameContext::AddServerCommandSorted(sServerCommand* pCmd){
	if(!m_FirstServerCommand){
		m_FirstServerCommand = pCmd;
	} else {
		sServerCommand* pFindCmd = m_FirstServerCommand;
		sServerCommand* pPrevFindCmd = 0;
		while(pFindCmd){
			if(str_comp_nocase(pCmd->m_Cmd, pFindCmd->m_Cmd) < 0) {
				if(pPrevFindCmd){
					pPrevFindCmd->m_NextCommand = pCmd;
				} else {
					m_FirstServerCommand = pCmd;
				}
				pCmd->m_NextCommand = pFindCmd;
				return;
			}
			pPrevFindCmd = pFindCmd;
			pFindCmd = pFindCmd->m_NextCommand;
		}
		pPrevFindCmd->m_NextCommand = pCmd;
	}
}

void CGameContext::AddServerCommand(const char* pCmd, const char* pDesc, const char* pArgFormat, ServerCommandExecuteFunc pFunc){
	if(!pCmd) return;
	sServerCommand* pFindCmd = FindCommand(pCmd);
	if(!pFindCmd){
		pFindCmd = new sServerCommand(pCmd, pDesc, pArgFormat, pFunc);	
		AddServerCommandSorted(pFindCmd);
	} else {
		pFindCmd->m_Func = pFunc;
		pFindCmd->m_Desc = pDesc;
		pFindCmd->m_ArgFormat = pArgFormat;
	}
}

void CGameContext::ExecuteServerCommand(int pClientID, const char* pLine){
	if(!pLine || !*pLine) return;
	const char* pCmdEnd = pLine;
	while(*pCmdEnd && !is_whitespace(*pCmdEnd)) ++pCmdEnd;
	
	const char* pArgs = (*pCmdEnd) ? pCmdEnd + 1 : 0;
	
	sServerCommand* pCmd = FindCommand(pLine);
	if(pCmd) pCmd->ExecuteCommand(this, pClientID, pArgs);
	else SendChatTarget(pClientID, "Server command not found");
}

void CGameContext::CmdStats(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum){
	CPlayer* p = pContext->m_apPlayers[pClientID];
	
	char buff[600];

	str_format(buff, 600, "╔═════════ Statistics ═════════\n"
		"║\n"
		"║Kills(weapon): %d\n"
		"║Hits(By opponent's weapon): %d\n"
		"║\n"
		"║Kills/Deaths: %4.2f\n"
		"║Shots | Kills/Shots: %d | %3.1f%%\n"
		"║\n"
		"╠══════════ Spikes ══════════\n"
		"║\n"
		"║Kills(normal spikes): %d\n"
		"║Kills(team spikes): %d\n"
		"║Kills(golden spikes): %d\n"
		"║Kills(false spikes): %d\n"
		"║Spike deaths(while frozen): %d\n"
		"║\n"
		"╠═══════════ Misc ══════════\n"
		"║\n"
		"║Mates hammer-/unfrozen: %d/%d\n"
		"║\n"
		"╚══════════════════════════\n", p->m_Stats.m_Kills, p->m_Stats.m_Hits, (p->m_Stats.m_Hits != 0) ? (float)((float)p->m_Stats.m_Kills / (float)p->m_Stats.m_Hits) : (float)p->m_Stats.m_Kills, p->m_Stats.m_Shots, ((float)p->m_Stats.m_Kills / (float)(p->m_Stats.m_Shots == 0 ? 1: p->m_Stats.m_Shots)) * 100.f, p->m_Stats.m_GrabsNormal, p->m_Stats.m_GrabsTeam, p->m_Stats.m_GrabsGold, p->m_Stats.m_GrabsFalse, p->m_Stats.m_Deaths, p->m_Stats.m_UnfreezingHammerHits, p->m_Stats.m_Unfreezes);

	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = buff;
	pContext->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, pClientID);
}

void CGameContext::CmdWhisper(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum){
	CPlayer* p = pContext->m_apPlayers[pClientID];
	
	if (ArgNum > 1) {

		int max = 0;
		int maxPlayerID = -1;
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			if (pContext->m_apPlayers[i]) {
				const char* name = pContext->Server()->ClientName(i);
				const char* arg_names = *pArgs;

				int count = 0;
				while (*name && *arg_names) {
					//if the name char isn't equal to the name in argument, we break
					if (*name != *arg_names) break;
					++count;
					++name; ++arg_names;
				}

				//If the player name is at its end, we found a player with that name
				if (*name == 0) {
					if (max < count) {
						max = count;
						maxPlayerID = i;
					}
				}
			}
		}


		if (maxPlayerID == -1) {
			pContext->SendChatTarget(pClientID, "Player not found.");
			return;
		}
		else {
			int length = str_length(*pArgs);
			if (length > max + 1 && is_whitespace((*pArgs)[max])) {
				char buff[257];
				if(pContext->m_apPlayers[maxPlayerID]->m_ClientVersion != CPlayer::CLIENT_VERSION_DDNET) {
					str_format(buff, 256, "[← %s] %s", pContext->Server()->ClientName(pClientID), (*pArgs + max + 1));
					pContext->SendChatTarget(maxPlayerID, buff);
				}
				else {
					pContext->SendChat(pClientID, CHAT_WHISPER_RECV, (*pArgs + max + 1), maxPlayerID);
				}
				

				if(p->m_ClientVersion != CPlayer::CLIENT_VERSION_DDNET) {
					str_format(buff, 256, "[→ %s] %s", pContext->Server()->ClientName(maxPlayerID), (*pArgs + max + 1));
					pContext->SendChatTarget(pClientID, buff);
				}
				else {
					pContext->SendChat(maxPlayerID, CHAT_WHISPER_SEND, (*pArgs + max + 1), pClientID);
				}

				p->m_WhisperPlayer.PlayerID = maxPlayerID;
				memcpy(p->m_WhisperPlayer.PlayerName, pContext->Server()->ClientName(maxPlayerID), strlen(pContext->Server()->ClientName(maxPlayerID)) + 1);
			}
			else {
				pContext->SendChatTarget(pClientID, "No whisper text written.");
			}
		}
	}
	else pContext->SendChatTarget(pClientID, "[/whisper] usage: /w <playname> <text>");
}

void CGameContext::CmdConversation(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum){
	CPlayer* p = pContext->m_apPlayers[pClientID];
	
	if (ArgNum >= 1) {
		if (p->m_WhisperPlayer.PlayerID != -1) {
			if (strcmp(pContext->Server()->ClientName(p->m_WhisperPlayer.PlayerID), p->m_WhisperPlayer.PlayerName) == 0) {
				char buff[257];
				if(pContext->m_apPlayers[p->m_WhisperPlayer.PlayerID]->m_ClientVersion != CPlayer::CLIENT_VERSION_DDNET) {
					str_format(buff, 256, "[← %s] %s", pContext->Server()->ClientName(pClientID), (*pArgs));
					pContext->SendChatTarget(p->m_WhisperPlayer.PlayerID, buff);
				}
				else {
					pContext->SendChat(pClientID, CHAT_WHISPER_RECV, (*pArgs), p->m_WhisperPlayer.PlayerID);
				}

				if(p->m_ClientVersion != CPlayer::CLIENT_VERSION_DDNET) {
					str_format(buff, 256, "[→ %s] %s", pContext->Server()->ClientName(p->m_WhisperPlayer.PlayerID), (*pArgs));
					pContext->SendChatTarget(pClientID, buff);
				}
				else {
					pContext->SendChat(p->m_WhisperPlayer.PlayerID, CHAT_WHISPER_SEND, (*pArgs), pClientID);
				}
			}
			else pContext->SendChatTarget(pClientID, "Player left the game or renamed.");
		}
		else pContext->SendChatTarget(pClientID, "No player whispered to yet.");
	}
	else pContext->SendChatTarget(pClientID, "[/conversation] usage: /c <text>, after you already whispered to a player");
}

void CGameContext::CmdHelp(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum){
	CPlayer* p = pContext->m_apPlayers[pClientID];
	
	if (ArgNum != 0) {
		sServerCommand* pCmd = pContext->FindCommand(pArgs[0]);
		if(pCmd) {
			char buff[200];
			str_format(buff, 200, "[/%s] %s", pCmd->m_Cmd, pCmd->m_Desc);
			pContext->SendChatTarget(pClientID, buff);
		}
	}
	else {
		pContext->SendChatTarget(pClientID, "command list: type /help <command> for more information");
		
		sServerCommand* pCmd = pContext->m_FirstServerCommand;
		char buff[200];
		while(pCmd){
			str_format(buff, 200, "/%s %s", pCmd->m_Cmd, (pCmd->m_ArgFormat) ? pCmd->m_ArgFormat : "");
			pContext->SendChatTarget(pClientID, buff);
			pCmd = pCmd->m_NextCommand;			
		}
	}
}

void CGameContext::CmdEmote(CGameContext* pContext, int pClientID, const char** pArgs, int ArgNum){
	CPlayer* pPlayer = pContext->m_apPlayers[pClientID];
	
	if (ArgNum > 0) {
		if (!str_comp_nocase_whitespace(pArgs[0], "angry"))
				pPlayer->m_Emotion = EMOTE_ANGRY;
			else if (!str_comp_nocase_whitespace(pArgs[0], "blink"))
				pPlayer->m_Emotion = EMOTE_BLINK;
			else if (!str_comp_nocase_whitespace(pArgs[0], "close"))
				pPlayer->m_Emotion = EMOTE_BLINK;
			else if (!str_comp_nocase_whitespace(pArgs[0], "happy"))
				pPlayer->m_Emotion = EMOTE_HAPPY;
			else if (!str_comp_nocase_whitespace(pArgs[0], "pain"))
				pPlayer->m_Emotion = EMOTE_PAIN;
			else if (!str_comp_nocase_whitespace(pArgs[0], "surprise"))
				pPlayer->m_Emotion = EMOTE_SURPRISE;
			else if (!str_comp_nocase_whitespace(pArgs[0], "normal"))
				pPlayer->m_Emotion = EMOTE_NORMAL;
			else
				pContext->SendChatTarget(pClientID, "Unknown emote... Say /emote");
			
			int Duration = pContext->Server()->TickSpeed();
			if(ArgNum > 1) Duration = str_toint(pArgs[1]);
			
			pPlayer->m_EmotionDuration = Duration * pContext->Server()->TickSpeed();
	} else {
		//ddrace like
		pContext->SendChatTarget(pClientID, "Emote commands are: /emote surprise /emote blink /emote close /emote angry /emote happy /emote pain");
		pContext->SendChatTarget(pClientID, "Example: /emote surprise 10 for 10 seconds or /emote surprise (default 1 second)");
	}
}


void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float NewValue = pResult->GetFloat(1);

	if(pSelf->Tuning()->Set(pParamName, NewValue))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		pSelf->SendTuningParams(-1);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
}

void CGameContext::ConTuneReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CTuningParams TuningParams;
	*pSelf->Tuning() = TuningParams;
	pSelf->SendTuningParams(-1);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
}

void CGameContext::ConTuneDump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[256];
	for(int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float v;
		pSelf->Tuning()->Get(i, &v);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", pSelf->Tuning()->m_apNames[i], v);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConPause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->TogglePause();
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ChangeMap(pResult->NumArguments() ? pResult->GetString(0) : "");
}

void CGameContext::ConRestart(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
		pSelf->m_pController->DoWarmup(pResult->GetInteger(0));
	else
		pSelf->m_pController->StartRound();
}

void CGameContext::ConBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendBroadcast(pResult->GetString(0), -1);
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, pResult->GetString(0));
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int Team = pSelf->m_pController->ClampTeam(pResult->GetInteger(1));
	int Delay = pResult->NumArguments()>2 ? pResult->GetInteger(2) : 0;
	if(!pSelf->m_apPlayers[ClientID])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick = pSelf->Server()->Tick()+pSelf->Server()->TickSpeed()*Delay*60;
	pSelf->m_apPlayers[ClientID]->SetTeam(Team);
	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Team = pSelf->m_pController->ClampTeam(pResult->GetInteger(1));

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", pSelf->m_pController->GetTeamName(Team));
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(pSelf->m_apPlayers[i])
			pSelf->m_apPlayers[i]->SetTeam(Team, false);

	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConSwapTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SwapTeams();
}

void CGameContext::ConShuffleTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->m_pController->IsTeamplay())
		return;
	
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, "Teams were shuffled");
	
	pSelf->m_pController->ShuffleTeams();
	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConLockTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_LockTeams ^= 1;
	if(pSelf->m_LockTeams)
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, "Teams were locked");
	else
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, "Teams were unlocked");
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	if(pSelf->m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	// check for valid option
	if(!pSelf->Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while(*pDescription && *pDescription == ' ')
		pDescription++;
	if(str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++pSelf->m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)pSelf->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = pSelf->m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	pSelf->m_pVoteOptionLast = pOption;
	if(!pSelf->m_pVoteOptionFirst)
		pSelf->m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, pCommand, Len+1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// inform clients about added option
	CNetMsg_Sv_VoteOptionAdd OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
}

void CGameContext::ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if(!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// inform clients about removed option
	CNetMsg_Sv_VoteOptionRemove OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);

	// TODO: improve this
	// remove the option
	--pSelf->m_NumVoteOptions;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "removed option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	CHeap *pVoteOptionHeap = new CHeap();
	CVoteOptionServer *pVoteOptionFirst = 0;
	CVoteOptionServer *pVoteOptionLast = 0;
	int NumVoteOptions = pSelf->m_NumVoteOptions;
	for(CVoteOptionServer *pSrc = pSelf->m_pVoteOptionFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if(pSrc == pOption)
			continue;

		// copy option
		int Len = str_length(pSrc->m_aCommand);
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if(!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription, sizeof(pDst->m_aDescription));
		mem_copy(pDst->m_aCommand, pSrc->m_aCommand, Len+1);
	}

	// clean up
	delete pSelf->m_pVoteOptionHeap;
	pSelf->m_pVoteOptionHeap = pVoteOptionHeap;
	pSelf->m_pVoteOptionFirst = pVoteOptionFirst;
	pSelf->m_pVoteOptionLast = pVoteOptionLast;
	pSelf->m_NumVoteOptions = NumVoteOptions;
}

void CGameContext::ConForceVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pType = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);
	const char *pReason = pResult->NumArguments() > 2 && pResult->GetString(2)[0] ? pResult->GetString(2) : "No reason given";
	char aBuf[128] = {0};

	if(str_comp_nocase(pType, "option") == 0)
	{
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while(pOption)
		{
			if(str_comp_nocase(pValue, pOption->m_aDescription) == 0)
			{
				str_format(aBuf, sizeof(aBuf), "admin forced server option '%s' (%s)", pValue, pReason);
				pSelf->SendChatTarget(-1, aBuf);
				pSelf->Console()->ExecuteLine(pOption->m_aCommand);
				break;
			}

			pOption = pOption->m_pNext;
		}

		if(!pOption)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' isn't an option on this server", pValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
	}
	else if(str_comp_nocase(pType, "kick") == 0)
	{
		int KickID = str_toint(pValue);
		if(KickID < 0 || KickID >= MAX_CLIENTS || !pSelf->m_apPlayers[KickID])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to kick");
			return;
		}

		if (!pSelf->m_Config->m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickID, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, pSelf->m_Config->m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
	}
	else if(str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateID = str_toint(pValue);
		if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateID] || pSelf->m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to move");
			return;
		}

		str_format(aBuf, sizeof(aBuf), "admin moved '%s' to spectator (%s)", pSelf->Server()->ClientName(SpectateID), pReason);
		pSelf->SendChatTarget(-1, aBuf);
		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateID, pSelf->m_Config->m_SvVoteSpectateRejoindelay);
		pSelf->Console()->ExecuteLine(aBuf);
	}
}

void CGameContext::ConClearVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "cleared votes");
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	pSelf->m_pVoteOptionHeap->Reset();
	pSelf->m_pVoteOptionFirst = 0;
	pSelf->m_pVoteOptionLast = 0;
	pSelf->m_NumVoteOptions = 0;
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	// check if there is a vote running
	if(!pSelf->m_VoteCloseTime)
		return;

	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_YES;
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "admin forced vote %s", pResult->GetString(0));
	pSelf->SendChatTarget(-1, aBuf);
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CNetMsg_Sv_Motd Msg;
		Msg.m_pMessage = pSelf->m_Config->m_SvMotd;
		CGameContext *pSelf = (CGameContext *)pUserData;
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(pSelf->m_apPlayers[i])
				pSelf->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}
}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	Console()->Register("tune", "si", CFGFLAG_SERVER, ConTuneParam, this, "Tune variable to value");
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, ConTuneReset, this, "Reset tuning");
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, ConTuneDump, this, "Dump tuning");

	Console()->Register("pause", "", CFGFLAG_SERVER, ConPause, this, "Pause/unpause game");
	Console()->Register("change_map", "?r", CFGFLAG_SERVER|CFGFLAG_STORE, ConChangeMap, this, "Change map");
	Console()->Register("restart", "?i", CFGFLAG_SERVER|CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)");
	Console()->Register("broadcast", "r", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("say", "r", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("set_team", "ii?i", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");
	Console()->Register("set_team_all", "i", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team");
	Console()->Register("swap_teams", "", CFGFLAG_SERVER, ConSwapTeams, this, "Swap the current teams");
	Console()->Register("shuffle_teams", "", CFGFLAG_SERVER, ConShuffleTeams, this, "Shuffle the current teams");
	Console()->Register("lock_teams", "", CFGFLAG_SERVER, ConLockTeams, this, "Lock/unlock teams");

	Console()->Register("add_vote", "sr", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "s", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("force_vote", "ss?r", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("vote", "r", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");

	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);
}

void CGameContext::OnInit(/*class IKernel *pKernel*/)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);
	
	AddServerCommand("stats", "show the stats of the current game", 0, CmdStats);
	AddServerCommand("s", "show the stats of the current game", 0, CmdStats);
	AddServerCommand("whisper", "whisper to a player in the server privately", "<playername> <text>", CmdWhisper);
	AddServerCommand("w", "whisper to a player in the server privately", "<playername> <text>", CmdWhisper);
	AddServerCommand("conversation", "whisper to the player, you whispered to last", "<text>", CmdConversation);
	AddServerCommand("c", "whisper to the player, you whispered to last", "<text>", CmdConversation);
	AddServerCommand("help", "show the cmd list or get more information to any command", "<command>", CmdHelp);
	AddServerCommand("cmdlist", "show the cmd list", 0, CmdHelp);
	if(m_Config->m_SvEmoteWheel || m_Config->m_SvEmotionalTees) AddServerCommand("emote", "enable custom emotes", "<emote type> <time in seconds>", CmdEmote);

	//if(!data) // only load once
		//data = load_data_from_memory(internal_data);

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers);

	// select gametype
	if (str_comp(m_Config->m_SvGametype, "fng2") == 0)
		m_pController = new CGameControllerFNG2(this);
	else if (str_comp(m_Config->m_SvGametype, "fng2solo") == 0)
		m_pController = new CGameControllerFNG2Solo(this);
	else if (str_comp(m_Config->m_SvGametype, "fng2boom") == 0)
		m_pController = new CGameControllerFNG2Boom(this);
	else if (str_comp(m_Config->m_SvGametype, "fng2boomsolo") == 0)
		m_pController = new CGameControllerFNG2BoomSolo(this);
	else if (str_comp(m_Config->m_SvGametype, "fng24teams") == 0)
		m_pController = new CGameControllerFNG24Teams(this);
	else 
#define CONTEXT_INIT_WITHOUT_CONFIG
#include "gamecontext_additional_gametypes.h"
#undef CONTEXT_INIT_WITHOUT_CONFIG
		m_pController = new CGameControllerFNG2(this);
		
	if(m_Config->m_SvEmoteWheel) m_pController->m_pGameType = "fng2+";

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);

	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y*pTileMap->m_Width+x].m_Index;

			if(Index >= 128)
			{
				vec2 Pos(x*32.0f+16.0f, y*32.0f+16.0f);
				m_pController->OnEntity(Index-ENTITY_OFFSET, Pos);
			}
		}
	}

	//game.world.insert_entity(game.Controller);

#ifdef CONF_DEBUG
	if(m_Config->m_DbgDummies)
	{
		for(int i = 0; i < m_Config->m_DbgDummies ; i++)
		{
			OnClientConnected(MAX_CLIENTS-i-1);
		}
	}
#endif
}


void CGameContext::OnInit(IKernel *pKernel, IMap* pMap, CConfiguration* pConfigFile)
{
	IKernel *kernel = NULL;
	if(pKernel != NULL) kernel = pKernel;
	else kernel = Kernel();
	
	if(Kernel() == NULL) SetKernel(kernel);
	
	m_pServer = kernel->RequestInterface<IServer>();
	m_pConsole = kernel->RequestInterface<IConsole>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);
	
	AddServerCommand("stats", "show the stats of the current game", 0, CmdStats);
	AddServerCommand("s", "show the stats of the current game", 0, CmdStats);
	AddServerCommand("whisper", "whisper to a player in the server privately", "<playername> <text>", CmdWhisper);
	AddServerCommand("w", "whisper to a player in the server privately", "<playername> <text>", CmdWhisper);
	AddServerCommand("conversation", "whisper to the player, you whispered to last", "<text>", CmdConversation);
	AddServerCommand("c", "whisper to the player, you whispered to last", "<text>", CmdConversation);
	AddServerCommand("help", "show the cmd list or get more information to any command", "<command>", CmdHelp);
	AddServerCommand("cmdlist", "show the cmd list", 0, CmdHelp);
	if(m_Config->m_SvEmoteWheel || m_Config->m_SvEmotionalTees) AddServerCommand("emote", "enable custom emotes", "<emote type> <time in seconds>", CmdEmote);

	//if(!data) // only load once
		//data = load_data_from_memory(internal_data);

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(kernel, pMap);
	m_Collision.Init(&m_Layers);

	
	CConfiguration* pConfig;
	CConfiguration Config;
	if(pConfigFile) {
		pConfig = pConfigFile;
		m_Config = pConfig;
	} else {
		pConfig = &Config;
	}
	
	if (str_comp(pConfig->m_SvGametype, "fng2") == 0)
		m_pController = new CGameControllerFNG2(this, *pConfig);
	else if (str_comp(pConfig->m_SvGametype, "fng2solo") == 0)
		m_pController = new CGameControllerFNG2Solo(this, *pConfig);
	else if (str_comp(pConfig->m_SvGametype, "fng2boom") == 0)
		m_pController = new CGameControllerFNG2Boom(this, *pConfig);
	else if (str_comp(pConfig->m_SvGametype, "fng2boomsolo") == 0)
		m_pController = new CGameControllerFNG2BoomSolo(this, *pConfig);
	else if (str_comp(pConfig->m_SvGametype, "fng24teams") == 0)
		m_pController = new CGameControllerFNG24Teams(this, *pConfig);
	else 
#include "gamecontext_additional_gametypes.h"
		m_pController = new CGameControllerFNG2(this, *pConfig);

	if(m_Config->m_SvEmoteWheel) m_pController->m_pGameType = "fng2+";
	
	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)pMap->GetData(pTileMap->m_Data);

	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y*pTileMap->m_Width+x].m_Index;

			if(Index >= 128)
			{
				vec2 Pos(x*32.0f+16.0f, y*32.0f+16.0f);
				m_pController->OnEntity(Index-ENTITY_OFFSET, Pos);
			}
		}
	}

#ifdef CONF_DEBUG
	if(m_Config->m_DbgDummies)
	{
		for(int i = 0; i < m_Config->m_DbgDummies ; i++)
		{
			OnClientConnected(MAX_CLIENTS-i-1);
		}
	}
#endif
}

void CGameContext::OnShutdown()
{
	delete m_pController;
	m_pController = 0;
	Clear();
}

void CGameContext::OnSnap(int ClientID)
{
	// add tuning to demo
	CTuningParams StandardTuning;
	if(ClientID == -1 && Server()->DemoRecorder_IsRecording() && mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning;
		for(unsigned i = 0; i < sizeof(m_Tuning)/sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Server()->SendMsg(&Msg, MSGFLAG_RECORD|MSGFLAG_NOSEND, ClientID);
	}

	m_World.Snap(ClientID);
	m_pController->Snap(ClientID);
	m_Events.Snap(ClientID);

	if (ClientID > -1 && m_apPlayers[ClientID]) {
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i])
				m_apPlayers[i]->Snap(ClientID);
		}

		if(m_apPlayers[ClientID]->m_ClientVersion != CPlayer::CLIENT_VERSION_DDNET)
			m_apPlayers[ClientID]->FakeSnap();
		else 
			m_apPlayers[ClientID]->FakeSnap(CPlayer::DDNET_CLIENT_MAX_CLIENTS - 1);
	}
	else if (ClientID == -1) {
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i])
				m_apPlayers[i]->Snap(ClientID);
		}
	}
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_Events.Clear();
}

bool CGameContext::IsClientReady(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReady ? true : false;
}

bool CGameContext::IsClientPlayer(int ClientID)
{
	return m_apPlayers[ClientID] && (m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS ? false : true);
}

const char *CGameContext::GameType() { return m_pController && m_pController->m_pGameType ? m_pController->m_pGameType : ""; }
const char *CGameContext::Version() { return m_Config->m_SvEmoteWheel ? GAME_VERSION_PLUS : GAME_VERSION; }
const char *CGameContext::NetVersion() { return GAME_NETVERSION; }


void CGameContext::SendRoundStats() {
	char buff[300];
	float bestKD = 0;
	float bestAccuracy = 0;
	QuadroMask bestKDPlayerIDs(0);
	QuadroMask bestAccuarcyPlayerIDs(0);
	
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		CPlayer* p = m_apPlayers[i];
		if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
		SendChatTarget(i, "╔═════════ Statistics ═════════");
		SendChatTarget(i, "║");

		str_format(buff, 300, "║Kills(weapon): %d", p->m_Stats.m_Kills);
		SendChatTarget(i, buff);
		str_format(buff, 300, "║Hits(By opponent's weapon): %d", p->m_Stats.m_Hits);
		SendChatTarget(i, buff);
		SendChatTarget(i, "║");
		str_format(buff, 300, "║Kills/Deaths: %4.2f", (p->m_Stats.m_Hits != 0) ? (float)((float)p->m_Stats.m_Kills / (float)p->m_Stats.m_Hits) : (float)p->m_Stats.m_Kills);
		SendChatTarget(i, buff);		
		str_format(buff, 300, "║Shots | Kills/Shots: %d | %3.1f%%\n", p->m_Stats.m_Shots, ((float)p->m_Stats.m_Kills / (float)(p->m_Stats.m_Shots == 0 ? 1: p->m_Stats.m_Shots)) * 100.f);
		SendChatTarget(i, buff);
		SendChatTarget(i, "║");
		SendChatTarget(i, "╠══════════ Spikes ══════════");
		SendChatTarget(i, "║");
		str_format(buff, 300, "║Kills(normal spikes): %d", p->m_Stats.m_GrabsNormal);
		SendChatTarget(i, buff);
		str_format(buff, 300, "║Kills(team spikes): %d", p->m_Stats.m_GrabsTeam);
		SendChatTarget(i, buff);
		str_format(buff, 300, "║Kills(golden spikes): %d", p->m_Stats.m_GrabsGold);
		SendChatTarget(i, buff);
		str_format(buff, 300, "║Kills(false spikes): %d", p->m_Stats.m_GrabsFalse);
		SendChatTarget(i, buff);
		str_format(buff, 300, "║Spike deaths(while frozen): %d", p->m_Stats.m_Deaths);
		SendChatTarget(i, buff);
		SendChatTarget(i, "║");
		SendChatTarget(i, "╠═══════════ Misc ══════════");
		SendChatTarget(i, "║");
		str_format(buff, 300, "║Teammates hammered/unfrozen: %d / %d", p->m_Stats.m_UnfreezingHammerHits, p->m_Stats.m_Unfreezes);
		SendChatTarget(i, buff);
		SendChatTarget(i, "║");
		SendChatTarget(i, "╚══════════════════════════");
		SendChatTarget(i, "Press F1 to view stats now!!");

		float kd = ((p->m_Stats.m_Hits != 0) ? (float)((float)p->m_Stats.m_Kills / (float)p->m_Stats.m_Hits) : (float)p->m_Stats.m_Kills);
		if (bestKD < kd) {
			bestKD = kd;
			bestKDPlayerIDs = 0;
			bestKDPlayerIDs.SetBitOfPosition(i);
		}
		else if (bestKD == kd) {
			bestKDPlayerIDs.SetBitOfPosition(i);
		}

		float accuracy = (float)p->m_Stats.m_Kills / (float)(p->m_Stats.m_Shots == 0 ? 1 : p->m_Stats.m_Shots);
		if (bestAccuracy < accuracy) {
			bestAccuracy = accuracy;
			bestAccuarcyPlayerIDs = 0;
			bestAccuarcyPlayerIDs.SetBitOfPosition(i);
		}
		else if (bestAccuracy == accuracy) {
			bestAccuarcyPlayerIDs.SetBitOfPosition(i);
		}
	}

	int bestKDCount = bestKDPlayerIDs.Count();
	if (bestKDCount > 0) {
		char buff[300];
		if(bestKDCount == 1){
			str_format(buff, 300, "Best player: %s with a K/D of %.3f", Server()->ClientName(bestKDPlayerIDs.PositionOfNonZeroBit(0)), bestKD);
		} else {
			//only allow upto 10 players at once(else we risk buffer overflow)
			int curPlayerCount = 0;
			int curPlayerIDOffset = -1;
			
			char PlayerNames[300];
			
			int CharacterOffset = 0;
			while((curPlayerIDOffset = bestKDPlayerIDs.PositionOfNonZeroBit(curPlayerIDOffset + 1)) != -1){
				if(curPlayerCount > 0){					
					str_format((PlayerNames + CharacterOffset), 300 - CharacterOffset,  ", ");
					CharacterOffset = str_length(PlayerNames);
				}
				str_format((PlayerNames + CharacterOffset), 300 - CharacterOffset, "%s", Server()->ClientName(curPlayerIDOffset));
				CharacterOffset = str_length(PlayerNames);
				++curPlayerCount;
				
				if(curPlayerCount > 10) break;
			}
			
			if(curPlayerCount > 10) str_format((PlayerNames + CharacterOffset), 300 - CharacterOffset, " and others");
			
			str_format(buff, 300, "Best players: %s with a K/D of %.3f", PlayerNames, bestKD);
		}
		SendChat(-1, CGameContext::CHAT_ALL, buff);
	}

	int bestAccuracyCount = bestAccuarcyPlayerIDs.Count();
	if (bestAccuracyCount > 0) {
		char buff[300];
		if (bestAccuracyCount == 1) {
			str_format(buff, 300, "Best accuracy: %s with %3.1f%%", Server()->ClientName(bestAccuarcyPlayerIDs.PositionOfNonZeroBit(0)), bestAccuracy * 100.f);
		}
		else {
			//only allow upto 10 players at once(else we risk buffer overflow)
			int curPlayerCount = 0;
			int curPlayerIDOffset = -1;

			char PlayerNames[300];

			int CharacterOffset = 0;
			while ((curPlayerIDOffset = bestAccuarcyPlayerIDs.PositionOfNonZeroBit(curPlayerIDOffset + 1)) != -1) {
				if (curPlayerCount > 0) {
					str_format((PlayerNames + CharacterOffset), 300 - CharacterOffset, ", ");
					CharacterOffset = str_length(PlayerNames);
				}
				str_format((PlayerNames + CharacterOffset), 300 - CharacterOffset, "%s", Server()->ClientName(curPlayerIDOffset));
				CharacterOffset = str_length(PlayerNames);
				++curPlayerCount;

				if (curPlayerCount > 10) break;
			}

			if (curPlayerCount > 10) str_format((PlayerNames + CharacterOffset), 300 - CharacterOffset, " and others");

			str_format(buff, 300, "Best accuracy: %s with %3.1f%%", PlayerNames, bestAccuracy * 100.f);
		}
		SendChat(-1, CGameContext::CHAT_ALL, buff);
	}
	
	if(m_Config->m_SvTrivia) SendRandomTrivia();
}

void CGameContext::SendRandomTrivia(){	
	srand (time(NULL));
	int r = rand()%8;
	
	bool TriviaSent = false;
	
	//most jumps
	if(r == 0){
		int MaxJumps = 0;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxJumps < p->m_Stats.m_NumJumped){
				MaxJumps = p->m_Stats.m_NumJumped;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s jumped %d time%s in this round.", Server()->ClientName(PlayerID), MaxJumps, (MaxJumps == 1 ? "" : "s"));
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//longest travel distance
	else if(r == 1){
		float MaxTilesMoved = 0.f;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxTilesMoved < p->m_Stats.m_NumTilesMoved){
				MaxTilesMoved = p->m_Stats.m_NumTilesMoved;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s moved %5.2f tiles in this round.", Server()->ClientName(PlayerID), MaxTilesMoved/32.f);
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//most hooks
	else if(r == 2){
		int MaxHooks = 0;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxHooks < p->m_Stats.m_NumHooks){
				MaxHooks = p->m_Stats.m_NumHooks;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s hooked %d time%s in this round.", Server()->ClientName(PlayerID), MaxHooks, (MaxHooks == 1 ? "" : "s"));
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//fastest player
	else if(r == 3){
		float MaxSpeed = 0.f;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxSpeed < p->m_Stats.m_MaxSpeed){
				MaxSpeed = p->m_Stats.m_MaxSpeed;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s was the fastest player with %4.2f tiles per second(no fallspeed).", Server()->ClientName(PlayerID), (MaxSpeed*(float)Server()->TickSpeed())/32.f);
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//most bounces
	else if(r == 4){
		int MaxTeeCols = 0;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxTeeCols < p->m_Stats.m_NumTeeCollisions){
				MaxTeeCols = p->m_Stats.m_NumTeeCollisions;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s bounced %d time%s from other players.", Server()->ClientName(PlayerID), MaxTeeCols, (MaxTeeCols == 1 ? "" : "s"));
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//player longest freeze time
	else if(r == 5){
		int MaxFreezeTicks = 0;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxFreezeTicks < p->m_Stats.m_NumFreezeTicks){
				MaxFreezeTicks = p->m_Stats.m_NumFreezeTicks;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s was frozen for %4.2f seconds total this round.", Server()->ClientName(PlayerID), (float)MaxFreezeTicks/(float)Server()->TickSpeed());
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//player with most hammers to frozen teammates
	else if(r == 6){
		int MaxUnfreezeHammers = 0;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxUnfreezeHammers < p->m_Stats.m_UnfreezingHammerHits){
				MaxUnfreezeHammers = p->m_Stats.m_UnfreezingHammerHits;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s hammered %d frozen teammate%s.", Server()->ClientName(PlayerID), MaxUnfreezeHammers, (MaxUnfreezeHammers == 1 ? "" : "s"));
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//player with most emotes
	else if(r == 7){
		int MaxEmotes = 0;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxEmotes < p->m_Stats.m_NumEmotes){
				MaxEmotes = p->m_Stats.m_NumEmotes;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s emoted %d time%s.", Server()->ClientName(PlayerID), MaxEmotes, (MaxEmotes == 1 ? "" : "s"));
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//player that was hit most often
	else if(r == 8){
		int MaxHit = 0;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxHit < p->m_Stats.m_Hits){
				MaxHit = p->m_Stats.m_Hits;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s was hitted %d time%s by the opponent's weapon.", Server()->ClientName(PlayerID), MaxHit, (MaxHit == 1 ? "" : "s"));
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//player that was thrown into spikes most often by opponents
	else if(r == 9){
		int MaxDeaths = 0;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxDeaths < p->m_Stats.m_Deaths){
				MaxDeaths = p->m_Stats.m_Deaths;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s was thrown %d time%s into spikes by the opponents, while being frozen.", Server()->ClientName(PlayerID), MaxDeaths, (MaxDeaths == 1 ? "" : "s"));
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		}
	}
	//player that threw most opponents into golden spikes
	else if(r == 10){
		int MaxGold = 0;
		int PlayerID = -1;
		
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			CPlayer* p = m_apPlayers[i];
			if (!p || p->GetTeam() == TEAM_SPECTATORS) continue;
			if(MaxGold < p->m_Stats.m_GrabsGold){
				MaxGold = p->m_Stats.m_GrabsGold;
				PlayerID = i;
			}
		}
		
		if(PlayerID != -1){
			char buff[300];
			str_format(buff, sizeof(buff), "Trivia: %s threw %d time%s frozen opponents into golden spikes.", Server()->ClientName(PlayerID), MaxGold, (MaxGold == 1 ? "" : "s"));
			SendChat(-1, CGameContext::CHAT_ALL, buff);
			TriviaSent = true;
		} else {
			//send another trivia, bcs this is rare on maps without golden spikes
			SendRandomTrivia();			
			TriviaSent = true;
		}
	}
	
	if(!TriviaSent){
		SendChat(-1, CGameContext::CHAT_ALL, "Trivia: Press F1 and use PageUp and PageDown to scroll in the console window");
	}
}

int CGameContext::SendPackMsg(CNetMsg_Sv_KillMsg *pMsg, int Flags)
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		CPlayer* p = m_apPlayers[i];
		if (!p) continue;

		int id = pMsg->m_Killer;
		int id2 = pMsg->m_Victim;

		int originalId = pMsg->m_Killer;
		int originalId2 = pMsg->m_Victim;
		if (!p->IsSnappingClient(pMsg->m_Killer, p->m_ClientVersion, id) || !p->IsSnappingClient(pMsg->m_Victim, p->m_ClientVersion, id2)) continue;
		pMsg->m_Killer = id;
		pMsg->m_Victim = id2;
		Server()->SendPackMsg(pMsg, Flags, i);
		pMsg->m_Killer = originalId;
		pMsg->m_Victim = originalId2;
	}
	return 0;
}

int CGameContext::SendPackMsg(CNetMsg_Sv_Emoticon *pMsg, int Flags)
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		CPlayer* p = m_apPlayers[i];
		if (!p) continue;

		int id = pMsg->m_ClientID;
		int originalID = pMsg->m_ClientID;
		if (!p->IsSnappingClient(pMsg->m_ClientID, p->m_ClientVersion, id)) continue;
		pMsg->m_ClientID = id;
		Server()->SendPackMsg(pMsg, Flags, i);
		pMsg->m_ClientID = originalID;
	}
	return 0;
}

char msgbuf[1000];

int CGameContext::SendPackMsg(CNetMsg_Sv_Chat *pMsg, int Flags)
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		CPlayer* p = m_apPlayers[i];
		if (!p) continue;

		int id = pMsg->m_ClientID;
		int originalID = pMsg->m_ClientID;
		const char* pOriginalText = pMsg->m_pMessage;
		if (id > -1 && id < MAX_CLIENTS && !p->IsSnappingClient(pMsg->m_ClientID, p->m_ClientVersion, id)) {
			str_format(msgbuf, sizeof(msgbuf), "%s: %s", Server()->ClientName(pMsg->m_ClientID), pMsg->m_pMessage);

			pMsg->m_ClientID = (p->m_ClientVersion == CPlayer::CLIENT_VERSION_DDNET) ? CPlayer::DDNET_CLIENT_MAX_CLIENTS - 1 : CPlayer::VANILLA_CLIENT_MAX_CLIENTS - 1;
			pMsg->m_pMessage = msgbuf;
		}
		else pMsg->m_ClientID = id;
		Server()->SendPackMsg(pMsg, Flags, i); 
		pMsg->m_ClientID = originalID;
		pMsg->m_pMessage = pOriginalText;
	}
	return 0;
}


int CGameContext::SendPackMsg(CNetMsg_Sv_Chat *pMsg, int Flags, int ClientID)
{
	CPlayer* p = m_apPlayers[ClientID];
	if (!p)
		return -1;

	int id = pMsg->m_ClientID;

	int originalID = pMsg->m_ClientID;
	const char* pOriginalText = pMsg->m_pMessage;

	bool ClientNotVisible = (id > -1 && id < MAX_CLIENTS && !p->IsSnappingClient(pMsg->m_ClientID, p->m_ClientVersion, id));

	if(ClientNotVisible) {
		pMsg->m_ClientID = (p->m_ClientVersion == CPlayer::CLIENT_VERSION_DDNET) ? CPlayer::DDNET_CLIENT_MAX_CLIENTS - 1 : CPlayer::VANILLA_CLIENT_MAX_CLIENTS - 1;
	}
	else
		pMsg->m_ClientID = id;

	//if not ddnet client, split the msg
	if(p->m_ClientVersion == CPlayer::CLIENT_VERSION_DDNET)
		Server()->SendPackMsg(pMsg, Flags, ClientID);
	else {
		int StrLen = str_length(pMsg->m_pMessage);

		int StrOffset = 0;
		while(StrLen >= (126 - MAX_NAME_LENGTH)) {
			if(ClientNotVisible)
				str_format(msgbuf, sizeof(msgbuf), "%s: %s", Server()->ClientName(originalID), (pMsg->m_pMessage + (intptr_t)StrOffset));
			else
				str_format(msgbuf, sizeof(msgbuf), "%s", (pMsg->m_pMessage + (intptr_t)StrOffset));
			
			msgbuf[126] = 0;
			pMsg->m_pMessage = msgbuf;
			Server()->SendPackMsg(pMsg, Flags, ClientID);

			StrLen -= (126 - MAX_NAME_LENGTH);
			StrOffset += (126 - MAX_NAME_LENGTH);
		}

		if(StrLen > 0) {
			if(ClientNotVisible)
				str_format(msgbuf, sizeof(msgbuf), "%s: %s", Server()->ClientName(originalID), (pMsg->m_pMessage + (intptr_t)StrOffset));
			else
				str_format(msgbuf, sizeof(msgbuf), "%s", (pMsg->m_pMessage + (intptr_t)StrOffset));
			pMsg->m_pMessage = msgbuf;
			Server()->SendPackMsg(pMsg, Flags, ClientID);
		}
	}

	pMsg->m_ClientID = originalID;
	pMsg->m_pMessage = pOriginalText;
	return 0;
}

IGameServer *CreateGameServer() { return new CGameContext; }
