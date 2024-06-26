# About
Cheat module for Garry's Mod (x64, Client-state)
Author: https://github.com/D3D9C

New module, now works in Menu-state!
https://github.com/D3D9C/sublime-x64

## Code example
```
require("zxcmodule")

// ConVar manipulation
ded.ConVarSetFlags( "mat_fullbright", 0 )

// Disable interp
ded.SetInterpolation(false)

// Disable sequence interp
ded.SetSequenceInterpolation(false)
```

### Lua API
ServerCmd  
ClientCmd  
SetViewAngles  
ExecuteClientCmd  
RawClientCmdUnrestricted  
ClientCmdUnrestricted  
SetRestrictServerCommands  
SetRestrictClientCommands  
GetGameDirectory  
GetLocalPlayer  
GetTime  
GetLastTimeStamp  
IsBoxVisible  
IsBoxInViewCluster  
IsOccluded  
GetLastCommandAck  
GetLastOutgoingCommand  
SetLastOutgoingCommand  
GetChokedCommands  
SetChokedCommands  
GetPreviousTick  
GetCurTime  
SetCurTime  
GetFrameTime  
SetFrameTime  
GetRealTime  
SetRealTime  
GetFrameCount  
SetFrameCount  
GetAbsFrameTime  
SetAbsFrameTime  
GetInterpoloationAmount  
SetInterpoloationAmount  
ConVarSetValue  
ConVarSetFlags  
SpoofConVar  
SpoofedConVarSetNumber  
SetCommandNumber  
SetCommandTick  
SetTyping  
SetContextVector  
GetRandomSeed  
SetRandomSeed  
PredictSpread  
GetServerTime  
StartPrediction  
FinishPrediction  
StartSimulation  
SimulateTick  
GetSimulationData  
FinishSimulation  
EditSimulationData  
SetBSendPacket  
SetInterpolation  
SetSequenceInterpolation  
EnableBoneFix  
EnableAnimFix  
AllowAnimationUpdate  
EnableTickbaseShifting  
StartShifting  
EnableSlowmotion  
StartRecharging  
SetMissedTicks  
SetMinShift  
SetMaxShift  
GetCurrentCharge  
GetIsShifting  
GetIsCharging  
GetClipboardText  
ExcludeFromCapture  
Read  
Write  
GetNetworkedVar  
SetEntityFlags  
UpdateAnimations  
UpdateClientAnimation  
SetCurrentLowerBodyYaw  
SetTargetLowerBodyYaw  
GetCurrentLowerBodyYaw  
GetTargetLowerBodyYaw  
GetSimulationTime  
GetNetName  
GetNetAdress  
GetNetTime  
GetNetTimeConnected  
GetNetBufferSize  
GetNetDataRate  
GetIsLoopback  
GetIsTimingOut  
SetOutSequenceNr  
SetTimeout  
NetShutdownStr  
SetMaxRoutablePayloadSize  
GetOutSequenceNr  
SetRemoteFramerate  
SetInterpolationAmount  
SetCompressionMode  
SetInSequenceNr  
SetChallengeNr  
SetDataRate  
GetPacketBytes  
GetInSequenceNr  
GetPacketTime  
IsValidPacket  
GetOutSequenceNrAck  
SetOutSequenceNrAck  
SetNetChokedPackets  
GetNetChokedPackets  
SetPacketDrop  
SetOutReliableState  
GetOutReliableState  
GetPacketDrop  
SetInReliableState  
GetInReliableState  
GetSequenceNrFlow  
GetTotalData  
GetAvgPackets  
GetAvgData  
GetAvgChoke  
GetAvgLoss  
GetAvgLatency  
GetLatency  
SendFile  
RequestFile  
NetDisconnect  
NetSetConVar  
PushSpecial  
