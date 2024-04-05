
#pragma once

#include "d3d9.h"

namespace globals {
	extern IDirect3DDevice9* device;

	extern bool		bSendPacket;

	extern bool		shouldInterpolate;
	extern bool		shouldInterpolateSequences;

	extern bool		shouldFixBones;

	extern bool		shouldFixAnimations;
	extern bool		updateAllowed;
	extern int		updateTicks;

	extern bool		canShiftTickbase;

	extern int		maxTickbaseShift;
	extern int		minTickbaseShift;
	extern int		curTickbaseCharge;

	extern bool		bShouldSlowMo;

	extern bool		bIsShifting;
	extern bool		bIsRecharging;

	extern bool		bShouldShift;
	extern bool		bShouldRecharge;

	extern bool		luaInit;
	extern bool		runTime;

	extern int		currentStage;
}
 