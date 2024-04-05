
#pragma once

class INetChannelInfo;
class INetChannel;

#include "vmt.h"
#include "util.h"

#include "angle.h"

class IEngineClient {
public:
	VPROXY(ServerCmd, 6, void, (char const*, bool));
	VPROXY(ClientCmd, 7, void, (char const*));
	VPROXY(GetLocalPlayer, 12, int, (void));
	VPROXY(Time, 14, float, (void));
	VPROXY(GetLastTimeStamp, 15, float, (void));
	VPROXY(SetViewAngles, 20, void, (Angle viewAngles), viewAngles);
	VPROXY(IsBoxVisible, 31, bool, (Vector const&, Vector const&));
	VPROXY(IsBoxInViewCluster, 32, bool, (Vector const&, Vector const&));
	VPROXY(GetGameDirectory, 35, char const*, (void));
	VPROXY(IsOccluded, 69, bool, (Vector const&, Vector const&));
	VPROXY(GetNetChannelInfo, 72, INetChannelInfo*, (void));
	VPROXY(IsPlayingTimeDemo, 78, bool, (void));
	VPROXY(ExecuteClientCmd, 102, void, (char const*));
	VPROXY(ClientCmd_Unrestricted, 106, void, (char const*));
	VPROXY(SetRestrictServerCommands, 107, void, (bool));
	VPROXY(SetRestrictClientCommands, 108, void, (bool));
	VPROXY(GMOD_RawClientCmd_Unrestricted, 140, void, (char const*));
	 
	INetChannel* GetNetChannel() { return reinterpret_cast<INetChannel*>(GetNetChannelInfo()); }
}; 
 