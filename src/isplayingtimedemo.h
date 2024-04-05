#pragma once

namespace isplayingtimedemohook {
	bool __fastcall IsPlayingTimeDemoHookFunc(IEngineClient* self);

	void hook();
	void unHook();
}