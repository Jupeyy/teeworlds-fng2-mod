/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>

#include <base/math.h>
#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/shared/protocol.h>
#include <engine/storage.h>

#include "gameserver_config.h"

#include <cstring>
#include <iostream>

#pragma pack(push, 1)
struct CIntVarData
{
	int *m_pVariable;
	int m_Min;
	int m_Max;
};

struct CStrVarData
{
	char *m_pStr;
	int m_MaxSize;
	int reserved;
};

union CStrIntVarData
{
	CIntVarData data;
	CStrVarData datastr;
};
#pragma pack(pop)

enum
{
	CONSOLE_MAX_STR_LENGTH = 1024,
	MAX_PARTS = (CONSOLE_MAX_STR_LENGTH + 1) / 2
};

class CConfigCommand
{
public:
	CConfigCommand *m_pNext;
	int m_Flags;
	const char *m_pName;
	IConsole::FCommandCallback m_pfnCallback;
	CStrIntVarData m_pUserData;
};

class CResult : public IConsole::IResult
{
public:
	char m_aStringStorage[CONSOLE_MAX_STR_LENGTH + 1];
	char *m_pArgsStart;

	const char *m_pCommand;
	const char *m_apArgs[MAX_PARTS];

	CResult() : IConsole::IResult()
	{
		mem_zero(m_aStringStorage, sizeof(m_aStringStorage));
		m_pArgsStart = 0;
		m_pCommand = 0;
		mem_zero(m_apArgs, sizeof(m_apArgs));
	}

	CResult &operator=(const CResult &Other)
	{
		if(this != &Other)
		{
			IConsole::IResult::operator=(Other);
			mem_copy(m_aStringStorage, Other.m_aStringStorage, sizeof(m_aStringStorage));
			m_pArgsStart = m_aStringStorage + (Other.m_pArgsStart - Other.m_aStringStorage);
			m_pCommand = m_aStringStorage + (Other.m_pCommand - Other.m_aStringStorage);
			for(unsigned i = 0; i < Other.m_NumArgs; ++i)
				m_apArgs[i] = m_aStringStorage + (Other.m_apArgs[i] - Other.m_aStringStorage);
		}
		return *this;
	}

	void AddArgument(const char *pArg) { m_apArgs[m_NumArgs++] = pArg; }

	virtual const char *GetString(unsigned Index);
	virtual int GetInteger(unsigned Index);
	virtual float GetFloat(unsigned Index);
};

const char *CResult::GetString(unsigned Index)
{
	if(Index >= m_NumArgs)
		return "";
	return m_apArgs[Index];
}

int CResult::GetInteger(unsigned Index)
{
	if(Index >= m_NumArgs)
		return 0;
	return str_toint(m_apArgs[Index]);
}

float CResult::GetFloat(unsigned Index)
{
	if(Index >= m_NumArgs)
		return 0.0f;
	return str_tofloat(m_apArgs[Index]);
}

void CGameServerConfig::ToConfigObject(struct CConfiguration *&pConfig) { pConfig = &m_Config; }

int CGameServerConfig::ParseStart(CResult *pResult, const char *pString, int Length)
{
	char *pStr;
	int Len = sizeof(pResult->m_aStringStorage);
	if(Length < Len)
		Len = Length;

	str_copy(pResult->m_aStringStorage, pString, Len);
	pStr = pResult->m_aStringStorage;

	// get command
	pStr = str_skip_whitespaces(pStr);
	pResult->m_pCommand = pStr;
	pStr = str_skip_to_whitespace(pStr);

	if(*pStr)
	{
		pStr[0] = 0;
		pStr++;
	}

	pResult->m_pArgsStart = pStr;
	return 0;
}

int CGameServerConfig::ParseArgs(CResult *pResult)
{
	char *pStr;
	int Optional = 1;
	int Error = 0;

	pStr = pResult->m_pArgsStart;

	while(1)
	{
		pStr = str_skip_whitespaces(pStr);

		if(!(*pStr)) // error, non optional command needs value
		{
			if(!Optional)
				Error = 1;
			break;
		}

		// add token
		if(*pStr == '"')
		{
			char *pDst;
			pStr++;
			pResult->AddArgument(pStr);

			pDst = pStr; // we might have to process escape data
			while(1)
			{
				if(pStr[0] == '"')
					break;
				else if(pStr[0] == '\\')
				{
					if(pStr[1] == '\\')
						pStr++; // skip due to escape
					else if(pStr[1] == '"')
						pStr++; // skip due to escape
				}
				else if(pStr[0] == 0)
					return 1; // return error

				*pDst = *pStr;
				pDst++;
				pStr++;
			}

			// write null termination
			*pDst = 0;

			pStr++;
		}
		else
		{
			pResult->AddArgument(pStr);
			pStr++;
		}
	}

	return Error;
}

CConfigCommand *CGameServerConfig::FindCommand(const char *pName, int FlagMask)
{
	for(CConfigCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & FlagMask)
		{
			if(str_comp_nocase(pCommand->m_pName, pName) == 0)
				return pCommand;
		}
	}

	return 0x0;
}

void CGameServerConfig::ExecuteLine(const char *pStr)
{
	while(pStr && *pStr)
	{
		CResult Result;
		const char *pEnd = pStr;
		const char *pNextPart = 0;
		int InString = 0;

		while(*pEnd)
		{
			if(*pEnd == '"')
				InString ^= 1;
			else if(*pEnd == '\\') // escape sequences
			{
				if(pEnd[1] == '"')
					pEnd++;
			}
			else if(!InString)
			{
				if(*pEnd == ';') // command separator
				{
					pNextPart = pEnd + 1;
					break;
				}
				else if(*pEnd == '#') // comment, no need to do anything more
					break;
			}

			pEnd++;
		}

		if(ParseStart(&Result, pStr, (pEnd - pStr) + 1) != 0)
			return;

		if(!*Result.m_pCommand)
			return;

		CConfigCommand *pCommand = FindCommand(Result.m_pCommand, CFGFLAG_SERVER);

		if(pCommand)
		{
			if(ParseArgs(&Result))
			{
				return;
			}
			else
				pCommand->m_pfnCallback((IConsole::IResult *)&Result, &pCommand->m_pUserData);
		}
		else
		{
			return;
		}

		pStr = pNextPart;
	}
}

void CGameServerConfig::ExecuteFile(const char *pFilename, IKernel *pKernel)
{
	if(!m_pStorage)
		m_pStorage = pKernel->RequestInterface<IStorage>();
	if(!m_pStorage)
		return;

	// exec the file
	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[256];
	if(File)
	{
		char *pLine;
		CLineReader lr;

		lr.Init(File);

		while((pLine = lr.Get()))
			ExecuteLine(pLine);

		io_close(File);
	}
	else
	{
	}
}

static void IntVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CIntVarData *pData = (CIntVarData *)pUserData;

	if(pResult->NumArguments())
	{
		int Val = pResult->GetInteger(0);

		// do clamping
		if(pData->m_Min != pData->m_Max)
		{
			if(Val < pData->m_Min)
				Val = pData->m_Min;
			if(pData->m_Max != 0 && Val > pData->m_Max)
				Val = pData->m_Max;
		}

		*(pData->m_pVariable) = Val;
	}
	else
	{
		return;
	}
}

static void StrVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CStrVarData *pData = (CStrVarData *)pUserData;

	if(pResult->NumArguments())
	{
		const char *pString = pResult->GetString(0);
		if(!str_utf8_check(pString))
		{
			char Temp[4];
			int Length = 0;
			while(*pString)
			{
				int Size = str_utf8_encode(Temp, static_cast<const unsigned char>(*pString++));
				if(Length + Size < pData->m_MaxSize)
				{
					mem_copy(pData->m_pStr + Length, &Temp, Size);
					Length += Size;
				}
				else
					break;
			}
			pData->m_pStr[Length] = 0;
		}
		else
			str_copy(pData->m_pStr, pString, pData->m_MaxSize);
	}
	else
	{
		return;
	}
}

CGameServerConfig::CGameServerConfig()
{
	m_pFirstCommand = NULL;
	m_pStorage = NULL;

#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	{ \
		CIntVarData Data = {&m_Config.m_##Name, Min, Max}; \
		m_Config.m_##Name = Def; \
		Register(#ScriptName, Flags, IntVariableCommand, &Data); \
	}

#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	{ \
		CStrVarData Data = {m_Config.m_##Name, Len}; \
		str_copy(m_Config.m_##Name, Def, Len); \
		Register(#ScriptName, Flags, StrVariableCommand, &Data); \
	}

#include <engine/shared/config_variables.h>

#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_STR
}

void CGameServerConfig::AddCommandSorted(CConfigCommand *pCommand)
{
	if(!m_pFirstCommand || str_comp(pCommand->m_pName, m_pFirstCommand->m_pName) <= 0)
	{
		if(m_pFirstCommand && m_pFirstCommand->m_pNext)
			pCommand->m_pNext = m_pFirstCommand;
		else
			pCommand->m_pNext = 0;
		m_pFirstCommand = pCommand;
	}
	else
	{
		for(CConfigCommand *p = m_pFirstCommand; p; p = p->m_pNext)
		{
			if(!p->m_pNext || str_comp(pCommand->m_pName, p->m_pNext->m_pName) <= 0)
			{
				pCommand->m_pNext = p->m_pNext;
				p->m_pNext = pCommand;
				break;
			}
		}
	}
}

void CGameServerConfig::Register(const char *pName, int Flags, IConsole::FCommandCallback pfnFunc, void *pUser)
{
	CConfigCommand *pCommand = FindCommand(pName, Flags);
	bool DoAdd = false;
	if(pCommand == 0)
	{
		pCommand = new(mem_alloc(sizeof(CConfigCommand), sizeof(void *))) CConfigCommand;
		DoAdd = true;
	}
	pCommand->m_pfnCallback = pfnFunc;
	pCommand->m_pUserData = *((CStrIntVarData *)pUser);

	pCommand->m_pName = pName;

	pCommand->m_Flags = Flags;

	if(DoAdd)
		AddCommandSorted(pCommand);
}