#include "Client.h"
#include "PluginCallbacks.h"
#include "Typedefs.h"
#include <cellstatus.h>
#include <sys/prx.h>
#include <cell/pad.h>
#include <sysutil/sysutil_oskdialog.h>
#include <sysutil/sysutil_oskdialog_ext.h>
#include <sys/socket.h>
#include <netex/net.h>
#include <netex/libnetctl.h>
#include <netinet\in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/timer.h>
#include <sys/ppu_thread.h>
#include "DetourHook.hpp"
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <sys/memory.h>

IVEngineClient* client;
uint64_t enginebase;

#define ioctl( s, cmd, pVal ) setsockopt( s, SOL_SOCKET, cmd, pVal, sizeof( *( pVal ) ) )

SYS_LIB_DECLARE_WITH_STUB( LIBNAME, SYS_LIB_AUTO_EXPORT, STUBNAME );
SYS_LIB_EXPORT( _PortalPlugin_export_function, LIBNAME );

extern "C" int _PortalPlugin_export_function(void)
{
    return CELL_OK;
}

struct PS3_PrxLoadParametersBase_t
{
	int32_t cbSize;
	sys_prx_id_t sysPrxId;
	uint64_t uiFlags;
	uint64_t reserved[7];
};

struct PS3_LoadAppSystemInterface_Parameters_t : public PS3_PrxLoadParametersBase_t
{
	typedef void* (*PFNCREATEINTERFACE)(const char *pName, int *pReturnCode);

	PFNCREATEINTERFACE pfnCreateInterface; // [OUT] [ app system module create interface entry point ]
	uint64_t reserved[8];
};

CEmptyServerPlugin* g_EmptyServerPlugin;

void* CreateInterface(const char *pName, int *pReturnCode)
{
	g_EmptyServerPlugin = new CEmptyServerPlugin();
	return g_EmptyServerPlugin;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEmptyServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, (*g_EmptyServerPlugin));

PS3_PRX_APPSYSTEM_MODULE(PortalPlugin);

CEmptyServerPlugin::CEmptyServerPlugin()
{

}

CEmptyServerPlugin::~CEmptyServerPlugin()
{
}

unsigned short NET_HostToNetShort(unsigned short us_in)
{
	return htons(us_in);
}


bool NET_StringToSockaddr(const char *s, struct sockaddr *sadr)
{
	char	*colon;
	char	copy[128];
	memset(sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	strncpy(copy, s, sizeof(copy));
	// strip off a trailing :port if present
	for (colon = copy; *colon; colon++)
	{
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = NET_HostToNetShort((short)atoi(colon + 1));
		}
	}

	if (copy[0] >= '0' && copy[0] <= '9' && strstr(copy, "."))
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{

		struct hostent	*h;
		if ((h = gethostbyname(copy)) == NULL)
			return false;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}

	return true;
}

int NET_OpenSocket(const char *net_interface, int& port, int protocol)
{
	int	newsocket = -1;
	struct sockaddr_in address;
	if (protocol == IPPROTO_TCP)
	{
		newsocket = socket(AF_INET, SOCK_STREAM, 6);
	}
	else
	{
		newsocket = socket(AF_INET, SOCK_DGRAM, protocol);
	}
	unsigned int		opt = 1;
	int ret = ioctl(newsocket, SO_NBIO, (unsigned long*)&opt);
	if (protocol == IPPROTO_TCP)
	{
		return newsocket;
	}

	ret = setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt));
	if (!net_interface || !net_interface[0] || !strcmp(net_interface, "localhost"))
	{
		address.sin_addr.s_addr = INADDR_ANY;
	}
	else
	{
		NET_StringToSockaddr(net_interface, (struct sockaddr *)&address);
	}

	address.sin_port = NET_HostToNetShort((short)(port));
	ret = bind(newsocket, (struct sockaddr *)&address, sizeof(address));

	address.sin_family = AF_INET;
	return newsocket;
}

typedef enum
{
	NA_NULL = 0,
	NA_LOOPBACK,
	NA_IP,
	NA_BROADCAST,
} netadrtype_t;

typedef struct netadr_s{
public:
	bool	SetFromSockadr(const struct sockaddr *s);
	unsigned short	type;
	unsigned short	port;
	unsigned char	ip[4];
} netadr_t;


bool netadr_t::SetFromSockadr(const struct sockaddr * s)
{
	type = NA_IP;
	*(int *)&ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
	port = ((struct sockaddr_in *)s)->sin_port;
	return true;
}

struct ns_address
{
	netadr_s m_adr;
	uint64_t m_steamID;
	uint32_t m_AddrType;

	bool SetFromSockadr(const struct sockaddr * s)
	{
		m_AddrType = 0;
		m_steamID = 0;
		return m_adr.SetFromSockadr(s);
	}
};

DetourHook* OpenSocketInternalHk;

static void OpenSocketInternal(int nModule, int nSetPort, int nDefaultPort, const char *pName, int nProtocol, bool bTryAny)
{
	//char dof[128];
	//unsigned int writelen;
	//sys_tty_write(0, dof, snprintf(dof, 128, "%i %i %i %s %i\n", nModule, nSetPort, nDefaultPort, pName, nProtocol), &writelen);
	int iVar8 = nDefaultPort;
	if (nSetPort != 0) {
		iVar8 = nSetPort;
	}
	int* piVar7;
	int iVar2 = nModule * 0x4;
	int* net_sockets = *(int**)((*(byte*)(enginebase + 0x01ffe73)) * 0x10000 - 0x59e0);
	if (nProtocol == 6)
	{
		piVar7 = (int*)(net_sockets + iVar2 + 0xc);
	}
	else
	{
		piVar7 = (int*)(net_sockets + iVar2 + 8);
	}
	*piVar7 = NET_OpenSocket("127.0.0.1", iVar8, nProtocol);
	*(int*)(net_sockets + iVar2) = iVar8;
	//sys_tty_write(0, dof, snprintf(dof, 128, "addr: %x\n", net_sockets + iVar2), &writelen);
	//(**(LOL**)(*(int *)(enginebase+0x0055f84c) + 0xc))
	//	((enginebase + 0x0055f84c), *piVar7, nModule, nSetPort, nDefaultPort, pName,
	//	nProtocol, bTryAny);
}
DetourHook* CSteamSocketMgrSendtoHk;

int funnysocket[4] = { 0 };

int CSteamSocketMgrSendto(unsigned long long thisptr, int s, const char * buf, int len, int flags, const ns_address &to)
{
	struct sockaddr_in c;
	memset(&c, 0, sizeof(sockaddr_in));
	c.sin_family = AF_INET;
	//c.sin_port = to.m_adr.port;
	c.sin_port = 27006;
	c.sin_addr = *(in_addr *)&to.m_adr.ip;
	//c.sin_addr = ()0xc0a8011e;
	char dof[128];
	unsigned int writelen;
	//sys_tty_write(0, dof, snprintf(dof, 128, "%i %i %x\n", s, len, flags), &writelen);
	//int ret = sendto(s, (void*)buf, len, flags, (sockaddr*)&c, sizeof(c));
	if (funnysocket[s] == 0)
	{
		funnysocket[s] = socket(AF_INET, SOCK_DGRAM, 17);
		bind(funnysocket[s], (struct sockaddr *)&c, sizeof(c));
		unsigned int opt = 1;
		ioctl(funnysocket[s], SO_NBIO, (unsigned long*)&opt);
	}
	c.sin_port = 27015;
	return sendto(funnysocket[s], (void*)buf, len, flags, (sockaddr*)&c, sizeof(c));
}

DetourHook* CSteamSocketMgrRecvfromHk;
int CSteamSocketMgrRecvfrom(unsigned long long thisptr, int s, char * buf, int len, int flags, ns_address *from, ns_address *otherfrom)
{
	char dof[128];
	unsigned int writelen;
	//sys_tty_write(0, dof, snprintf(dof, 128, "recv: %i %i %x\n", s, len, flags), &writelen);
	sockaddr sadrfrom;
	socklen_t fromlen = sizeof(sadrfrom);
	int iret = recvfrom(funnysocket[s], buf, len, flags, &sadrfrom, &fromlen);
	if (iret > 0)
	{
		//sys_tty_write(0, dof, snprintf(dof, 128, "from: %x | otherfrom: %x\n", from, otherfrom), &writelen);
		from->SetFromSockadr(&sadrfrom);
		return iret;
	}
	return 0;
}

DetourHook* Net_SendStreamHk;
int NET_SendStream(int nSock, const char * buf, int len, int flags)
{
	return send(nSock, buf, len, flags);
}


DetourHook* NET_CloseSocketHk;
void NET_CloseSocket(int hSocket, int sock)
{
	int ret = close(hSocket);
}


int getkbLen(char* str)
{
	int nullCount = 0;
	int i = 0; //num of chars..
	for (i = 0; i < 64; i++)
	{
		if (nullCount == 2) { break; }
		if (*(str + i) == 0x00) { nullCount++; }
		else { nullCount = 0; }
	}
	return i;
}
void makekbStr(char* str, char* dest, int len)
{
	int nulls = 0;
	for (int i = 0; i < len; i++)
	{
		if (*(str + i) == 0x00) { nulls++; }
		else { *(dest + i - nulls) = *(str + i); }
	}
	*(dest + len + 1 - nulls) = 0x00;  //make sure its nulled...
}
bool pressedtriangle = false;
bool keyboardon = false;

void sysutil_callback(uint64_t status, uint64_t param, void *userdata)
{
	unsigned int writelen;
	if (status != CELL_SYSUTIL_OSKDIALOG_FINISHED)
	{
		if (status == CELL_SYSUTIL_OSKDIALOG_UNLOADED)
		{
			keyboardon = false;
		}
		return;
	}
	CellOskDialogCallbackReturnParam OutputInfo;
	OutputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
	OutputInfo.numCharsResultString = 32;
	uint16_t Result_Text_Buffer[CELL_OSKDIALOG_STRING_SIZE + 1];
	OutputInfo.pResultString = Result_Text_Buffer;
	cellOskDialogUnloadAsync(&OutputInfo);
	int len = getkbLen((char*)(Result_Text_Buffer));
	char command[CELL_OSKDIALOG_STRING_SIZE + 1] = { 0 };
	makekbStr((char*)(Result_Text_Buffer), command, len);
	client->ClientCmd(command);
	keyboardon = false;
}

void ConsoleKeyboardInputThread(uint64_t IDK)
{
	for (;;)
	{
		sys_timer_usleep(90000);
		if (!keyboardon)
		{
			CellPadData data;
			cellPadGetData(0, &data);
			if (!pressedtriangle && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE))
			{
				int ret;
				CellOskDialogInputFieldInfo inputFieldInfo;
				inputFieldInfo.message = (uint16_t*)"\x00e\x00n\x00t\x00e\x00r\x00 \x00c\x00o\x00m\x00m\x00a\x00n\x00d\x00\x00";
				inputFieldInfo.init_text = (uint16_t*)"";
				inputFieldInfo.limit_length = CELL_OSKDIALOG_STRING_SIZE;

				ret = cellOskDialogSetKeyLayoutOption(CELL_OSKDIALOG_10KEY_PANEL | CELL_OSKDIALOG_FULLKEY_PANEL);


				CellOskDialogPoint pos;
				pos.x = 0.0;  pos.y = 0.0;
				int32_t LayoutMode = CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER | CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_TOP;
				ret = cellOskDialogSetLayoutMode(LayoutMode);


				CellOskDialogParam dialogParam;
				dialogParam.allowOskPanelFlg = CELL_OSKDIALOG_PANELMODE_ALPHABET |
					CELL_OSKDIALOG_PANELMODE_NUMERAL |
					CELL_OSKDIALOG_PANELMODE_ENGLISH;
				//E Panel to display first 
				dialogParam.firstViewPanel = CELL_OSKDIALOG_PANELMODE_ALPHABET;
				// E Initial display position of the on-screen keyboard dialog
				dialogParam.controlPoint = pos;
				//E Prohibited operation flag(s) (ex. CELL_OSKDIALOG_NO_SPACE)
				dialogParam.prohibitFlgs = 0;
				cellSysutilRegisterCallback(0, sysutil_callback, NULL);
				cellOskDialogLoadAsync(SYS_MEMORY_CONTAINER_ID_INVALID, &dialogParam, &inputFieldInfo);
				pressedtriangle = true;
				keyboardon = true;

			}
			else if (pressedtriangle && !(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE))
			{
				pressedtriangle = false;
			}
		}
	}
}

bool CEmptyServerPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	client = (IVEngineClient*)interfaceFactory("VEngineClient013", NULL);

	enginebase = 0;
	sys_prx_id_t enginesprx = sys_prx_get_module_id_by_name("engine_rel", 0, 0);
	if (enginesprx < 0)
	{
		//cellMsgDialogOpen2(ok_message, "RPCS3 does not support sys_prx_get_module_id_by_name which means that\nyou wont get built-in source engine console, sorry!\n(cry about it to rpcs3 devs)", closethedialog, NULL, NULL);
		enginebase = 0x670000;
	}
	else
	{
		sys_prx_module_info_t info{};
		static sys_prx_segment_info_t segments[10]{};
		static char filename[SYS_PRX_MODULE_FILENAME_SIZE]{};

		memset(segments, 0, sizeof(segments));
		memset(filename, 0, sizeof(filename));

		info.size = sizeof(info);
		info.segments = segments;
		info.segments_num = sizeof(segments) / sizeof(sys_prx_segment_info_t);
		info.filename = filename;
		info.filename_size = sizeof(filename);
		sys_prx_get_module_info(enginesprx, 0, &info);
		enginebase = info.segments->base;
	}
	if (enginebase)
	{
		OpenSocketInternalHk = new DetourHook(enginebase + 0x001febcc, (uintptr_t)OpenSocketInternal);
		CSteamSocketMgrSendtoHk = new DetourHook(enginebase + 0x002051c8, (uintptr_t)CSteamSocketMgrSendto);
		CSteamSocketMgrRecvfromHk = new DetourHook(enginebase + 0x0020555c, (uintptr_t)CSteamSocketMgrRecvfrom);
		Net_SendStreamHk = new DetourHook(enginebase + 0x001fa790, (uintptr_t)NET_SendStream);
		NET_CloseSocketHk = new DetourHook(enginebase + 0x001fa538, (uintptr_t)NET_CloseSocket);
	}
	sys_ppu_thread_t keyboardppulmao;
	sys_ppu_thread_create(&keyboardppulmao, ConsoleKeyboardInputThread, 0, 420, 0x5000, SYS_PPU_THREAD_CREATE_JOINABLE, "If_you_see_this_KEEP_YOURSELF_SAFE");
	return true;
}

void CEmptyServerPlugin::Unload(void)
{

}

void CEmptyServerPlugin::Pause(void)
{

}

void CEmptyServerPlugin::UnPause(void)
{

}

const char *CEmptyServerPlugin::GetPluginDescription(void)
{
	return "Epic plugin for Portal 2";
}

void CEmptyServerPlugin::LevelInit(char const *pMapName)
{
	
}

void CEmptyServerPlugin::ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{

}

void CEmptyServerPlugin::GameFrame(bool simulating)
{

}

void CEmptyServerPlugin::LevelShutdown(void) // !!!!this can get called multiple times per map change
{
	
}

void CEmptyServerPlugin::ClientActive(edict_t *pEntity)
{
	
}

void CEmptyServerPlugin::ClientDisconnect(edict_t *pEntity)
{
	
}

void CEmptyServerPlugin::ClientPutInServer(edict_t *pEntity, char const *playername)
{

}

void CEmptyServerPlugin::SetCommandClient(int index)
{

}

void CEmptyServerPlugin::ClientSettingsChanged(edict_t *pEdict)
{

}

PLUGIN_RESULT CEmptyServerPlugin::ClientConnect(bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen)
{
	return PLUGIN_CONTINUE;
}

PLUGIN_RESULT CEmptyServerPlugin::ClientCommand(edict_t *pEntity, const void* &args)
{
	return PLUGIN_CONTINUE;
}

PLUGIN_RESULT CEmptyServerPlugin::NetworkIDValidated(const char *pszUserName, const char *pszNetworkID)
{
	return PLUGIN_CONTINUE;
}

void CEmptyServerPlugin::OnQueryCvarValueFinished(void* iCookie, edict_t *pPlayerEntity, void* eStatus, const char *pCvarName, const char *pCvarValue)
{

}

void CEmptyServerPlugin::OnEdictAllocated(edict_t *edict)
{

}

void CEmptyServerPlugin::OnEdictFreed(const edict_t *edict)
{

}

void CEmptyServerPlugin::FireGameEvent(KeyValues * event)
{

}
