#include "runstringex.h"
#include "luashared.h"
#include "vmt.h"
#include "interfaces.h"
#include "globals.h"

namespace runstringex {
	using RunStringExFn = bool(__fastcall*)(void* self, const char* filename, const char* path, const char* runstring, bool run, bool print_errors, bool dont_push_errors, bool no_returns);
	RunStringExFn RunStringExOriginal = nullptr;

	bool __fastcall RunStringExHookFunc(void* self, const char* filename, const char* path, const char* runstring, bool run, bool print_errors, bool dont_push_errors, bool no_returns) {
		if (globals::luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("RunStringEx");

			lua->PushString(filename);
			lua->PushString(runstring); 

			lua->Call(2, 0);
			lua->Pop(3);
		} 

		return RunStringExOriginal(self, filename, path, runstring, run, print_errors, dont_push_errors, no_returns);
	}

	void hook() {
		vmt::hook(interfaces::clientLua, &RunStringExOriginal, (const void*)RunStringExHookFunc, 111);
	}

	void unHook() {
		RunStringExFn dummy = nullptr;
		vmt::hook(interfaces::clientLua, &dummy, RunStringExOriginal, 111);
	}
}