#pragma once

#include "cprediction.h"

namespace runcommandhook {
	void __fastcall RunCommandHookFunc(CPrediction* self, CBaseEntity* player, CUserCmd* ucmd, IMoveHelper* moveHelper);

	void hook();
	void unHook();
} 