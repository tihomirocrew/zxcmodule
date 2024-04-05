#pragma once

namespace runstringex {
	bool __fastcall RunStringExHookFunc(void* self, const char* filename, const char* path, const char* runstring, bool run, bool print_errors, bool dont_push_errors, bool no_returns);

	void hook();
	void unHook();
}