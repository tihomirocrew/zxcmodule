#pragma once

#include "clientmodeshared.h"

namespace cm_createmove {
	bool __fastcall ClientModeCreateMoveHookFunc(ClientModeShared* self, float flInputSampleTime, CUserCmd* cmd);

	void hook();
	void unHook();
}

