/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H
#include "kernel.h"
#include "message.h"

#define GAME_ID_INVALID 0xFFFFFFFF

struct sGame{
	class IGameServer *m_pGameServer;
	unsigned int m_uiGameID;
	sGame* m_pNext;
	
	sGame() : m_pGameServer(0), m_uiGameID(GAME_ID_INVALID), m_pNext(0){
		
	}
	class IGameServer *GameServer() { return m_pGameServer; }
};

struct sMap {
	char m_aCurrentMap[64];
	unsigned m_CurrentMapCrc;
	unsigned char *m_pCurrentMapData;
	int m_CurrentMapSize;
	unsigned int m_uiGameID;

	class IMap* m_pMap;
	
	sMap* m_pNextMap;

	sMap() : m_pCurrentMapData(0), m_pMap(0), m_pNextMap(0) {
	}
	
	~sMap();
};

class IServer : public IInterface
{
	MACRO_INTERFACE("server", 0)
protected:
	int m_CurrentGameTick;
	int m_TickSpeed;

public:
	/*
		Structure: CClientInfo
	*/
	struct CClientInfo
	{
		const char *m_pName;
		int m_Latency;
	};

	int Tick() const { return m_CurrentGameTick; }
	int TickSpeed() const { return m_TickSpeed; }

	virtual int MaxClients() const = 0;
	virtual const char *ClientName(int ClientID, bool ForceGet = false) = 0;
	virtual const char *ClientClan(int ClientID, bool ForceGet = false) = 0;
	virtual int ClientCountry(int ClientID, bool ForceGet = false) = 0;
	virtual bool ClientIngame(int ClientID) = 0;
	virtual int GetClientInfo(int ClientID, CClientInfo *pInfo) = 0;
	virtual void GetClientAddr(int ClientID, char *pAddrStr, int Size) = 0;
		
	virtual int BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason, bool Force = true) = 0;
	virtual void GetNetAddr(NETADDR *pAddr, int ClientID) = 0;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID) = 0;

	template<class T>
	int SendPackMsg(T *pMsg, int Flags, int ClientID)
	{
		CMsgPacker Packer(pMsg->MsgID());
		if (pMsg->Pack(&Packer))
			return -1;
		return SendMsg(&Packer, Flags, ClientID);
	}

	virtual void SetClientName(int ClientID, char const *pName) = 0;
	virtual void SetClientClan(int ClientID, char const *pClan) = 0;
	virtual void SetClientCountry(int ClientID, int Country) = 0;
	virtual void SetClientScore(int ClientID, int Score) = 0;
	virtual void SetClientVersion(int ClientID, int Version) = 0;
	virtual void SetClientUnknownFlags(int ClientID, int UnknownFlags) = 0;

	virtual int SnapNewID() = 0;
	virtual void SnapFreeID(int ID) = 0;
	virtual void *SnapNewItem(int Type, int ID, int Size) = 0;

	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	enum
	{
		RCON_CID_SERV=-1,
		RCON_CID_VOTE=-2,
	};
	virtual void SetRconCID(int ClientID) = 0;
	virtual bool IsAuthed(int ClientID) = 0;
	virtual void Kick(int ClientID, const char *pReason) = 0;

	virtual void DemoRecorder_HandleAutoStart() = 0;
	virtual bool DemoRecorder_IsRecording() = 0;
	
	virtual int StartGameServer(const char* pMap, struct CConfiguration* pConfig = 0) = 0;
	virtual void StopGameServer(unsigned int GameID, int MoveToGameID = -1) = 0;
	virtual bool ChangeGameServerMap(unsigned int GameID, const char* pMapName) = 0;
	virtual void MovePlayerToGameServer(int PlayerID, unsigned int GameID) = 0;
	//if wanted or needed, players that are still connecting (loading the map or smth.) can be kicked all at once
	virtual void KickConnectingPlayers(unsigned int GameID, const char* pReason) = 0;
	//we can check, if wanted, if there are players connecting.. e.g. to wait at mapchange
	virtual bool CheckForConnectingPlayers(unsigned int GameID) = 0;
	
	virtual struct sGame* GetGame(unsigned int GameID) = 0;

};

class IGameServer : public IInterface
{
	MACRO_INTERFACE("gameserver", 0)
protected:
public:
	struct CConfiguration* m_Config;
	
	virtual void OnInit() = 0;
	virtual void OnInit(class IKernel* pKernel, class IMap* pMap, struct CConfiguration* pConfigFile = 0) = 0;
	virtual void OnConsoleInit() = 0;
	virtual void OnShutdown() = 0;

	virtual int PreferedTeamPlayer(int ClientID) = 0;

	virtual void OnTick() = 0;
	virtual void OnPreSnap() = 0;
	virtual void OnSnap(int ClientID) = 0;
	virtual void OnPostSnap() = 0;

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) = 0;

	virtual void OnClientConnected(int ClientID, int PreferedTeam = -2) = 0;
	virtual void OnClientEnter(int ClientID) = 0;
	virtual bool OnClientDrop(int ClientID, const char *pReason, bool Force) = 0;
	virtual void OnClientDirectInput(int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientID, void *pInput) = 0;

	virtual bool IsClientReady(int ClientID) = 0;
	virtual bool IsClientPlayer(int ClientID) = 0;

	virtual const char *GameType() = 0;
	virtual const char *Version() = 0;
	virtual const char *NetVersion() = 0;
};

extern IGameServer *CreateGameServer();
#endif
