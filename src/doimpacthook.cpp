
#include "doimpacthook.h"

#include "effects.h"
#include "patternscan.h"
#include "util.h"
#include "globals.h"
#include "interfaces.h"

namespace doimpacthook {

ClientEffectCallback impactFunctionOriginal = nullptr;
void __cdecl ImpactFunctionHookFunc(const CEffectData& data) {
	if (globals::luaInit) {
		auto* lua = interfaces::clientLua;
		lua->PushSpecial(SPECIAL_GLOB);
		lua->GetField(-1, "hook");
		lua->GetField(-1, "Run");

		lua->PushString("OnImpact");
		lua->CreateTable();
		{
			lua->PushVector(data.m_vOrigin);
			lua->SetField(-2, "m_vOrigin");

			lua->PushVector(data.m_vStart);
			lua->SetField(-2, "m_vStart");

			lua->PushVector(data.m_vNormal);
			lua->SetField(-2, "m_vNormal");

			lua->PushAngle(data.m_vAngles);
			lua->SetField(-2, "m_vAngles");

			lua->PushNumber(data.m_nEntIndex);
			lua->SetField(-2, "m_nEntIndex");

			lua->PushNumber(data.m_nDamageType);
			lua->SetField(-2, "m_nDamageType");

			lua->PushNumber(data.m_nHitBox);
			lua->SetField(-2, "m_nHitBox");
		}
		lua->Call(2, 0);
		lua->Pop(2); // _G + hook
	} 

	impactFunctionOriginal(data);
}

void hookEffect(ClientEffectCallback hookFunc, const char* effectName) {
	static CClientEffectRegistration* s_pHead = nullptr;
	if (!s_pHead) {
		auto dispatchEffectToCallbackInst = findPattern("client.dll", "48 89 5C 24 30 48 8B 1D ?? ?? ?? ?? 48 85 DB");
		s_pHead = *reinterpret_cast<CClientEffectRegistration**>(getAbsAddr(dispatchEffectToCallbackInst + 0x5));
	}

	for (CClientEffectRegistration* pReg = s_pHead; pReg; pReg = pReg->m_pNext) {
		if (pReg->m_pEffectName && strcmp(pReg->m_pEffectName, effectName) == 0) {
			impactFunctionOriginal = pReg->m_pFunction;
			pReg->m_pFunction = hookFunc;
		}
	}
}

void hook() {
	hookEffect(ImpactFunctionHookFunc, "Impact");
}

void unHook() {
	hookEffect(impactFunctionOriginal, "Impact");
}

}

