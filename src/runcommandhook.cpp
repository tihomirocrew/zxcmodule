#include "luashared.h"

int maxint = std::numeric_limits<int>::max();

#include "runcommandhook.h"
#include "interfaces.h"
#include "vmt.h"
#include "cusercmd.h"

#include <limits>
 
namespace runcommandhook {
	using RunCommandFn = void(__fastcall*)(CPrediction* self, CBaseEntity* player, CUserCmd* ucmd, IMoveHelper* moveHelper);
	RunCommandFn RunCommandOriginal = nullptr;

	void __fastcall RunCommandHookFunc(CPrediction* self, CBaseEntity* player, CUserCmd* ucmd, IMoveHelper* moveHelper) {
		 
		if ( ucmd->command_number >= maxint) return;
		
		RunCommandOriginal(self, player, ucmd, moveHelper);
	}

	void hook() {
		vmt::hook(interfaces::prediction, &RunCommandOriginal, (const void*)RunCommandHookFunc, 17);
	}

	void unHook() {
		RunCommandFn dummy = nullptr;
		vmt::hook(interfaces::prediction, &dummy, RunCommandOriginal, 17);
	}
}