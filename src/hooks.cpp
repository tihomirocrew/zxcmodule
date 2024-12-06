#include "globals.h"
#include "luashared.h"
#include "patternscan.h"
#include "vmt.h"
#include "util.h"
#include "interfaces.h"

#include "hooks.h"

#include "clientmodeshared.h"
#include "effects.h"
#include "chlclient.h"
#include "cprediction.h"
#include "cusercmd.h"
#include "cinput.h"
#include "prediction.h"
#include "engineclient.h"
#include "entity.h"
#include "inetchannel.h"
#include "globalvars.h"
#include "clientstate.h"
#include "isurface.h"
//#include "modelrender.h"

#include "minhook/minhook.h"

#include <deque>
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

class CStudioHdr;
class matrix3x4_t;

struct OutCommand {
	bool is_outgoing;
	bool is_used;
	int command_number;
	int previous_command_number;
};

struct model_t
{
	int pad;
	char* name;
};

struct DrawModelState_t
{
	void* m_pStudioHdr;
	void* m_pStudioHWData;
	void* m_pRenderable;
	const matrix3x4_t* m_pModelToWorld;
	void* m_decals;
	int						m_drawFlags;
	int						m_lod;
};

struct ModelRenderInfo_t
{
	Vector origin;
	Angle angles;
	void* pRenderable;
	const model_t* pModel;
	const matrix3x4_t* pModelToWorld;
	const matrix3x4_t* pLightingOffset;
	const Vector* pLightingOrigin;
	int flags;
	int entity_index;
	int skin;
	int body;
	int hitboxset;
	void* instance;

	ModelRenderInfo_t()
	{
		pModelToWorld = NULL;
		pLightingOffset = NULL;
		pLightingOrigin = NULL;
	}
};

namespace detours {
	bool luaInit = false;

	static void* jePtr;
	static DWORD isPlayingTimeDemoRet = findPattern("client.dll", "48 8B 05 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? FF 50 68 48 8B 0D ?? ?? ?? ?? 85 C0 0F 95 05 ?? ?? ?? ?? 48 8B 01 FF 90 70 02 00 00");
	
	auto SeqChangePattern = findPattern("client.dll", "48 85 D2 0F 84 D5 00 00 00 48 89 6C 24 10 48 89 74 24 18");
	auto CL_MovePattern = findPattern("engine.dll", "40 55 53 48 8D AC 24 38 F0 FF FF B8 C8 10 00 00 E8 ?? ?? ?? ?? 48 2B E0 0F 29 B4 24 B0 10 00 00");
	auto PacketStartPattern = findPattern("engine.dll", "48 8B 01 48 83 C4 20 5B 48 FF A0 ?? ?? ?? ?? 48 83 C4 20 5B C3 CC CC CC CC CC CC CC 89 91 ?? ?? ?? ??") + 28;

	ClientEffectCallback impactFunctionOriginal = nullptr;
	CBaseEntity* localPlayer;

	// Pre CreateMove
	using PreCreateMoveFn = bool(__fastcall*)(ClientModeShared* self, float flInputSampleTime, CUserCmd* cmd);
	PreCreateMoveFn preCreateMoveOriginal = nullptr;

	bool __fastcall ClientModeCreateMoveHookFunc(ClientModeShared* self, float flInputSampleTime, CUserCmd* cmd) {
		static bool runtimeHooksInit = false;

		if (!cmd->command_number)
			return preCreateMoveOriginal(self, flInputSampleTime, cmd);

		if (luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("PreCreateMove");

			lua->PushUserType(cmd, Type::UserCmd);

			lua->Call(2, 0);
			lua->Pop(2);
		}

		preCreateMoveOriginal(self, flInputSampleTime, cmd);

		return false;
	}

	// Post CreateMove
	using PostCreateMoveFn = void(__fastcall*)(CHLClient* self, int sequence_number, float input_sample_frametime, bool active);
	PostCreateMoveFn postCreateMoveOriginal = nullptr;
	
	static uint8_t jzBytes[] = { 0x0f, 0x84, 0x04, 0x01, 0x00, 0x00 };
	static uint8_t jnzBytes[] = { 0x0f, 0x85, 0x04, 0x01, 0x00, 0x00 };

	void __fastcall CreateMoveHookFunc(CHLClient* self, int sequence_number, float input_sample_frametime, bool active) {
		CVerifiedUserCmd* vcmd = interfaces::input->GetVerifiedCommand(sequence_number);
		CUserCmd* cmd = &vcmd->cmd;

		postCreateMoveOriginal(self, sequence_number, input_sample_frametime, active);

		if (luaInit) {
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

		memcpy(jePtr, globals::bSendPacket ? jzBytes : jnzBytes, 6);

		cmd->forwardmove = std::clamp(cmd->forwardmove, -10000.f, 10000.f);
		cmd->sidemove = std::clamp(cmd->sidemove, -10000.f, 10000.f);
		cmd->upmove = std::clamp(cmd->upmove, -10000.f, 10000.f);

		vcmd->crc = cmd->GetChecksum();
	}

	// Frame stage notify 
	using FrameStageFn = void(__fastcall*)(CHLClient* self, int stage);
	FrameStageFn FrameStageOriginal = nullptr;
	
	void __fastcall FrameStageNotifyHookFunc(CHLClient* self, int stage) {
		if (!luaInit && stage == ClientFrameStage_t::FRAME_START) {
			interfaces::clientLua = interfaces::luaShared->GetLuaInterface(static_cast<int>(LuaInterface::CLIENT));
			
			luaInit = true;
		}

		if (luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("PreFrameStageNotify");

			lua->PushNumber(stage);

			lua->Call(2, 0);
			lua->Pop(2);
		}

		FrameStageOriginal(self, stage);

		if (luaInit) {
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

	// IsPlayingTimeDemo 
	using IsPlayingTimeDemoFn = bool(__fastcall*)(IEngineClient* self);
	IsPlayingTimeDemoFn IsPlayingTimeDemoOriginal = nullptr;

	bool __fastcall IsPlayingTimeDemoHookFunc(IEngineClient* self) {
		if (!globals::shouldInterpolate && reinterpret_cast<DWORD>(_ReturnAddress()) == (isPlayingTimeDemoRet + 42))
			return true;

		return IsPlayingTimeDemoOriginal(self);
	}

	// RunCommand 
	using RunCommandFn = void(__fastcall*)(CPrediction* self, CBaseEntity* player, CUserCmd* ucmd, IMoveHelper* moveHelper);
	RunCommandFn RunCommandOriginal = nullptr;
	
	void __fastcall RunCommandHookFunc(CPrediction* self, CBaseEntity* player, CUserCmd* ucmd, IMoveHelper* moveHelper) {
		if (ucmd->command_number >= INT_MAX) return;

		RunCommandOriginal(self, player, ucmd, moveHelper);
	}

	// View Render 
	using ViewRenderFn = bool(__fastcall*)(CHLClient* self, VRect* rect);
	ViewRenderFn ViewRenderOriginal = nullptr;

	bool __fastcall ViewRenderHookFunc(CHLClient* self, VRect* rect) {
		bool result = ViewRenderOriginal(self, rect);

		if (luaInit) {
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

	// Should Interpolate
	using ShouldInterpolateFn = bool(__fastcall*)(CBaseEntity* self);
	ShouldInterpolateFn ShouldInterpolateOriginal = nullptr;

	bool __fastcall ShouldInterpolateHookFunc(CBaseEntity* self) {
		if (globals::shouldInterpolate)
			return ShouldInterpolateOriginal(self);

		return (self == localPlayer) ? ShouldInterpolateOriginal(self) : false;
	}

	// CL_Move
	using CLMoveFn = void(__fastcall*)(float accumulated_extra_samples, bool bFinalTick);
	CLMoveFn CLMoveOriginal = nullptr;

	void __fastcall CLMoveHookFunc(float accumulated_extra_samples, bool bFinalTick) {
		globals::bIsRecharging = false;
		globals::bIsShifting = false;

		if (not globals::canShiftTickbase) {
			CLMoveOriginal(accumulated_extra_samples, bFinalTick);
			return;
		}

		if (globals::bShouldRecharge and globals::curTickbaseCharge < globals::maxTickbaseShift) {
			globals::curTickbaseCharge++;
			globals::bIsRecharging = true;
			return;
		}

		if (globals::canShiftTickbase and globals::curTickbaseCharge >= globals::minTickbaseShift and globals::bShouldShift) {
			globals::bIsShifting = true;

			globals::bSendPacket = false;
			interfaces::engineClient->GetNetChannel()->SetChoked();
			interfaces::clientState->chokedcommands++;

			int shiftAmt = globals::minTickbaseShift; //- interfaces::clientState->chokedcommands + 1;

			for (int i = 0; i < shiftAmt; ++i)
			{
				globals::curTickbaseCharge--;

				if (i != globals::curTickbaseCharge - 1) {
					globals::bSendPacket = true;
				}

				CLMoveOriginal(accumulated_extra_samples, bFinalTick);
			}

			globals::bIsShifting = false;

			return;
		}

		CLMoveOriginal(accumulated_extra_samples, bFinalTick);
	}

	// CheckForSequenceChange
	using SeqChangeFn = bool(__fastcall*)(void* self, CStudioHdr* hdr, int nCurSequence, bool bForceNewSequence, bool bInterpolate);
	SeqChangeFn SeqChangeOriginal = nullptr;

	bool __fastcall CheckForSequenceChangeHookFunc(void* self, CStudioHdr* hdr, int nCurSequence, bool bForceNewSequence, bool bInterpolate) {
		if (!globals::shouldInterpolateSequences)
			bInterpolate = false;

		return SeqChangeOriginal(self, hdr, nCurSequence, bForceNewSequence, bInterpolate);
	}

	// Packet start 
	using PacketStartFn = void(__fastcall*)(CClientState* self, int incoming_sequence, int outgoing_acknowledged);
	PacketStartFn PacketStartOriginal = nullptr;

	std::deque<OutCommand> cmds;

	void __fastcall PacketStartHookFunc(CClientState* self, int incoming_sequence, int outgoing_acknowledged) {
		for (auto it = cmds.rbegin(); it != cmds.rend(); ++it) {
			if (!it->is_outgoing)
				continue;

			if (it->command_number == outgoing_acknowledged || outgoing_acknowledged > it->command_number && (!it->is_used || it->previous_command_number == outgoing_acknowledged)) {
				it->previous_command_number = outgoing_acknowledged;
				it->is_used = true;
				PacketStartOriginal(self, incoming_sequence, outgoing_acknowledged);
				break;
			}
		}

		auto result = false;

		for (auto it = cmds.begin(); it != cmds.end();) {
			if (outgoing_acknowledged == it->command_number || outgoing_acknowledged == it->previous_command_number)
				result = true;

			if (outgoing_acknowledged > it->command_number && outgoing_acknowledged > it->previous_command_number)
				it = cmds.erase(it);
			else
				++it;
		}

		if (!result)
			PacketStartOriginal(self, incoming_sequence, outgoing_acknowledged);
	}

	// UpdateClientsideAnimation
	using UpdateClientsideAnimationFn = void(__fastcall*)(CBasePlayer* self);
	UpdateClientsideAnimationFn UpdateClientsideAnimationOriginal = nullptr;

	void __fastcall UpdateClientsideAnimationHookFunc(CBasePlayer* self) {
		if (!globals::shouldFixAnimations)
			return UpdateClientsideAnimationOriginal(self);

		CBasePlayer* localPlayer = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(interfaces::engineClient->GetLocalPlayer()));

		globals::updateTicks = 1;
		globals::updateAllowed = false;

		if (luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("ShouldUpdateAnimation");

			lua->PushNumber(self->GetClientNetworkable()->entIndex());

			lua->Call(2, 0);
			lua->Pop(2);
		}

		if (!globals::updateAllowed)
			return;

		VarMapping_t& map = self->GetVarMapping();
		for (int i = 0; i < map.m_nInterpolatedEntries; i++) {
			VarMapEntry_t& entry = map.m_Entries[i];
			entry.m_bNeedsToInterpolate = false;
		}

		float OldCurtime = interfaces::globalVars->curtime;
		float OldFrameTime = interfaces::globalVars->frametime;
		int OldFlags = self->m_iEFlags();
		int OldEffects = self->m_fEffects();

		float simulationTime = self == localPlayer ? g_prediction.GetServerTime() : self->m_flSimulationTime();

		interfaces::globalVars->curtime = simulationTime;
		interfaces::globalVars->frametime = interfaces::globalVars->interval_per_tick * globals::updateTicks;

		self->m_iEFlags() |= static_cast<int>(EFlags::DIRTY_ABSVELOCITY);
		self->m_fEffects() |= static_cast<int>(EEffects::NOINTERP);

		UpdateClientsideAnimationOriginal(self);

		interfaces::globalVars->curtime = OldCurtime;
		interfaces::globalVars->frametime = OldFrameTime;

		self->m_iEFlags() = OldFlags;
		self->m_fEffects() = OldEffects;

		globals::updateTicks = 1;
		globals::updateAllowed = false;
	}

	// SendNetMsg 
	using SendNetMsgFn = bool(__fastcall*)(INetChannel* self, INetMessage& msg, bool bForceReliable, bool bVoice);
	SendNetMsgFn SendNetMsgOriginal = nullptr;

	void SendMove() {
		uint8_t data[MAX_CMD_BUFFER];

		int nextCommandNr = interfaces::clientState->lastoutgoingcommand + interfaces::clientState->chokedcommands + 1;

		CLC_Move moveMsg;
		moveMsg.Init();

		moveMsg.m_DataOut.StartWriting(data, sizeof(data));

		// How many real new commands have queued up
		moveMsg.m_nNewCommands = 1 + interfaces::clientState->chokedcommands;
		moveMsg.m_nNewCommands = std::clamp(moveMsg.m_nNewCommands, 0, MAX_NEW_COMMANDS);

		if (globals::bShouldSlowMo) moveMsg.m_nNewCommands = 0;

		int extraCommands = interfaces::clientState->chokedcommands + 1 - moveMsg.m_nNewCommands;

		int backupCommands = max(2, extraCommands);
		moveMsg.m_nBackupCommands = std::clamp(backupCommands, 0, MAX_BACKUP_COMMANDS);

		int numCommands = moveMsg.m_nNewCommands + moveMsg.m_nBackupCommands;

		int from = -1;	// first command is deltaed against zeros 

		bool bOK = true;

		for (int to = nextCommandNr - numCommands + 1; to <= nextCommandNr; to++) {
			bool isnewcmd = to >= (nextCommandNr - moveMsg.m_nNewCommands + 1);

			// first valid command number is 1
			bOK = bOK && interfaces::client->WriteUsercmdDeltaToBuffer(&moveMsg.m_DataOut, from, to, isnewcmd);
			from = to;
		}

		// only write message if all usercmds were written correctly, otherwise parsing would fail
		if (bOK) {
			INetChannel* chan = reinterpret_cast<INetChannel*>(interfaces::engineClient->GetNetChannelInfo());

			chan->m_nChokedPackets -= extraCommands;
			if (globals::bShouldSlowMo) chan->m_nChokedPackets = 0;

			SendNetMsgOriginal(chan, reinterpret_cast<INetMessage&>(moveMsg), false, false);
		}
	}

	bool __fastcall SendNetMsgHookFunc(INetChannel* self, INetMessage& msg, bool bForceReliable, bool bVoice) {
		if (msg.GetGroup() == static_cast<int>(NetMessage::clc_VoiceData))
			bVoice = true;

		auto msgName = msg.GetName();

		if (luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("SendNetMsg");

			lua->PushString(msgName);

			lua->Call(2, 0);

			/*bool ret = lua->GetBool(-1);

			if (ret == false) {
				lua->Pop(2);
				return true;
			}*/

			lua->Pop(2);
		}

		if (strcmp(msgName, "clc_Move") == 0) {
			SendMove();

			return true;
		}

		return SendNetMsgOriginal(self, msg, bForceReliable, bVoice);
	}

	// Dispatch effect 
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
	
	void __cdecl ImpactFunctionHookFunc(const CEffectData& data) {
		if (luaInit) {
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
			lua->Pop(2);
		}

		impactFunctionOriginal(data);
	}

	// DrawModelEx 
	using DrawModelExecuteFn = void(__fastcall*)(void* modelrender, const DrawModelState_t& state, ModelRenderInfo_t* pInfo, matrix3x4_t* pCustomBoneToWorld);
	DrawModelExecuteFn DrawModelExecuteOriginal = nullptr;

	void __fastcall DrawModelExecuteHookFunc(void* modelrender, const DrawModelState_t& state, ModelRenderInfo_t* pInfo, matrix3x4_t* pCustomBoneToWorld) {
		if (pInfo->entity_index) {
			if (luaInit) {
				auto* lua = interfaces::clientLua;
				lua->PushSpecial(SPECIAL_GLOB);
				lua->GetField(-1, "hook");
				lua->GetField(-1, "Run");

				lua->PushString("DrawModelExecute");

				lua->PushNumber(pInfo->entity_index);
				lua->PushNumber(pInfo->flags);

				lua->Call(3, 0);
				lua->Pop(2); 
			}
		}

		return DrawModelExecuteOriginal(modelrender, state, pInfo, pCustomBoneToWorld);
	}

	void hook() {
		std::uintptr_t clMovePtr = findPattern("engine.dll", "40 55 53 48 8D AC 24 38 F0 FF FF B8 C8 10 00 00 E8 ?? ?? ?? ?? 48 2B E0 0F 29 B4 24 B0 10 00 00");
		jePtr = reinterpret_cast<void*>(clMovePtr + 0x168);

		DWORD dummy;
		VirtualProtect(jePtr, 8, PAGE_EXECUTE_READWRITE, &dummy);

		globals::device = *reinterpret_cast<IDirect3DDevice9**>(getAbsAddr(findPattern("shaderapidx9.dll", "3D 7C 01 76 88 74 07 3D 0E 00 07 80 75 34 48 8B 0D") + 14));

		void* EndSceneT = vmt::get<void*>(globals::device, 42);
		void* SendNetMsgT = vmt::get<void*>(interfaces::engineClient->GetNetChannel(), 40);

		vmt::hook(interfaces::modelRender, &DrawModelExecuteOriginal, (const void*)DrawModelExecuteHookFunc, 20);
		vmt::hook(interfaces::client, &postCreateMoveOriginal, (const void*)CreateMoveHookFunc, 21);
		vmt::hook(interfaces::client, &FrameStageOriginal, (const void*)FrameStageNotifyHookFunc, 35);
		//vmt::hook(interfaces::client, &ViewRenderOriginal, (const void*)ViewRenderHookFunc, 26);
		vmt::hook(interfaces::engineClient, &IsPlayingTimeDemoOriginal, (const void*)IsPlayingTimeDemoHookFunc, 78);
		vmt::hook(interfaces::prediction, &RunCommandOriginal, (const void*)RunCommandHookFunc, 17);
		vmt::hook(interfaces::clientMode, &preCreateMoveOriginal, (const void*)ClientModeCreateMoveHookFunc, 21);

		hookEffect(ImpactFunctionHookFunc, "Impact");

		if (MH_Initialize() == MH_OK)
		{
			MH_CreateHook(SendNetMsgT, (LPVOID)&SendNetMsgHookFunc, (LPVOID*)&SendNetMsgOriginal);

			MH_CreateHook((LPVOID)SeqChangePattern, (LPVOID)&CheckForSequenceChangeHookFunc, (LPVOID*)&SeqChangeOriginal);
			MH_CreateHook((LPVOID)CL_MovePattern, (LPVOID)&CLMoveHookFunc, (LPVOID*)&CLMoveOriginal);
			MH_CreateHook((LPVOID)PacketStartPattern, (LPVOID)&PacketStartHookFunc, (LPVOID*)&PacketStartOriginal);

			MH_EnableHook(MH_ALL_HOOKS);
		}
	}

	void postInit() {
		localPlayer = reinterpret_cast<CBaseEntity*>(interfaces::entityList->GetClientEntity(interfaces::engineClient->GetLocalPlayer()));

		void* ShouldInterpolateT = vmt::get<void*>(localPlayer, 146);
		void* UpdateClientAnimsT = vmt::get<void*>(localPlayer, 235);

		MH_CreateHook(ShouldInterpolateT, (LPVOID)&ShouldInterpolateHookFunc, (LPVOID*)&ShouldInterpolateOriginal);
		MH_CreateHook(UpdateClientAnimsT, (LPVOID)&UpdateClientsideAnimationHookFunc, (LPVOID*)&UpdateClientsideAnimationOriginal);

		MH_EnableHook(ShouldInterpolateT);
		MH_EnableHook(UpdateClientAnimsT);
	}

	void unHook() {
		memcpy(jePtr, jzBytes, 6);

		PreCreateMoveFn PreCreateMoveDummy = nullptr; 
		PostCreateMoveFn PostCreateMoveDummy = nullptr;
		FrameStageFn FrameStageDummy = nullptr;
		RunCommandFn RunCommandDummy = nullptr;
		IsPlayingTimeDemoFn IsPlayingTimeDemoDummy = nullptr;
		DrawModelExecuteFn DrawModelDummy = nullptr;
		ViewRenderFn ViewRenderDummy = nullptr;
		 
		vmt::hook(interfaces::clientMode, &PreCreateMoveDummy, preCreateMoveOriginal, 21);
		vmt::hook(interfaces::client, &PostCreateMoveDummy, postCreateMoveOriginal, 21);
		vmt::hook(interfaces::modelRender, &DrawModelDummy, DrawModelExecuteOriginal, 20);
		vmt::hook(interfaces::client, &FrameStageDummy, FrameStageOriginal, 35);
		//vmt::hook(interfaces::client, &ViewRenderDummy, ViewRenderOriginal, 26);
		vmt::hook(interfaces::prediction, &RunCommandDummy, RunCommandOriginal, 17);
		vmt::hook(interfaces::engineClient, &IsPlayingTimeDemoDummy, IsPlayingTimeDemoOriginal, 78);
		
		hookEffect(impactFunctionOriginal, "Impact");

		MH_DisableHook(MH_ALL_HOOKS);
		MH_Uninitialize();
	}
}