
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <fstream>

#define GMOD_USE_SOURCESDK
#include "GarrysMod/Lua/Interface.h"
#include "GarrysMod/Lua/Types.h"
using namespace GarrysMod::Lua;

#include "vstdlib.h"
#include "md5.h"
#include "util.h"
#include "convar.h"
#include "spoofedconvar.h"
#include "cusercmd.h"
#include "prediction.h"
#include "simulation.h"
#include "engineclient.h"
#include "inetchannel.h"
#include "clientstate.h"
#include "globalvars.h"
#include "luashared.h"
#include "globals.h"
#include "interfaces.h"
#include "cliententitylist.h"
#include "entity.h"
 
#include "hooks.h"

std::vector<std::unique_ptr<SpoofedConVar>> spoofedConVars;

// Engine client 
LUA_FUNCTION(ServerCmd) {
	LUA->CheckString(1);
	LUA->CheckType(2, Type::Bool);

	interfaces::engineClient->ServerCmd(LUA->GetString(1), LUA->GetBool(2));

	return 0;
}

LUA_FUNCTION(ClientCmd) {
	LUA->CheckString(1);

	interfaces::engineClient->ClientCmd(LUA->GetString(1));

	return 0;
}

LUA_FUNCTION(SetViewAngles) {
	LUA->CheckType(1, Type::Angle);

	interfaces::engineClient->SetViewAngles(LUA->GetAngle(1));

	return 0;
}

LUA_FUNCTION(ExecuteClientCmd) {
	LUA->CheckString(1);

	interfaces::engineClient->ExecuteClientCmd(LUA->GetString(1));

	return 0;
}

LUA_FUNCTION(RawClientCmdUnrestricted) {
	LUA->CheckString(1);

	interfaces::engineClient->GMOD_RawClientCmd_Unrestricted(LUA->GetString(1));

	return 0;
}

LUA_FUNCTION(ClientCmdUnrestricted) {
	LUA->CheckString(1);

	interfaces::engineClient->ClientCmd_Unrestricted(LUA->GetString(1));

	return 0;
}

LUA_FUNCTION(SetRestrictServerCommands) {
	LUA->CheckType(1, Type::Bool);

	interfaces::engineClient->SetRestrictServerCommands(LUA->GetBool(1));

	return 0;
}

LUA_FUNCTION(SetRestrictClientCommands) {
	LUA->CheckType(1, Type::Bool);

	interfaces::engineClient->SetRestrictClientCommands(LUA->GetBool(1));

	return 0;
}

LUA_FUNCTION(GetGameDirectory) {
	LUA->PushString(interfaces::engineClient->GetGameDirectory());

	return 1;
}

LUA_FUNCTION(GetLocalPlayer) {
	LUA->PushNumber(interfaces::engineClient->GetLocalPlayer());

	return 1;
}

LUA_FUNCTION(GetTime) {
	LUA->PushNumber(interfaces::engineClient->Time());

	return 1;
}

LUA_FUNCTION(GetLastTimeStamp) {
	LUA->PushNumber(interfaces::engineClient->GetLastTimeStamp());

	return 1;
}

LUA_FUNCTION(IsBoxVisible) {
	LUA->CheckType(1, Type::Vector);
	LUA->CheckType(2, Type::Vector);

	LUA->PushBool(interfaces::engineClient->IsBoxVisible(LUA->GetVector(1), LUA->GetVector(2)));

	return 1;
}

LUA_FUNCTION(IsBoxInViewCluster) {
	LUA->CheckType(1, Type::Vector);
	LUA->CheckType(2, Type::Vector);

	LUA->PushBool(interfaces::engineClient->IsBoxInViewCluster(LUA->GetVector(1), LUA->GetVector(2)));

	return 1;
}

LUA_FUNCTION(IsOccluded) {
	LUA->CheckType(1, Type::Vector);
	LUA->CheckType(2, Type::Vector);

	LUA->PushBool(interfaces::engineClient->IsOccluded(LUA->GetVector(1), LUA->GetVector(2)));

	return 1;
}

// ClientState
LUA_FUNCTION_GETSET(LastCommandAck, Number, interfaces::clientState->last_command_ack);
LUA_FUNCTION_GETSET(LastOutgoingCommand, Number, interfaces::clientState->lastoutgoingcommand);
LUA_FUNCTION_GETSET(ChokedCommands, Number, interfaces::clientState->chokedcommands);
LUA_FUNCTION_GETTER(GetPreviousTick, Number, interfaces::clientState->oldtickcount);

// GlobalVars 
LUA_FUNCTION_GETSET(CurTime, Number, interfaces::globalVars->curtime);
LUA_FUNCTION_GETSET(FrameTime, Number, interfaces::globalVars->frametime);
LUA_FUNCTION_GETSET(RealTime, Number, interfaces::globalVars->realtime);
LUA_FUNCTION_GETSET(FrameCount, Number, interfaces::globalVars->framecount);
LUA_FUNCTION_GETSET(AbsFrameTime, Number, interfaces::globalVars->absoluteframetime);
LUA_FUNCTION_GETSET(InterpoloationAmount, Number, interfaces::globalVars->interpolation_amount);

// ConVar 
LUA_FUNCTION(ConVarSetValue) {
	LUA->CheckString(1);
	LUA->CheckNumber(2);

	auto* var = interfaces::cvar->FindVar(LUA->GetString(1));
	if (!var) return 1;

	var->SetValue(LUA->GetNumber(2));

	return 0;
}


LUA_FUNCTION(ConVarSetFlags) {
	LUA->CheckString(1);
	LUA->CheckNumber(2);

	auto* var = interfaces::cvar->FindVar(LUA->GetString(1));
	if (!var) return 1;

	var->SetFlags(LUA->GetNumber(2));

	return 0;
}

LUA_FUNCTION(SpoofConVar) {
	LUA->CheckString(1);

	auto conVarName = LUA->GetString(1);

	for (auto& spoofedConVar : spoofedConVars) {
		if (strcmp(spoofedConVar->m_szOriginalName, conVarName) == 0)
			return true;
	}

	auto* var = interfaces::cvar->FindVar(conVarName);
	if (!var) {
		LUA->PushBool(false);
		return 1;
	}

	auto& spoofedConVar = spoofedConVars.emplace_back(std::make_unique<SpoofedConVar>(var));
	spoofedConVar->m_pOriginalCVar->DisableCallback();

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION(SpoofedConVarSetNumber) {
	LUA->CheckString(1);
	LUA->CheckNumber(2);

	auto conVarName = LUA->GetString(1);
	const auto& it = std::find_if(spoofedConVars.begin(), spoofedConVars.end(), [=](const auto& spoofedConVar) {
		return strcmp(spoofedConVar->m_szOriginalName, conVarName) == 0;
		});

	if (it == spoofedConVars.end()) {
		LUA->PushBool(false);
		return 1;
	}

	auto& spoofedConVar = *it;

	spoofedConVar->m_pOriginalCVar->SetValue(LUA->CheckNumber(2));

	LUA->PushBool(true);
	return 1;
}

// CUserCmd 
LUA_FUNCTION(SetCommandNumber) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckNumber(2);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	cmd->command_number = LUA->GetNumber(2);

	return 0;
}

LUA_FUNCTION(SetCommandTick) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckNumber(2);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	cmd->tick_count = LUA->GetNumber(2);

	return 0;
}

LUA_FUNCTION(SetTyping) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckType(2, Type::Bool);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	cmd->istyping = LUA->GetBool(2);
	cmd->unk = LUA->GetBool(2);

	return 0;
}

LUA_FUNCTION(SetContextVector) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckType(2, Type::Vector);
	LUA->CheckType(3, Type::Bool);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	cmd->context_menu = LUA->GetBool(3);
	cmd->context_normal = LUA->GetVector(2);

	return 0;
}

LUA_FUNCTION(GetRandomSeed) {
	LUA->CheckType(1, Type::UserCmd);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);

	uint8_t seed;
	{
		Chocobo1::MD5 md5;
		md5.addData(&cmd->command_number, sizeof(cmd->command_number));
		md5.finalize();

		seed = *reinterpret_cast<uint32_t*>(md5.toArray().data() + 6) & 0xFF;
	}

	LUA->PushNumber(seed);

	return 1;
}

LUA_FUNCTION(SetRandomSeed) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckNumber(2);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	int nSeed = LUA->GetNumber(2);
	int num = 0;

	uint8_t seed;
	{
		Chocobo1::MD5 md5;
		md5.addData(&nSeed, sizeof(nSeed));
		md5.finalize();

		seed = *reinterpret_cast<uint32_t*>(md5.toArray().data() + 6) & 0xFF;
	}

	for (int i = cmd->command_number + 1; !num; i++)
	{
		uint8_t uSeed;
		{
			Chocobo1::MD5 md5;
			md5.addData(&i, sizeof(i));
			md5.finalize();

			uSeed = *reinterpret_cast<uint32_t*>(md5.toArray().data() + 6) & 0xFF;
		}

		if (!uSeed)
			continue;

		if ((uSeed & 255) != (seed & 255))
			continue;

		num = i;

	}

	cmd->command_number = num;

	return 0;
}

LUA_FUNCTION(PredictSpread) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckType(2, Type::Angle);
	LUA->CheckType(3, Type::Vector);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	Angle angle = LUA->GetAngle(2);
	Vector vector = LUA->GetVector(3);

	uint8_t seed;
	{
		Chocobo1::MD5 md5;
		md5.addData(&cmd->command_number, sizeof(cmd->command_number));
		md5.finalize();

		seed = *reinterpret_cast<uint32_t*>(md5.toArray().data() + 6) & 0xFF;
	}

	vstdlib::RandomSeed(seed);

	float rand_x = vstdlib::RandomFloat(-0.5, 0.5) + vstdlib::RandomFloat(-0.5, 0.5);
	float rand_y = vstdlib::RandomFloat(-0.5, 0.5) + vstdlib::RandomFloat(-0.5, 0.5);

	Vector spreadDir = Vector(1.f, -vector.x * rand_x, vector.y * rand_y);

	LUA->PushVector(spreadDir);

	return 1;
}

// Prediction 
LUA_FUNCTION(GetServerTime) {
	LUA->CheckType(1, Type::UserCmd);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	LUA->PushNumber(g_prediction.GetServerTime(cmd));

	return 1;
}

LUA_FUNCTION(StartPrediction) {
	LUA->CheckType(1, Type::UserCmd);

	g_prediction.Start(LUA->GetUserType<CUserCmd>(1, Type::UserCmd));

	return 0;
}

LUA_FUNCTION(FinishPrediction) {
	g_prediction.Finish();

	return 0;
}

// Simulation 
LUA_FUNCTION(StartSimulation) {
	LUA->CheckNumber(1);

	CBasePlayer* ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	g_simulation.Start(ply);

	return 0;
}

LUA_FUNCTION(SimulateTick) {
	g_simulation.SimulateTick();

	return 0;
}

LUA_FUNCTION(GetSimulationData) {
	LUA->CreateTable();

	const auto& moveData = g_simulation.GetMoveData();

	LUA->PushAngle(moveData.m_vecViewAngles);
	LUA->SetField(-2, "m_vecViewAngles");

	LUA->PushVector(moveData.m_vecVelocity);
	LUA->SetField(-2, "m_vecVelocity");

	LUA->PushAngle(moveData.m_vecAngles);
	LUA->SetField(-2, "m_vecAngles");

	LUA->PushVector(moveData.m_vecAbsOrigin);
	LUA->SetField(-2, "m_vecAbsOrigin");

	return 1;
}

LUA_FUNCTION(FinishSimulation) {
	g_simulation.Finish();

	return 0;
}

LUA_FUNCTION(EditSimulationData) {
	LUA->CheckType(1, Type::Table);

	auto fieldExists = [&](const char* name) -> bool {
		LUA->GetField(1, name);
		bool isNil = LUA->IsType(-1, Type::Nil);
		LUA->Pop();
		return !isNil;
		};

	auto& moveData = g_simulation.GetMoveData();
	if (fieldExists("m_nButtons")) {
		LUA->GetField(1, "m_nButtons");
		moveData.m_nButtons = LUA->GetNumber();
	}

	if (fieldExists("m_nOldButtons")) {
		LUA->GetField(1, "m_nOldButtons");
		moveData.m_nOldButtons = LUA->GetNumber();
	}

	if (fieldExists("m_flForwardMove")) {
		LUA->GetField(1, "m_flForwardMove");
		moveData.m_flForwardMove = LUA->GetNumber();
	}

	if (fieldExists("m_flSideMove")) {
		LUA->GetField(1, "m_flSideMove");
		moveData.m_flSideMove = LUA->GetNumber();
	}

	if (fieldExists("m_vecViewAngles")) {
		LUA->GetField(1, "m_vecViewAngles");
		moveData.m_vecViewAngles = *LUA->GetUserType<Angle>(-1, Type::Angle);
	}

	return 0;
}

// Globals 
LUA_FUNCTION_BSETTER(SetBSendPacket, globals::bSendPacket);
LUA_FUNCTION_BSETTER(SetInterpolation, globals::shouldInterpolate);
LUA_FUNCTION_BSETTER(SetSequenceInterpolation, globals::shouldInterpolateSequences);
LUA_FUNCTION_BSETTER(EnableBoneFix, globals::shouldFixBones);
LUA_FUNCTION_BSETTER(EnableAnimFix, globals::shouldFixAnimations);
LUA_FUNCTION_BSETTER(AllowAnimationUpdate, globals::updateAllowed);
LUA_FUNCTION_BSETTER(EnableTickbaseShifting, globals::canShiftTickbase);
LUA_FUNCTION_BSETTER(StartShifting, globals::bShouldShift);
LUA_FUNCTION_BSETTER(EnableSlowmotion, globals::bShouldSlowMo);
LUA_FUNCTION_BSETTER(StartRecharging, globals::bShouldRecharge);

LUA_FUNCTION_SETTER(SetMissedTicks, Number, globals::updateTicks);
LUA_FUNCTION_SETTER(SetMaxShift, Number, globals::maxTickbaseShift);
LUA_FUNCTION_SETTER(SetMinShift, Number, globals::minTickbaseShift);

LUA_FUNCTION_GETTER(GetCurrentCharge, Number, globals::curTickbaseCharge);
LUA_FUNCTION_GETTER(GetIsShifting, Bool, globals::bIsShifting);
LUA_FUNCTION_GETTER(GetIsCharging, Bool, globals::bIsRecharging);

// Win API
LUA_FUNCTION(GetClipboardText) {
	if (!OpenClipboard(nullptr)) {
		LUA->PushBool(false);
		return 1;
	}

	HANDLE clipboardHandle = GetClipboardData(CF_TEXT);
	if (clipboardHandle == nullptr) {
		CloseClipboard();
		LUA->PushBool(false);
		return 1;
	}

	char* clipboardText = static_cast<char*>(GlobalLock(clipboardHandle));
	if (clipboardText == nullptr) {
		CloseClipboard();
		LUA->PushBool(false);
		return 1;
	}

	LUA->PushString(clipboardText);

	GlobalUnlock(clipboardHandle);
	CloseClipboard();

	return 1;
}

LUA_FUNCTION(ExcludeFromCapture) {
	LUA->CheckType(1, Type::Bool);

	HWND hWnd = FindWindowA("Valve001", nullptr);

	if (LUA->GetBool(1)) {
		SetWindowDisplayAffinity(hWnd, WDA_EXCLUDEFROMCAPTURE);
	}
	else
	{
		SetWindowDisplayAffinity(hWnd, !WDA_EXCLUDEFROMCAPTURE);
	}

	return 1;
}

// File 
LUA_FUNCTION(Read) {
	LUA->CheckString(1);
	
	const char* path = LUA->GetString();
	std::ifstream file;
	file.open(path, std::ios_base::binary | std::ios_base::ate);
	if (!file.good()) {
		LUA->PushBool(false);
		return 1;
	}

	auto fileSize = file.tellg();

	// Prevent heap corruption
	if (fileSize == 0) {
		LUA->PushBool(true);
		LUA->PushString("");
		return 2;
	}

	char* buffer = new char[fileSize] {0};
	file.seekg(std::ios::beg);
	file.read(buffer, fileSize);
	file.close();

	LUA->PushBool(true);
	LUA->PushString(buffer, fileSize);

	delete[] buffer;

	return 2;
}

LUA_FUNCTION(Write) {
	LUA->CheckString(1);
	LUA->CheckString(2);

	const char* path = LUA->GetString(1);
	std::ofstream file;
	file.open(path, std::ios_base::binary);
	if (!file.good()) {
		LUA->PushBool(false);
		return 1;
	}

	unsigned int dataSize = 0;
	const char* data = LUA->GetString(2, &dataSize);
	file.write(data, dataSize);
	file.close();

	LUA->PushBool(true);

	return 1;
}


// NetChannel
LUA_FUNCTION(NetSetConVar) {
	LUA->CheckString(1);
	LUA->CheckString(2);

	const char* conVar = LUA->CheckString(1);
	const char* value = LUA->CheckString(2);

	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	uint8_t msgBuf[1024];
	NetMessageWriteable netMsg(NetMessage::net_SetConVar, msgBuf, sizeof(msgBuf));
	netMsg.write.WriteUInt(static_cast<uint32_t>(NetMessage::net_SetConVar), NET_MESSAGE_BITS);
	netMsg.write.WriteByte(1);
	netMsg.write.WriteString(conVar);
	netMsg.write.WriteString(value);

	netChan->SendNetMsg(netMsg, true);

	return 1;
}

LUA_FUNCTION(NetDisconnect) {
	LUA->CheckString(1);

	const char* str = LUA->CheckString(1);

	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	uint8_t msgBuf[1024];
	 
	NetMessageWriteable netMsg(NetMessage::net_Disconnect, msgBuf, sizeof(msgBuf));
	netMsg.write.WriteUInt(static_cast<uint32_t>(NetMessage::net_Disconnect), NET_MESSAGE_BITS);
	netMsg.write.WriteByte(1); 
	netMsg.write.WriteString(str);

	netChan->SendNetMsg(netMsg, true);

	return 1;
}

LUA_FUNCTION(RequestFile) {
	LUA->CheckString(1);

	const char* str = LUA->CheckString(1);
	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	netChan->RequestFile(str);

	return 1;
} 

LUA_FUNCTION(SendFile) {
	LUA->CheckString(1);
	LUA->CheckNumber(2);

	const char* str = LUA->CheckString(1);
	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	netChan->SendFile(str, LUA->GetNumber(2));

	return 1;
}

LUA_FUNCTION(GetLatency) {
	LUA->CheckNumber(1);

	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	LUA->PushNumber(netChan->GetLatency(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetAvgLatency) {
	LUA->CheckNumber(1);

	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	LUA->PushNumber(netChan->GetAvgLatency(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetAvgLoss) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetAvgLoss(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetAvgChoke) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetAvgChoke(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetAvgData) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetAvgData(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetAvgPackets) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetAvgPackets(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetTotalData) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetTotalData(LUA->GetNumber(1)));

	return 1;
} 

LUA_FUNCTION(GetSequenceNrFlow) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetSequenceNr(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(IsValidPacket) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	LUA->PushBool(interfaces::engineClient->GetNetChannel()->IsValidPacket(LUA->GetNumber(1), LUA->GetNumber(2)));

	return 1;
}

LUA_FUNCTION(GetPacketTime) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetPacketTime(LUA->GetNumber(1), LUA->GetNumber(2)));

	return 1;
}

LUA_FUNCTION(GetPacketBytes) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);
	LUA->CheckNumber(3);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetPacketBytes(LUA->GetNumber(1), LUA->GetNumber(2), LUA->GetNumber(3)));

	return 1;
}

LUA_FUNCTION(SetDataRate) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetDataRate(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION(SetChallengeNr) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetChallengeNr(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION(SetCompressionMode) {
	LUA->CheckType(1, Type::Bool);

	interfaces::engineClient->GetNetChannel()->SetCompressionMode(LUA->GetBool(1));

	return 0;
}

LUA_FUNCTION(SetInterpolationAmount) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetInterpolationAmount(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION(SetRemoteFramerate) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	interfaces::engineClient->GetNetChannel()->SetRemoteFramerate(LUA->GetNumber(1), LUA->GetNumber(2));

	return 1;
}

LUA_FUNCTION(SetMaxRoutablePayloadSize) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetMaxRoutablePayloadSize(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION(NetShutdownStr) {
	LUA->CheckString(1);

	interfaces::engineClient->GetNetChannel()->Shutdown(LUA->GetString(1));

	return 0;
}

LUA_FUNCTION(SetTimeout) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetTimeout(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION_GETTER(GetNetName, String, interfaces::engineClient->GetNetChannel()->GetName())
LUA_FUNCTION_GETTER(GetNetAdress, String, interfaces::engineClient->GetNetChannel()->GetAddress())

LUA_FUNCTION_GETTER(GetNetTime, Number, interfaces::engineClient->GetNetChannel()->GetTime())
LUA_FUNCTION_GETTER(GetNetTimeConnected, Number, interfaces::engineClient->GetNetChannel()->GetTimeConnected())
LUA_FUNCTION_GETTER(GetNetBufferSize, Number, interfaces::engineClient->GetNetChannel()->GetBufferSize())
LUA_FUNCTION_GETTER(GetNetDataRate, Number, interfaces::engineClient->GetNetChannel()->GetDataRate())

LUA_FUNCTION_GETTER(GetIsLoopback, Bool, interfaces::engineClient->GetNetChannel()->IsLoopback())
LUA_FUNCTION_GETTER(GetIsTimingOut, Bool, interfaces::engineClient->GetNetChannel()->IsTimingOut())

LUA_FUNCTION_GETSET(OutSequenceNr, Number, interfaces::engineClient->GetNetChannel()->m_nOutSequenceNr);
LUA_FUNCTION_GETSET(InSequenceNr, Number, interfaces::engineClient->GetNetChannel()->m_nInSequenceNr);
LUA_FUNCTION_GETSET(OutSequenceNrAck, Number, interfaces::engineClient->GetNetChannel()->m_nOutSequenceNrAck);
LUA_FUNCTION_GETSET(NetChokedPackets, Number, interfaces::engineClient->GetNetChannel()->m_nChokedPackets);
LUA_FUNCTION_GETSET(PacketDrop, Number, interfaces::engineClient->GetNetChannel()->m_PacketDrop);
LUA_FUNCTION_GETSET(OutReliableState, Number, interfaces::engineClient->GetNetChannel()->m_nOutReliableState);
LUA_FUNCTION_GETSET(InReliableState, Number, interfaces::engineClient->GetNetChannel()->m_nInReliableState);

// Entity 
LUA_FUNCTION(GetNetworkedVar) {
	LUA->CheckNumber(1);
	LUA->CheckString(2);
	LUA->CheckString(3);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));

	int offset = netvars::netvars[LUA->GetString(1), "->", LUA->GetString(2)];
	auto ret = *reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(Ply) + offset);

	LUA->PushNumber(ret);

	return 1;
}

LUA_FUNCTION(GetSimulationTime) {
	LUA->CheckNumber(1);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	
	LUA->PushNumber(Ply->m_flSimulationTime());

	return 1;
}

LUA_FUNCTION(GetTargetLowerBodyYaw) { 
	LUA->CheckNumber(1);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	LUA->PushNumber(animState->m_flGoalFeetYaw);

	return 1;
}

LUA_FUNCTION(GetCurrentLowerBodyYaw) {
	LUA->CheckNumber(1);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	LUA->PushNumber(animState->m_flCurrentFeetYaw);

	return 1;
}

LUA_FUNCTION(SetTargetLowerBodyYaw) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	animState->m_flGoalFeetYaw = LUA->GetNumber(2);

	return 0;
}

LUA_FUNCTION(SetCurrentLowerBodyYaw) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	animState->m_flCurrentFeetYaw = LUA->GetNumber(2);

	return 0;
}

LUA_FUNCTION(UpdateClientAnimation) {
	LUA->CheckNumber(1);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>( interfaces::entityList->GetClientEntity( LUA->GetNumber(1) ) );
	Ply->UpdateClientsideAnimation();

	return 0;
}

LUA_FUNCTION(UpdateAnimations) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);
	LUA->CheckNumber(3);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	animState->Update( LUA->GetNumber(2), LUA->GetNumber(3) );

	return 0;
}

LUA_FUNCTION(SetEntityFlags) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>( interfaces::entityList->GetClientEntity( LUA->GetNumber(1) ) );
	Ply->m_fFlags() = static_cast<int>( LUA->GetNumber(2) );

	return 0;
}

// Legacy 

LUA_FUNCTION(PushSpecial) {
	LUA->CheckNumber(1);

	LUA->PushSpecial(LUA->GetNumber(1));

	return 1;
}

// Api

ILuaBase* luaBase;
auto PushApiFunction = [&](const char* name, CFunc func) {
	luaBase->PushCFunction(func);
	luaBase->SetField(-2, name);
};

GMOD_MODULE_OPEN() {
	 
	luaBase = LUA;

	interfaces::init();
	netvars::init();
	detours::hook();
	detours::postInit();

	LUA->PushSpecial(SPECIAL_GLOB);
	LUA->CreateTable();
		PushApiFunction("ServerCmd", ServerCmd);
		PushApiFunction("ClientCmd", ClientCmd);
		PushApiFunction("SetViewAngles", SetViewAngles);
		PushApiFunction("ExecuteClientCmd", ExecuteClientCmd);
		PushApiFunction("RawClientCmdUnrestricted", RawClientCmdUnrestricted);
		PushApiFunction("ClientCmdUnrestricted", ClientCmdUnrestricted);
		PushApiFunction("SetRestrictServerCommands", SetRestrictServerCommands);
		PushApiFunction("SetRestrictClientCommands", SetRestrictClientCommands);
		PushApiFunction("GetGameDirectory", GetGameDirectory);
		PushApiFunction("GetLocalPlayer", GetLocalPlayer);
		PushApiFunction("GetTime", GetTime);
		PushApiFunction("GetLastTimeStamp", GetLastTimeStamp);
		PushApiFunction("IsBoxVisible", IsBoxVisible);
		PushApiFunction("IsBoxInViewCluster", IsBoxInViewCluster);
		PushApiFunction("IsOccluded", IsOccluded);

		PushApiFunction("GetLastCommandAck", GetLastCommandAck);
		PushApiFunction("GetLastOutgoingCommand", GetLastOutgoingCommand);
		PushApiFunction("SetLastOutgoingCommand", SetLastOutgoingCommand);
		PushApiFunction("GetChokedCommands", GetChokedCommands);
		PushApiFunction("SetChokedCommands", SetChokedCommands);
		PushApiFunction("GetPreviousTick", GetPreviousTick);

		PushApiFunction("GetCurTime", GetCurTime);
		PushApiFunction("SetCurTime", SetCurTime);
		PushApiFunction("GetFrameTime", GetFrameTime);
		PushApiFunction("SetFrameTime", SetFrameTime);
		PushApiFunction("GetRealTime", GetRealTime);
		PushApiFunction("SetRealTime", SetRealTime);
		PushApiFunction("GetFrameCount", GetFrameCount);
		PushApiFunction("SetFrameCount", SetFrameCount);
		PushApiFunction("GetAbsFrameTime", GetAbsFrameTime);
		PushApiFunction("SetAbsFrameTime", SetAbsFrameTime);
		PushApiFunction("GetInterpoloationAmount", GetInterpoloationAmount);
		PushApiFunction("SetInterpoloationAmount", SetInterpoloationAmount);

		PushApiFunction("ConVarSetValue", ConVarSetValue);
		PushApiFunction("ConVarSetFlags", ConVarSetFlags);
		PushApiFunction("SpoofConVar", SpoofConVar);
		PushApiFunction("SpoofedConVarSetNumber", SpoofedConVarSetNumber);

		PushApiFunction("SetCommandNumber", SetCommandNumber);
		PushApiFunction("SetCommandTick", SetCommandTick);
		PushApiFunction("SetTyping", SetTyping);
		PushApiFunction("SetContextVector", SetContextVector);
		PushApiFunction("GetRandomSeed", GetRandomSeed);
		PushApiFunction("SetRandomSeed", SetRandomSeed);
		PushApiFunction("PredictSpread", PredictSpread);

		PushApiFunction("GetServerTime", GetServerTime);
		PushApiFunction("StartPrediction", StartPrediction);
		PushApiFunction("FinishPrediction", FinishPrediction);

		PushApiFunction("StartSimulation", StartSimulation);
		PushApiFunction("SimulateTick", SimulateTick);
		PushApiFunction("GetSimulationData", GetSimulationData);
		PushApiFunction("FinishSimulation", FinishSimulation);
		PushApiFunction("EditSimulationData", EditSimulationData);

		PushApiFunction("SetBSendPacket", SetBSendPacket);
		PushApiFunction("SetInterpolation", SetInterpolation);
		PushApiFunction("SetSequenceInterpolation", SetSequenceInterpolation);
		PushApiFunction("EnableBoneFix", EnableBoneFix);
		PushApiFunction("EnableAnimFix", EnableAnimFix);
		PushApiFunction("AllowAnimationUpdate", AllowAnimationUpdate);
		PushApiFunction("EnableTickbaseShifting", EnableTickbaseShifting);
		PushApiFunction("StartShifting", StartShifting);
		PushApiFunction("EnableSlowmotion", EnableSlowmotion);
		PushApiFunction("StartRecharging", StartRecharging);
		PushApiFunction("SetMissedTicks", SetMissedTicks);
		PushApiFunction("SetMinShift", SetMinShift);
		PushApiFunction("SetMaxShift", SetMaxShift);
		PushApiFunction("GetCurrentCharge", GetCurrentCharge);
		PushApiFunction("GetIsShifting", GetIsShifting);
		PushApiFunction("GetIsCharging", GetIsCharging);

		PushApiFunction("GetClipboardText", GetClipboardText);
		PushApiFunction("ExcludeFromCapture", ExcludeFromCapture);

		PushApiFunction("Read", Read);
		PushApiFunction("Write", Write);

		PushApiFunction("GetNetworkedVar", GetNetworkedVar);
		PushApiFunction("SetEntityFlags", SetEntityFlags);
		PushApiFunction("UpdateAnimations", UpdateAnimations);
		PushApiFunction("UpdateClientAnimation", UpdateClientAnimation);
		PushApiFunction("SetCurrentLowerBodyYaw", SetCurrentLowerBodyYaw);
		PushApiFunction("SetTargetLowerBodyYaw", SetTargetLowerBodyYaw);
		PushApiFunction("GetCurrentLowerBodyYaw", GetCurrentLowerBodyYaw);
		PushApiFunction("GetTargetLowerBodyYaw", GetTargetLowerBodyYaw);
		PushApiFunction("GetSimulationTime", GetSimulationTime);

		PushApiFunction("GetNetName", GetNetName);
		PushApiFunction("GetNetAdress", GetNetAdress);
		PushApiFunction("GetNetTime", GetNetTime);
		PushApiFunction("GetNetTimeConnected", GetNetTimeConnected);
		PushApiFunction("GetNetBufferSize", GetNetBufferSize);
		PushApiFunction("GetNetDataRate", GetNetDataRate);
		PushApiFunction("GetIsLoopback", GetIsLoopback);
		PushApiFunction("GetIsTimingOut", GetIsTimingOut);
		PushApiFunction("SetOutSequenceNr", SetOutSequenceNr);
		PushApiFunction("SetTimeout", SetTimeout);
		PushApiFunction("NetShutdownStr", NetShutdownStr);
		PushApiFunction("SetMaxRoutablePayloadSize", SetMaxRoutablePayloadSize);
		PushApiFunction("GetOutSequenceNr", GetOutSequenceNr);
		PushApiFunction("SetRemoteFramerate", SetRemoteFramerate);
		PushApiFunction("SetInterpolationAmount", SetInterpolationAmount);
		PushApiFunction("SetCompressionMode", SetCompressionMode);
		PushApiFunction("SetInSequenceNr", SetInSequenceNr);
		PushApiFunction("SetChallengeNr", SetChallengeNr);
		PushApiFunction("SetDataRate", SetDataRate);
		PushApiFunction("GetPacketBytes", GetPacketBytes);
		PushApiFunction("GetInSequenceNr", GetInSequenceNr);
		PushApiFunction("GetPacketTime", GetPacketTime);
		PushApiFunction("IsValidPacket", IsValidPacket);
		PushApiFunction("GetOutSequenceNrAck", GetOutSequenceNrAck);
		PushApiFunction("SetOutSequenceNrAck", SetOutSequenceNrAck);
		PushApiFunction("SetNetChokedPackets", SetNetChokedPackets);
		PushApiFunction("GetNetChokedPackets", GetNetChokedPackets);
		PushApiFunction("SetPacketDrop", SetPacketDrop);
		PushApiFunction("SetOutReliableState", SetOutReliableState);
		PushApiFunction("GetOutReliableState", GetOutReliableState);
		PushApiFunction("GetPacketDrop", GetPacketDrop);
		PushApiFunction("SetInReliableState", SetInReliableState);
		PushApiFunction("GetInReliableState", GetInReliableState);
		PushApiFunction("GetSequenceNrFlow", GetSequenceNrFlow);
		PushApiFunction("GetTotalData", GetTotalData);
		PushApiFunction("GetAvgPackets", GetAvgPackets);
		PushApiFunction("GetAvgData", GetAvgData);
		PushApiFunction("GetAvgChoke", GetAvgChoke);
		PushApiFunction("GetAvgLoss", GetAvgLoss);
		PushApiFunction("GetAvgLatency", GetAvgLatency);
		PushApiFunction("GetLatency", GetLatency);
		PushApiFunction("SendFile", SendFile);
		PushApiFunction("RequestFile", RequestFile);
		PushApiFunction("NetDisconnect", NetDisconnect);
		PushApiFunction("NetSetConVar", NetSetConVar);	

		PushApiFunction("PushSpecial", PushSpecial);
	LUA->SetField(-2, "ded");

	return 0;
}

GMOD_MODULE_CLOSE() {
	detours::unHook();
	spoofedConVars.clear();

	return 0;
}

