#include "clientmodecreatemovehook.h"
#include "interfaces.h"
#include "cusercmd.h"
#include "luashared.h"
#include "globals.h"
#include "prediction.h"

#include <intrin.h>

namespace cm_createmove {
	using CreateMoveFn = bool(__fastcall*)(ClientModeShared* self, float flInputSampleTime, CUserCmd* cmd);
	CreateMoveFn createMoveOriginal = nullptr;

	bool __fastcall ClientModeCreateMoveHookFunc(ClientModeShared* self, float flInputSampleTime, CUserCmd* cmd) {
		static bool runtimeHooksInit = false;

		if (!cmd->command_number) 
			return createMoveOriginal(self, flInputSampleTime, cmd);

		// g_prediction.GetServerTime( cmd );

		if (globals::luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("PreCreateMove");
			 
			lua->PushUserType(cmd, Type::UserCmd);

			lua->Call(2, 0);
			lua->Pop(2);
		}

		createMoveOriginal(self, flInputSampleTime, cmd);

		return false;
	}

	void hook() {
		vmt::hook(interfaces::clientMode, &createMoveOriginal, (const void*)ClientModeCreateMoveHookFunc, 21);
	}

	void unHook() {
		CreateMoveFn dummy = nullptr;
		vmt::hook(interfaces::clientMode, &dummy, createMoveOriginal, 21);
	}
}
 