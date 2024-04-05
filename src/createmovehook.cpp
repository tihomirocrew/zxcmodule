
#include "createmovehook.h"
#include "cusercmd.h"
#include "cinput.h"

#include "luashared.h"
#include "interfaces.h"
#include "globals.h"
#include "vmt.h"
#include "patternscan.h"

#include "engineclient.h"
#include "entity.h"

namespace createmovehook {
	static void* jePtr;
	Angle prevViewAngles;

	using CreateMoveFn = void(__fastcall*)(CHLClient* self, int sequence_number, float input_sample_frametime, bool active);
	CreateMoveFn createMoveOriginal = nullptr;

	void __fastcall CreateMoveHookFunc(CHLClient* self, int sequence_number, float input_sample_frametime, bool active) {
		CVerifiedUserCmd* vcmd = interfaces::input->GetVerifiedCommand(sequence_number);
		CUserCmd* cmd = &vcmd->cmd;

		if (globals::bSendPacket and cmd->tick_count > 0) {
			//CBasePlayer* localPlayer = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(interfaces::engineClient->GetLocalPlayer()));
			//CBasePlayerAnimState* animState = localPlayer->GetAnimState();

			//animState->Update(cmd->viewangles.y, cmd->viewangles.p);
		}

		createMoveOriginal(self, sequence_number, input_sample_frametime, active);

		if (globals::luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("PostCreateMove");

			lua->PushUserType(cmd, Type::UserCmd);

			lua->Call(2, 0); 
			lua->Pop(2); 
		}

		interfaces::engineClient->SetViewAngles(cmd->viewangles);

		if (globals::bSendPacket) {
			// bSendPacket = true
			static uint8_t jzBytes[] = { 0x0f, 0x84, 0x04, 0x01, 0x00, 0x00 }; 
			memcpy(jePtr, jzBytes, 6); 
		}
		else {
			// bSendPacket = false
			static uint8_t jnzBytes[] = { 0x0f, 0x85, 0x04, 0x01, 0x00, 0x00 };
			memcpy(jePtr, jnzBytes, 6);
		}

		cmd->forwardmove = std::clamp(cmd->forwardmove, -10000.f, 10000.f);
		cmd->sidemove = std::clamp(cmd->sidemove, -10000.f, 10000.f);
		cmd->upmove = std::clamp(cmd->upmove, -10000.f, 10000.f); 

		vcmd->crc = cmd->GetChecksum();
	}	

	void hook() {
		std::uintptr_t clMovePtr = findPattern("engine.dll", "40 55 53 48 8D AC 24 38 F0 FF FF B8 C8 10 00 00 E8 ?? ?? ?? ?? 48 2B E0 0F 29 B4 24 B0 10 00 00");
		jePtr = reinterpret_cast<void*>(clMovePtr + 0x168);

		DWORD dummy;
		VirtualProtect(jePtr, 8, PAGE_EXECUTE_READWRITE, &dummy);

		vmt::hook(interfaces::client, &createMoveOriginal, (const void*)CreateMoveHookFunc, 21);
	}

	void unHook() {

		static uint8_t jzBytes[] = { 0x0f, 0x84, 0x04, 0x01, 0x00, 0x00 };
		memcpy(jePtr, jzBytes, 6);

		CreateMoveFn dummy = nullptr;
		vmt::hook(interfaces::client, &dummy, createMoveOriginal, 21);
	} 

}
 