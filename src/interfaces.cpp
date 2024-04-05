#include "interfaces.h"
#include "util.h"

#include "clientstate.h"

#include <Windows.h>
#include <bit>

using CreateInterfaceFn = void* (__cdecl*)(const char*, int*);
namespace interfaces {
	enum IFaceLocation {
		CLIENT,
		ENGINE,
		LUASHARED,
		VGUI,
		VStdLib,
		Surface
	};
	 
	CreateInterfaceFn ClientCreateInterface		= nullptr;
	CreateInterfaceFn EngineCreateInterface		= nullptr;
	CreateInterfaceFn SurfaceCreateInterface	= nullptr;
	CreateInterfaceFn LuaSharedCreateInterface	= nullptr;
	CreateInterfaceFn VGUICreateInterface		= nullptr;
	CreateInterfaceFn VStdLibCreateInterface	= nullptr;

	bool hadError = false;

	template <IFaceLocation location, typename T>
	static void CreateInterface(const char* name, T*& outIFace) {
		int status;

		void* iface;
		if constexpr (location == CLIENT) {
			iface = ClientCreateInterface(name, &status);
		}
		else if constexpr (location == ENGINE) {
			iface = EngineCreateInterface(name, &status);
		}
		else if constexpr (location == LUASHARED) {
			iface = LuaSharedCreateInterface(name, &status);
		}
		else if constexpr (location == VStdLib) {
			iface = VStdLibCreateInterface(name, &status);
		}
		else if constexpr (location == Surface) {
			iface = SurfaceCreateInterface(name, &status);
		}
		else {
			iface = VGUICreateInterface(name, &status);
		}

		if (!iface) {
			hadError = true;
		}

		outIFace = reinterpret_cast<T*>(iface);
	}

	IEngineClient* engineClient = nullptr;
	CHLClient* client = nullptr;
	CInput* input = nullptr; 
	CClientState* clientState = nullptr;
	ClientModeShared* clientMode = nullptr;
	CGlobalVars* globalVars = nullptr;
	IClientEntityList* entityList = nullptr;
	IPanel* panel = nullptr;
	CPrediction* prediction = nullptr;
	IGameMovement* gameMovement = nullptr;
	IMoveHelper* moveHelper = nullptr;
	IEngineTrace* engineTrace = nullptr;
	CLuaShared* luaShared = nullptr;
	ILuaBase* clientLua = nullptr;
	CCvar* cvar = nullptr;
	ISurface* surface = nullptr;
	CModelRender* modelRender = nullptr;
	CViewRender* view = nullptr;

	void init() {
		HMODULE clientDll		= GetModuleHandleA("client.dll");
		HMODULE engineDll		= GetModuleHandleA("engine.dll");
		HMODULE luaSharedDll	= GetModuleHandleA("lua_shared.dll");
		HMODULE vguiDll			= GetModuleHandleA("vgui2.dll");
		HMODULE vstdlibDll		= GetModuleHandleA("vstdlib.dll");
		HMODULE surfaceDll		= GetModuleHandleA("vguimatsurface.dll");

		ClientCreateInterface		= reinterpret_cast<CreateInterfaceFn>(GetProcAddress(clientDll, "CreateInterface"));
		EngineCreateInterface		= reinterpret_cast<CreateInterfaceFn>(GetProcAddress(engineDll, "CreateInterface"));
		LuaSharedCreateInterface	= reinterpret_cast<CreateInterfaceFn>(GetProcAddress(luaSharedDll, "CreateInterface"));
		VGUICreateInterface			= reinterpret_cast<CreateInterfaceFn>(GetProcAddress(vguiDll, "CreateInterface"));
		VStdLibCreateInterface		= reinterpret_cast<CreateInterfaceFn>(GetProcAddress(vstdlibDll, "CreateInterface"));
		SurfaceCreateInterface		= reinterpret_cast<CreateInterfaceFn>(GetProcAddress(surfaceDll, "CreateInterface"));

		CreateInterface<ENGINE>("VEngineClient015", engineClient);
		CreateInterface<ENGINE>("VEngineModel016", modelRender);
		CreateInterface<ENGINE>("EngineTraceClient003", engineTrace);

		CreateInterface<CLIENT>("VClient017", client);
		CreateInterface<CLIENT>("VClientEntityList003",	entityList);
		CreateInterface<CLIENT>("VClientPrediction001",	prediction);
		CreateInterface<CLIENT>("GameMovement001", gameMovement);
		
		CreateInterface<VStdLib>("VEngineCvar007", cvar);
		CreateInterface<Surface>("VGUI_Surface030",	surface);
		CreateInterface<VGUI>("VGUI_Panel009", panel);
		CreateInterface<LUASHARED>("LUASHARED003", luaShared);

		if (client) {
			std::uintptr_t createMovePtr = vmt::get<std::uintptr_t>(client, 21);
			input = *reinterpret_cast<CInput**>(getAbsAddr(createMovePtr + 0x3F));

			std::uintptr_t initPtr = vmt::get<std::uintptr_t>(client, 0);
			globalVars = *reinterpret_cast<CGlobalVars**>(getAbsAddr(initPtr + 0x94));
		}

		std::uintptr_t moveHelperPtr = findPattern("client.dll", "48 89 78 68 89 78 70 48 8B 43 10 48 89 78 74 89 78 7C 48 8B 0D ") + 21;
		if (moveHelperPtr) {
			moveHelper = reinterpret_cast<IMoveHelper*>(getAbsAddr(moveHelperPtr));
		}

		std::uintptr_t clMovePtr = findPattern("engine.dll", "40 55 53 48 8D AC 24 38 F0 FF FF B8 C8 10 00 00 E8 ?? ?? ?? ?? 48 2B E0 0F 29 B4 24 B0 10 00 00");
		if (clMovePtr) {
			void* chokedCommandsPtr = getAbsAddr(clMovePtr + 0x10E, 2);
			clientState = reinterpret_cast<CClientState*>(reinterpret_cast<std::uintptr_t>(chokedCommandsPtr) - offsetof(CClientState, chokedcommands) - 1);
		}

		std::uintptr_t hudProcessInput = vmt::get<std::uintptr_t>(client, 10);
		if (hudProcessInput) {
			clientMode = *reinterpret_cast<ClientModeShared**>(getAbsAddr(hudProcessInput));
		}

		std::uintptr_t CHLClient_Shutdown = vmt::get<std::uintptr_t>(client, 2);
		if (CHLClient_Shutdown) {
			view = *reinterpret_cast<CViewRender**>(getAbsAddr(CHLClient_Shutdown + 0xC4));
		}



	}
}
 