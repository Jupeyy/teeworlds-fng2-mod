#ifndef GAMESERVER_CONFIG
#define GAMESERVER_CONFIG

#include <engine/console.h>
#include <engine/shared/config.h>

class CGameServerConfig
{
	public:
	
	CConfiguration m_Config;
	
	class CConfigCommand *m_pFirstCommand;

	class IStorage *m_pStorage;
	
	int ParseArgs(class CResult *pResult);

	void AddCommandSorted(CConfigCommand *pCommand);
	CConfigCommand *FindCommand(const char *pName, int FlagMask);

public:
	CGameServerConfig();

	virtual int ParseStart(CResult *pResult, const char *pString, int Length);
	virtual void Register(const char *pName, int Flags, IConsole::FCommandCallback pfnFunc, void *pUser);
	virtual void ExecuteLine(const char *pStr);
	virtual void ExecuteFile(const char *pFilename, class IKernel* pKernel);
	
	void ToConfigObject(struct CConfiguration*& pConfig);
};

#endif
