
#pragma once

#include "effects.h"

namespace doimpacthook {
	void __cdecl ImpactFunctionHookFunc(const CEffectData& data);

	void hook();
	void unHook();
}
