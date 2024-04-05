#pragma once

#include "chlclient.h"

namespace viewrender {
	bool __fastcall ViewRenderHookFunc(CHLClient* self, VRect* rect);

	void hook();
	void unHook();
}