#pragma once
#include <t3d/common.h>

using KeyState = u8;

enum : KeyState {
	KeyState_none       = 0x0,
	KeyState_held       = 0x1,
	KeyState_down       = 0x2,
	KeyState_up         = 0x4,
	KeyState_repeated   = 0x8,
	KeyState_drag       = 0x10,
	KeyState_begin_drag = 0x20,
	KeyState_end_drag   = 0x40,
	KeyState_clicked    = 0x80,
};
