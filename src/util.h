#pragma once

#include <assert.h>

#include "vmt.h"
#include "patternscan.h"
#include "netvars.h"

#define VPROXY(methodName, methodIndex, retType, args, ...)					\
	retType methodName args noexcept { return vmt::call<retType>((void*)this, methodIndex, ## __VA_ARGS__); }

#define NETVAR_(type, tableName, propName, funcName)										\
	type& funcName() const noexcept {														\
		static const int offset = netvars::netvars[#tableName "->" #propName];				\
		assert(offset != NULL);																\
		return *reinterpret_cast<type*>(reinterpret_cast<std::uintptr_t>(this) + offset);	\
	}

#define NETVAR(type, tblname, propname) NETVAR_(type, tblname, propname, propname)
#define OFFSETVAR(type, funcName, offset) type& funcName() const noexcept { return *reinterpret_cast<type*>(reinterpret_cast<std::uintptr_t>(this) + offset); }

static void* getAbsAddr(std::uintptr_t inst, std::uintptr_t instOffset = 3, std::uintptr_t instSize = 7) {
	int offset = *reinterpret_cast<int*>(inst + instOffset);
	std::uintptr_t rip = inst + instSize;

	return reinterpret_cast<void*>(rip + static_cast<std::uintptr_t>(offset));
} 

#define MAX_CMD_BUFFER 4000

#define NUM_NEW_COMMAND_BITS		4
#define MAX_NEW_COMMANDS			((1 << NUM_NEW_COMMAND_BITS) - 1)

#define NUM_BACKUP_COMMAND_BITS		3
#define MAX_BACKUP_COMMANDS			((1 << NUM_BACKUP_COMMAND_BITS) - 1)