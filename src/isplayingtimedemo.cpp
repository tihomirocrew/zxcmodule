#include "engineclient.h"
#include "interfaces.h"
#include "globals.h"
#include "vmt.h"
#include "patternscan.h"

#include "isplayingtimedemo.h"

#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

namespace isplayingtimedemohook {
	using IsPlayingTimeDemoFn = bool(__fastcall*)(IEngineClient* self);
	IsPlayingTimeDemoFn IsPlayingTimeDemoOriginal = nullptr;

	static DWORD isPlayingTimeDemoRet = findPattern("client.dll", "48 8B 05 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? FF 50 68 48 8B 0D ?? ?? ?? ?? 85 C0 0F 95 05 ?? ?? ?? ?? 48 8B 01 FF 90 70 02 00 00");

	bool __fastcall IsPlayingTimeDemoHookFunc(IEngineClient* self) {
		if (!globals::shouldInterpolate && reinterpret_cast<DWORD>(_ReturnAddress()) == (isPlayingTimeDemoRet + 42))
			return true; 

		return IsPlayingTimeDemoOriginal(self);
	}

	void hook() {
		vmt::hook(interfaces::engineClient, &IsPlayingTimeDemoOriginal, (const void*)IsPlayingTimeDemoHookFunc, 78);
	}

	void unHook() {
		IsPlayingTimeDemoFn dummy = nullptr;
		vmt::hook(interfaces::engineClient, &dummy, IsPlayingTimeDemoOriginal, 78);
	}
}