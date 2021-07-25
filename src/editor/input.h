#pragma once
#include "tl.h"
#include "current.h"

using namespace tl;

struct KeyState {
	tl::KeyState state;
	v2s start_position;
};

::KeyState key_state[256 + 3];

struct InputQueryParams {
	bool anywhere = false;
};

bool input_is_locked;
v2s input_lock_mouse_position;

bool key_down  (u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_down    ) && (params.anywhere || in_bounds(key_state[key].start_position, current_viewport.aabb())); }
bool key_up    (u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_up      ) && (params.anywhere || in_bounds(key_state[key].start_position, current_viewport.aabb())); }
bool key_repeat(u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_repeated) && (params.anywhere || in_bounds(key_state[key].start_position, current_viewport.aabb())); }
bool key_held  (u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_held    ) && (params.anywhere || in_bounds(key_state[key].start_position, current_viewport.aabb())); }

bool mouse_down_no_lock (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_down) && (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport.aabb())); }
bool mouse_up_no_lock   (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_up  ) && (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport.aabb())); }
bool mouse_click_no_lock(u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_up  ) && (params.anywhere || (in_bounds(current_mouse_position, current_viewport.aabb()) && in_bounds(key_state[256 + button].start_position, current_viewport.aabb()))); }
bool mouse_held_no_lock (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_held) && (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport.aabb())); }

bool mouse_down (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_down_no_lock (button, params); }
bool mouse_up   (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_up_no_lock   (button, params); }
bool mouse_click(u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_click_no_lock(button, params); }
bool mouse_held (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_held_no_lock (button, params); }

void lock_input() {
	input_is_locked = true;
	input_lock_mouse_position = current_mouse_position;
}
void unlock_input() {
	input_is_locked = false;
}
