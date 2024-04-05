#pragma once

#include "minhook/minhook.h"

#include <deque>

class CStudioHdr;
class matrix3x4_t;

struct OutCommand {
	bool is_outgoing;
	bool is_used;
	int command_number;
	int previous_command_number;
};

extern std::deque<OutCommand> cmds;

namespace detour {
	void hook(); 
	void unHook();
}

