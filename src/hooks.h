#pragma once

class CStudioHdr;
class matrix3x4_t;

enum ClientFrameStage_t
{
	FRAME_UNDEFINED = -1,						// (haven't run any frames yet)
	FRAME_START = 0,
	FRAME_NET_UPDATE_START = 1,					// A network packet is being recieved
	FRAME_NET_UPDATE_POSTDATAUPDATE_START = 2,	// Data has been received and we're going to start calling PostDataUpdate 
	FRAME_NET_UPDATE_POSTDATAUPDATE_END = 3,	// Data has been received and we've called PostDataUpdate on all data recipients
	FRAME_NET_UPDATE_END = 4,					// We've received all packets, we can now do interpolation, prediction, etc..
	FRAME_RENDER_START = 5,						// We're about to start rendering the scene
	FRAME_RENDER_END = 6 						// We've finished rendering the scene.
};

namespace detours {
	void hook();
	void postInit();
	void unHook();
} 