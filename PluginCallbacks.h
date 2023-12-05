#ifndef PluginCallbacks
#define PluginCallbacks

#include "Typedefs.h"

; class InterfaceReg
{
public:
	InterfaceReg(InstantiateInterfaceFn fn, const char *pName);

public:
	InstantiateInterfaceFn	m_CreateFn;
	const char				*m_pName;

	InterfaceReg			*m_pNext; // For the global list.
	static InterfaceReg		*s_pInterfaceRegs;
};

InterfaceReg *InterfaceReg::s_pInterfaceRegs = 0;

InterfaceReg::InterfaceReg(InstantiateInterfaceFn fn, const char *pName) :
m_pName(pName)
{
	m_CreateFn = fn;
	m_pNext = s_pInterfaceRegs;
	s_pInterfaceRegs = this;
}

class IGameEventListener
{
public:
	virtual	~IGameEventListener(void) {};

	virtual void FireGameEvent(KeyValues * event) = 0;
};

typedef enum
{
	PLUGIN_CONTINUE = 0, // keep going
	PLUGIN_OVERRIDE, // run the game dll function but use our return value instead
	PLUGIN_STOP, // don't run the game dll function at all
} PLUGIN_RESULT;

class IServerPluginCallbacks
{
public:
	virtual bool			Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) = 0;

	virtual void			Unload(void) = 0;

	virtual void			Pause(void) = 0;

	virtual void			UnPause(void) = 0;

	virtual const char     *GetPluginDescription(void) = 0;

	virtual void			LevelInit(char const *pMapName) = 0;

	virtual void			ServerActivate(edict_t *pEdictList, int edictCount, int clientMax) = 0;

	virtual void			GameFrame(bool simulating) = 0;

	virtual void			LevelShutdown(void) = 0;

	virtual void			ClientActive(edict_t *pEntity) = 0;

	virtual void			ClientDisconnect(edict_t *pEntity) = 0;

	virtual void			ClientPutInServer(edict_t *pEntity, char const *playername) = 0;

	virtual void			SetCommandClient(int index) = 0;

	virtual void			ClientSettingsChanged(edict_t *pEdict) = 0;

	virtual PLUGIN_RESULT	ClientConnect(bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen) = 0;

	virtual PLUGIN_RESULT	ClientCommand(edict_t *pEntity, const void* &args) = 0;

	virtual PLUGIN_RESULT	NetworkIDValidated(const char *pszUserName, const char *pszNetworkID) = 0;

	virtual void			OnQueryCvarValueFinished(void* iCookie, edict_t *pPlayerEntity, void* eStatus, const char *pCvarName, const char *pCvarValue) = 0;

	virtual void			OnEdictAllocated(edict_t *edict) = 0;
	virtual void			OnEdictFreed(const edict_t *edict) = 0;
};


typedef void* KeyValues;



class CEmptyServerPlugin : public IServerPluginCallbacks, public IGameEventListener
{
public:
	CEmptyServerPlugin();
	~CEmptyServerPlugin();

	virtual bool			Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory);
	virtual void			Unload(void);
	virtual void			Pause(void);
	virtual void			UnPause(void);
	virtual const char     *GetPluginDescription(void);
	virtual void			LevelInit(char const *pMapName);
	virtual void			ServerActivate(edict_t *pEdictList, int edictCount, int clientMax);
	virtual void			GameFrame(bool simulating);
	virtual void			LevelShutdown(void);
	virtual void			ClientActive(edict_t *pEntity);
	virtual void			ClientDisconnect(edict_t *pEntity);
	virtual void			ClientPutInServer(edict_t *pEntity, char const *playername);
	virtual void			SetCommandClient(int index);
	virtual void			ClientSettingsChanged(edict_t *pEdict);
	virtual PLUGIN_RESULT	ClientConnect(bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen);
	virtual PLUGIN_RESULT	ClientCommand(edict_t *pEntity, const void* &args);
	virtual PLUGIN_RESULT	NetworkIDValidated(const char *pszUserName, const char *pszNetworkID);
	virtual void			OnQueryCvarValueFinished(void* iCookie, edict_t *pPlayerEntity, void* eStatus, const char *pCvarName, const char *pCvarValue);
	virtual void			OnEdictAllocated(edict_t *edict);
	virtual void			OnEdictFreed(const edict_t *edict);

	virtual void FireGameEvent(KeyValues * event);
};


#define EXPOSE_SINGLE_INTERFACE_GLOBALVAR_WITH_NAMESPACE(className, interfaceNamespace, interfaceName, versionName, globalVarName) \
	static void* __Create##className##interfaceName##_interface() { return static_cast<interfaceNamespace interfaceName *>(&globalVarName); } \
	static InterfaceReg __g_Create##className##interfaceName##_reg(__Create##className##interfaceName##_interface, versionName);

#define EXPOSE_SINGLE_INTERFACE_GLOBALVAR(className, interfaceName, versionName, globalVarName) \
	EXPOSE_SINGLE_INTERFACE_GLOBALVAR_WITH_NAMESPACE(className, , interfaceName, versionName, globalVarName)

#define INTERFACEVERSION_ISERVERPLUGINCALLBACKS				"ISERVERPLUGINCALLBACKS003"

#define PS3CLVERMAJOR 0
#define PS3CLVERMINOR 1

#ifndef PS3CLVERMAJOR
#define PS3CLVERMAJOR ((APPCHANGELISTVERSION / 256) % 256)
#define PS3CLVERMINOR  (APPCHANGELISTVERSION % 256)
#endif

#define PS3_PRX_SYS_MODULE_INFO_FULLMACROREPLACEMENTHELPER(ps3modulename) SYS_MODULE_INFO( ps3modulename##_rel, 0, PS3CLVERMAJOR, PS3CLVERMINOR)

#define PS3_PRX_SYS_MODULE_START_NAME_FULLMACROREPLACEMENTHELPER(ps3modulename)  _##ps3modulename##_ps3_prx_entry

#define PS3_PRX_APPSYSTEM_MODULE( ps3modulename ) \
	\
	PS3_PRX_SYS_MODULE_INFO_FULLMACROREPLACEMENTHELPER(ps3modulename); \
	SYS_MODULE_START(PS3_PRX_SYS_MODULE_START_NAME_FULLMACROREPLACEMENTHELPER(ps3modulename)); \
	\
	extern "C" int PS3_PRX_SYS_MODULE_START_NAME_FULLMACROREPLACEMENTHELPER(ps3modulename)(unsigned int args, void *pArg) \
{ \
	PS3_LoadAppSystemInterface_Parameters_t *pParams = reinterpret_cast< PS3_LoadAppSystemInterface_Parameters_t * >(pArg); \
	pParams->pfnCreateInterface = CreateInterface; \
	return SYS_PRX_RESIDENT; \
} \

#endif