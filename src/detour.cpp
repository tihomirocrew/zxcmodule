#include "detour.h"

#include "globals.h"
#include "interfaces.h"
#include "entity.h"
#include "cliententitylist.h"
#include "engineclient.h"
#include "globalvars.h"
#include "prediction.h"
#include "inetchannel.h"
#include "clientstate.h"
#include "luafuncs.h"
#include "chlclient.h"
#include "isurface.h"
#include "d3d9.h"
#include <intrin.h>

#define GetOriginValue(name) \
DWORD o_##name;\
globals::device->GetRenderState(name, &o_##name);
#define SetOriginValue(name) globals::device->SetRenderState(name, o_##name);
IDirect3DSurface9* gamesurface = NULL;
IDirect3DTexture9* gametexture = NULL;
IDirect3DVertexBuffer9* v_buffer = NULL;
struct CUSTOMVERTEX {
	FLOAT x, y, z, rhw, u, v;
};

namespace detour {
	CBaseEntity* localPlayer;

	/*
		Should interpolate
	*/

	using ShouldInterpolateFn = bool(__fastcall*)(CBaseEntity* self);
	ShouldInterpolateFn ShouldInterpolateOriginal = nullptr;

	bool __fastcall ShouldInterpolateHookFunc(CBaseEntity* self) {
		if (globals::shouldInterpolate)
			return ShouldInterpolateOriginal(self);

		return (self == localPlayer) ? ShouldInterpolateOriginal(self) : false;
	}

	/*
		CL_Move
	*/

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

	/*
		Check for sequence change
	*/

	using SeqChangeFn = bool(__fastcall*)(void* self, CStudioHdr* hdr, int nCurSequence, bool bForceNewSequence, bool bInterpolate);
	SeqChangeFn SeqChangeOriginal = nullptr;

	bool __fastcall CheckForSequenceChangeHookFunc(void* self, CStudioHdr* hdr, int nCurSequence, bool bForceNewSequence, bool bInterpolate) {
		if (!globals::shouldInterpolateSequences)
			bInterpolate = false;

		return SeqChangeOriginal(self, hdr, nCurSequence, bForceNewSequence, bInterpolate);
	}

	/*
		Packet start
	*/

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

	/*
		Update clientside animation
	*/

	using UpdateClientsideAnimationFn = void(__fastcall*)(CBasePlayer* self);
	UpdateClientsideAnimationFn UpdateClientsideAnimationOriginal = nullptr;

	void __fastcall UpdateClientsideAnimationHookFunc(CBasePlayer* self) {
		if (!globals::shouldFixAnimations)
			return UpdateClientsideAnimationOriginal(self);

		CBasePlayer* localPlayer = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(interfaces::engineClient->GetLocalPlayer()));

		globals::updateTicks = 1;
		globals::updateAllowed = false;

		if (globals::luaInit) {
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

	/*
		EndScene
	*/

	using EndSceneFn = long(__stdcall*)(IDirect3DDevice9*);
	EndSceneFn EndSceneOriginal = nullptr;

	long __stdcall EndSceneHookFunc(IDirect3DDevice9* device) {
		static const void* gameOverlayHook = 0;

		const void* returnAddress = _ReturnAddress();
		if (!gameOverlayHook) {
			MEMORY_BASIC_INFORMATION info;
			VirtualQuery(returnAddress, &info, sizeof(MEMORY_BASIC_INFORMATION));

			char moduleName[MAX_PATH];
			GetModuleFileNameA((HMODULE)info.AllocationBase, moduleName, MAX_PATH);

			if (strstr(moduleName, "gameoverlay"))
				gameOverlayHook = returnAddress;
		}

		bool inGameOverlay = returnAddress == gameOverlayHook;

		HRESULT result = EndSceneOriginal(device);

		if (!globals::luaInit) return result;

		// Render state fix start 

		device->SetRenderTarget(0, 0);

		IDirect3DStateBlock9* pixel_state = NULL;
		IDirect3DVertexDeclaration9* vertex_declaration;
		IDirect3DVertexShader9* vertex_shader;
		device->CreateStateBlock(D3DSBT_PIXELSTATE, &pixel_state);
		device->GetVertexDeclaration(&vertex_declaration);
		device->GetVertexShader(&vertex_shader);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);

		DWORD state1;
		DWORD state2;
		DWORD state3;
		device->GetRenderState(D3DRS_COLORWRITEENABLE, &state1);
		device->GetRenderState(D3DRS_COLORWRITEENABLE, &state2);
		device->GetRenderState(D3DRS_SRGBWRITEENABLE, &state3);

		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, false);

		device->SetPixelShader(NULL);
		device->SetVertexShader(NULL);
		IDirect3DSurface9* backbuffer = 0;
		device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
		D3DSURFACE_DESC desc;
		backbuffer->GetDesc(&desc);
		if (!gamesurface)
		{
			device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &gametexture, 0);
			gametexture->GetSurfaceLevel(0, &gamesurface);

			device->CreateVertexBuffer(6 * sizeof(CUSTOMVERTEX),
				D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
				(D3DFVF_XYZRHW | D3DFVF_TEX1),
				D3DPOOL_DEFAULT,
				&v_buffer,
				NULL);
		}

		IDirect3DVertexDeclaration9* vertDec = NULL; IDirect3DVertexShader9* vertShader = NULL;
		device->GetVertexDeclaration(&vertDec);
		device->GetVertexShader(&vertShader);
		GetOriginValue(D3DRS_COLORWRITEENABLE);
		GetOriginValue(D3DRS_SRGBWRITEENABLE);
		GetOriginValue(D3DRS_MULTISAMPLEANTIALIAS);
		GetOriginValue(D3DRS_CULLMODE);
		GetOriginValue(D3DRS_LIGHTING);
		GetOriginValue(D3DRS_ZENABLE);
		GetOriginValue(D3DRS_ALPHABLENDENABLE);
		GetOriginValue(D3DRS_ALPHATESTENABLE);
		GetOriginValue(D3DRS_BLENDOP);
		GetOriginValue(D3DRS_SRCBLEND);
		GetOriginValue(D3DRS_DESTBLEND);
		GetOriginValue(D3DRS_SCISSORTESTENABLE);

		DWORD D3DSAMP_ADDRESSU_o;
		device->GetSamplerState(NULL, D3DSAMP_ADDRESSU, &D3DSAMP_ADDRESSU_o);
		DWORD D3DSAMP_ADDRESSV_o;
		device->GetSamplerState(NULL, D3DSAMP_ADDRESSV, &D3DSAMP_ADDRESSV_o);
		DWORD D3DSAMP_ADDRESSW_o;
		device->GetSamplerState(NULL, D3DSAMP_ADDRESSW, &D3DSAMP_ADDRESSW_o);
		DWORD D3DSAMP_SRGBTEXTURE_o;
		device->GetSamplerState(NULL, D3DSAMP_SRGBTEXTURE, &D3DSAMP_SRGBTEXTURE_o);
		DWORD D3DTSS_COLOROP_o;
		device->GetTextureStageState(0, D3DTSS_COLOROP, &D3DTSS_COLOROP_o);
		DWORD D3DTSS_COLORARG1_o;
		device->GetTextureStageState(0, D3DTSS_COLORARG1, &D3DTSS_COLORARG1_o);
		DWORD D3DTSS_COLORARG2_o;
		device->GetTextureStageState(0, D3DTSS_COLORARG2, &D3DTSS_COLORARG2_o);
		DWORD D3DTSS_ALPHAOP_o;
		device->GetTextureStageState(0, D3DTSS_ALPHAOP, &D3DTSS_ALPHAOP_o);
		DWORD D3DTSS_ALPHAARG1_o;
		device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &D3DTSS_ALPHAARG1_o);
		DWORD D3DTSS_ALPHAARG2_o;
		device->GetTextureStageState(0, D3DTSS_ALPHAARG2, &D3DTSS_ALPHAARG2_o);
		DWORD D3DSAMP_MINFILTER_o;
		device->GetSamplerState(NULL, D3DSAMP_MINFILTER, &D3DSAMP_MINFILTER_o);
		DWORD D3DSAMP_MAGFILTER_o;
		device->GetSamplerState(NULL, D3DSAMP_MAGFILTER, &D3DSAMP_MAGFILTER_o);
		DWORD o_FVF;
		device->GetFVF(&o_FVF);

		if (gamesurface)
		{
			device->SetRenderTarget(NULL, gamesurface);
			device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);

			CUSTOMVERTEX Va[6];
			int b = 0;
			Va[b] = { 0.f, 0.f, 0.0f, 1.0f, 0.f,0.f }; b++;
			Va[b] = { (float)desc.Width, 0.f , 0.0f, 1.0f,1.f,0.f }; b++;
			Va[b] = { 0.f,(float)desc.Height, 0.0f, 1.0f,0.f,1.f }; b++;
			Va[b] = { (float)desc.Width, 0.f, 0.0f, 1.0f,1.f,0.f }; b++;
			Va[b] = { (float)desc.Width,(float)desc.Height , 0.0f, 1.0f,1.f,1.f }; b++;
			Va[b] = { 0.f,(float)desc.Height, 0.0f, 1.0f,0.f,1.f };

			for (auto it = 0; it < 6; it++) {
				Va[it].x -= 0.5f;
				Va[it].y -= 0.5f;
			}
			VOID* pVoid;
			v_buffer->Lock(0, 0, (void**)&pVoid, D3DLOCK_DISCARD);
			memcpy(pVoid, Va, sizeof(Va));
			v_buffer->Unlock();
			device->SetPixelShader(NULL);
			device->SetVertexShader(NULL);
			device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			device->SetSamplerState(NULL, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
			device->SetSamplerState(NULL, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
			device->SetSamplerState(NULL, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
			device->SetSamplerState(NULL, D3DSAMP_SRGBTEXTURE, FALSE);
			device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_NONE);
			device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_NONE);
			device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCCOLOR);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);

			device->SetStreamSource(0, v_buffer, 0, sizeof(CUSTOMVERTEX));
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->SetFVF((D3DFVF_XYZRHW | D3DFVF_TEX1));
			device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
			device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
			device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_SUBTRACT);
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCCOLOR);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

			device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

			device->SetRenderTarget(NULL, backbuffer);

			device->SetPixelShader(NULL);
			device->SetVertexShader(NULL);
			device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			device->SetSamplerState(NULL, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
			device->SetSamplerState(NULL, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
			device->SetSamplerState(NULL, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
			device->SetSamplerState(NULL, D3DSAMP_SRGBTEXTURE, FALSE);
			device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_NONE);
			device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_NONE);
			device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCCOLOR);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
			device->SetRenderState(D3DRS_ZENABLE, false);
			device->SetRenderState(D3DRS_ZWRITEENABLE, false);
			device->SetRenderState(D3DRS_LIGHTING, false);
			device->SetTexture(0, gametexture);
			device->SetStreamSource(0, v_buffer, 0, sizeof(CUSTOMVERTEX));
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->SetFVF((D3DFVF_XYZRHW | D3DFVF_TEX1));
			device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

		}
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
		device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, false);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		device->SetRenderState(D3DRS_LIGHTING, false);
		device->SetRenderState(D3DRS_ZENABLE, false);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
		device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);

		// Render state fix end 

		if (globals::luaInit) {
			interfaces::surface->StartDrawing();

			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("EndSceneOverlay");

			lua->PushBool(inGameOverlay);
			// lua->PushNumber( 1 );

			lua->Call(2, 0);
			lua->Pop(2);

			interfaces::surface->FinishDrawing();
		}

		// Render state fix start 

		device->SetSamplerState(NULL, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
		device->SetSamplerState(NULL, D3DSAMP_SRGBTEXTURE, NULL);

		device->SetPixelShader(NULL);
		device->SetVertexShader(NULL);
		device->SetTexture(NULL, NULL);

		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

		device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
		device->SetFVF(o_FVF);
		SetOriginValue(D3DRS_COLORWRITEENABLE);
		SetOriginValue(D3DRS_SRGBWRITEENABLE);
		SetOriginValue(D3DRS_MULTISAMPLEANTIALIAS);
		SetOriginValue(D3DRS_CULLMODE);
		SetOriginValue(D3DRS_LIGHTING);
		SetOriginValue(D3DRS_ZENABLE);
		SetOriginValue(D3DRS_ALPHABLENDENABLE);
		SetOriginValue(D3DRS_ALPHATESTENABLE);
		SetOriginValue(D3DRS_BLENDOP);
		SetOriginValue(D3DRS_SRCBLEND);
		SetOriginValue(D3DRS_DESTBLEND);
		SetOriginValue(D3DRS_SCISSORTESTENABLE);

		device->SetSamplerState(NULL, D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSU_o);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSV, D3DSAMP_ADDRESSV_o);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSW, D3DSAMP_ADDRESSW_o);
		device->SetSamplerState(NULL, D3DSAMP_SRGBTEXTURE, D3DSAMP_SRGBTEXTURE_o);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DSAMP_MINFILTER_o);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DSAMP_MAGFILTER_o);

		device->SetVertexDeclaration(vertDec);
		device->SetVertexShader(vertShader);
		device->SetRenderTarget(NULL, gamesurface);
		device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
		device->SetRenderTarget(NULL, backbuffer);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, state1);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, state2);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, state3);

		pixel_state->Apply();
		pixel_state->Release();
		device->SetVertexDeclaration(vertex_declaration);
		device->SetVertexShader(vertex_shader);

		// Render state fix end

		return result;
	}

	/*
		Send net msg ???
	*/

	#define MAX_CMD_BUFFER 4000

	#define NUM_NEW_COMMAND_BITS		4
	#define MAX_NEW_COMMANDS			((1 << NUM_NEW_COMMAND_BITS) - 1)

	#define NUM_BACKUP_COMMAND_BITS		3
	#define MAX_BACKUP_COMMANDS			((1 << NUM_BACKUP_COMMAND_BITS) - 1)

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

		if (globals::luaInit) {
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


	/*
		Lua hooks
	*/


	auto lua_libraries = **(lua_library_holder_t***)(getAbsAddr(findPattern("client.dll", "48 89 1D ?? ?? ?? ?? 40 84 ED"))); // getAbsAddr(\x48\x89\x1d ? ? ? ? \x48\x89\x6c\x24
	auto lua_classes = **(lua_class_holder_t***)(getAbsAddr(findPattern("client.dll", "48 8B 05 ?? ?? ?? ?? 48 8D 7F 08")));
	int(*luaFuncOriginal)(lua_State*);

	bool HookLuaFunction(const char* table, const char* func_name, void* detour, void** original) {
		auto cur = lua_libraries->m_next->m_next;

		// check namespaces...

		for (size_t i = 0; i < lua_libraries->m_size && cur; i++, cur = cur->m_next) {
			auto lib = cur->m_library;

			if (!lib || !lib->m_table_name)
				continue;

			if (!strcmp(lib->m_table_name, table)) {

				for (int j = 0; j < lib->m_lib_funcs.size; j++) {
					auto func = lib->m_lib_funcs[j];

					if (!strcmp(func->m_func_name, func_name)) {

						return MH_CreateHook(func->m_handler, detour, original);
					}
				}

				return false;
			}
		}

		// the table name doesnt exist in any namespaces, check classes

		for (int i = 0; i < lua_classes->m_classes.size; i++) {
			auto lua_class = lua_classes->m_classes[i];

			if (!lua_class || !lua_class->m_class_name)
				continue;

			if (!strcmp(lua_class->m_class_name, table)) {

				for (int j = 0; j < lua_class->m_lib_funcs->size; j++) {
					auto func = lua_class->m_lib_funcs->memory[j];

					if (!strcmp(func->m_func_name, func_name)) {

						return MH_CreateHook(func->m_handler, detour, original);
					}
				}

				return false;
			}
		}

		return false;
	}



	int net_start_hook(lua_State* lua) {

		if (globals::luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");

			lua->PushString("luafunc");

			lua->PushNumber(1);

			lua->Call(2, 0);
			lua->Pop(2);
		}

		return luaFuncOriginal(lua);
	}


	/*
		Patterns
	*/

	auto SeqChangePattern = findPattern("client.dll", "48 85 D2 0F 84 D5 00 00 00 48 89 6C 24 10 48 89 74 24 18");
	auto CL_MovePattern = findPattern("engine.dll", "40 55 53 48 8D AC 24 38 F0 FF FF B8 C8 10 00 00 E8 ?? ?? ?? ?? 48 2B E0 0F 29 B4 24 B0 10 00 00");
	auto PacketStartPattern = findPattern("engine.dll", "48 8B 01 48 83 C4 20 5B 48 FF A0 ?? ?? ?? ?? 48 83 C4 20 5B C3 CC CC CC CC CC CC CC 89 91 ?? ?? ?? ??") + 28;

	/*
		Hook
	*/

	void hook() {
		globals::device = *reinterpret_cast<IDirect3DDevice9**>(getAbsAddr(findPattern("shaderapidx9.dll", "3D 7C 01 76 88 74 07 3D 0E 00 07 80 75 34 48 8B 0D") + 14));

		void* EndSceneT = vmt::get<void*>(globals::device, 42);
		void* SendNetMsgT = vmt::get<void*>(interfaces::engineClient->GetNetChannel(), 40);

		if (MH_Initialize() == MH_OK)
		{
			// MH_CreateHook(EndSceneT,			(LPVOID)&EndSceneHookFunc,						(LPVOID*)&EndSceneOriginal);
			
			

			MH_CreateHook(SendNetMsgT, (LPVOID)&SendNetMsgHookFunc, (LPVOID*)&SendNetMsgOriginal);

			MH_CreateHook((LPVOID)SeqChangePattern, (LPVOID)&CheckForSequenceChangeHookFunc, (LPVOID*)&SeqChangeOriginal);
			MH_CreateHook((LPVOID)CL_MovePattern, (LPVOID)&CLMoveHookFunc, (LPVOID*)&CLMoveOriginal);
			MH_CreateHook((LPVOID)PacketStartPattern, (LPVOID)&PacketStartHookFunc, (LPVOID*)&PacketStartOriginal);

			HookLuaFunction("render", "Capture", net_start_hook, (LPVOID*)&luaFuncOriginal);

























			MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
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
		MH_DisableHook(MH_ALL_HOOKS);
		MH_Uninitialize();
	}
}