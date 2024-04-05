
#include "globals.h"

namespace globals {
	IDirect3DDevice9* device;
		
	bool	bSendPacket					= true;

	bool	shouldInterpolate			= true;
	bool	shouldInterpolateSequences	= true;

	bool	shouldFixBones				= false;

	bool	shouldFixAnimations			= false;
	bool	updateAllowed				= false;
	int		updateTicks					= 1;

	bool	canShiftTickbase			= false;

	int		maxTickbaseShift			= 1;
	int		minTickbaseShift			= 1;
	int		curTickbaseCharge			= 0;

	bool	bIsShifting					= false;
	bool	bIsRecharging				= false;

	bool	bShouldShift				= false;
	bool	bShouldRecharge				= false;

	bool	bShouldSlowMo				= false;

	bool	luaInit						= false;
	bool	runTime						= false;

	int		currentStage				= 1;
}