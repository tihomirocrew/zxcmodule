
#pragma once

class CHLClient;

namespace createmovehook {
	void __fastcall CreateMoveHookFunc(CHLClient* self, int sequence_number, float input_sample_frametime, bool active);

	void hook();
	void unHook();
}