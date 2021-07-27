#pragma once
#include "tl.h"
#include "current.h"

using namespace tl;

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
};

struct KeyInputState {
	KeyState state;
	v2s start_position;
};

::KeyInputState key_state[256 + 3];

struct InputQueryParams {
	bool anywhere = false;
};

bool input_is_locked;
v2s input_lock_mouse_position;

List<utf8> input_string;

bool key_down  (u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_down    ) && (params.anywhere || in_bounds(key_state[key].start_position, current_viewport.aabb())); }
bool key_up    (u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_up      ) && (params.anywhere || in_bounds(key_state[key].start_position, current_viewport.aabb())); }
bool key_repeat(u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_repeated) && (params.anywhere || in_bounds(key_state[key].start_position, current_viewport.aabb())); }
bool key_held  (u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_held    ) && (params.anywhere || in_bounds(key_state[key].start_position, current_viewport.aabb())); }

bool mouse_down_no_lock       (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_down      ) && (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport.aabb())); }
bool mouse_up_no_lock         (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_up        ) && (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport.aabb())); }
bool mouse_click_no_lock      (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_up        ) && (params.anywhere || (in_bounds(current_mouse_position, current_viewport.aabb()) && in_bounds(key_state[256 + button].start_position, current_viewport.aabb()))); }
bool mouse_held_no_lock       (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_held      ) && (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport.aabb())); }
bool mouse_drag_no_lock       (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_drag      ) && (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport.aabb())); }
bool mouse_begin_drag_no_lock (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_begin_drag) && (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport.aabb())); }
bool mouse_end_drag_no_lock   (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_end_drag  ) && (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport.aabb())); }

bool mouse_down      (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_down_no_lock      (button, params); }
bool mouse_up        (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_up_no_lock        (button, params); }
bool mouse_click     (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_click_no_lock     (button, params); }
bool mouse_held      (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_held_no_lock      (button, params); }
bool mouse_drag      (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_drag_no_lock      (button, params); }
bool mouse_begin_drag(u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_begin_drag_no_lock(button, params); }
bool mouse_end_drag  (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_end_drag_no_lock  (button, params); }

void lock_input(v2s position = current_mouse_position) {
	input_is_locked = true;
	input_lock_mouse_position = position;
}
void unlock_input() {
	input_is_locked = false;
}
