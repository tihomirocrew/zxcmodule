#include "viewrenderhook.h"
#include "vmt.h"
#include "interfaces.h"
#include "luashared.h"
#include "isurface.h"
#include "globals.h"

namespace viewrender {
	using ViewRenderFn = bool(__fastcall*)(CHLClient* self, VRect* rect);
	ViewRenderFn ViewRenderOriginal = nullptr;

	bool __fastcall ViewRenderHookFunc(CHLClient* self, VRect* rect) {
		bool result = ViewRenderOriginal(self, rect);

		if (globals::luaInit) {
			auto* lua = interfaces::clientLua;

			//interfaces::surface->StartDrawing();

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("ViewRenderDraw");

			lua->Call(2, 0); 
			lua->Pop(2);

			//interfaces::surface->FinishDrawing();
		}

		return result;
	}

	void hook() {
		vmt::hook(interfaces::view, &ViewRenderOriginal, (const void*)ViewRenderHookFunc, 26);
	}

	void unHook() {
		ViewRenderFn dummy = nullptr;
		vmt::hook(interfaces::view, &dummy, ViewRenderOriginal, 26);
	}
}