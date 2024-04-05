
#include "framestagenotify.h"
#include "interfaces.h"
#include "globals.h"
#include "luashared.h"

namespace framestagenotify {
	using FrameStageFn = void(__fastcall*)(CHLClient* self, int stage);
	FrameStageFn FrameStageOriginal = nullptr;

	void __fastcall FrameStageNotifyHookFunc(CHLClient* self, int stage) {

		if (!globals::luaInit && stage == ClientFrameStage_t::FRAME_START) {
			interfaces::clientLua = interfaces::luaShared->GetLuaInterface(static_cast<int>(LuaInterface::CLIENT));

			globals::luaInit = true;
		}

		if (globals::luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("PreFrameStageNotify");

			lua->PushNumber(stage);

			lua->Call(2, 0);
			lua->Pop(2);
		}

		globals::currentStage = stage;

		FrameStageOriginal(self, stage);

		if (globals::luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("PostFrameStageNotify");

			lua->PushNumber(stage);

			lua->Call(2, 0);
			lua->Pop(2);
		}
	}

	void hook() {
		vmt::hook(interfaces::client, &FrameStageOriginal, (const void*)FrameStageNotifyHookFunc, 35);
	}

	void unHook() {
		FrameStageFn dummy = nullptr;
		vmt::hook(interfaces::client, &dummy, FrameStageOriginal, 35);
	}
}