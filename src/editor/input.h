#pragma once
#include "tl.h"
#include "current.h"

using namespace tl;

struct KeyState {
	tl::KeyState state;
	v2s start_position;
};

::KeyState key_state[256 + 3];

bool key_down  (u8 key) { return (key_state[key].state & KeyState_down    ) && in_bounds(key_state[key].start_position, current_viewport.aabb()); }
bool key_up    (u8 key) { return (key_state[key].state & KeyState_up      ) && in_bounds(key_state[key].start_position, current_viewport.aabb()); }
bool key_repeat(u8 key) { return (key_state[key].state & KeyState_repeated) && in_bounds(key_state[key].start_position, current_viewport.aabb()); }
bool key_held  (u8 key) { return (key_state[key].state & KeyState_held    ) && in_bounds(key_state[key].start_position, current_viewport.aabb()); }

bool mouse_down(u8 button) { return (key_state[256 + button].state & KeyState_down) && in_bounds(key_state[256 + button].start_position, current_viewport.aabb()); }
bool mouse_up  (u8 button) { return (key_state[256 + button].state & KeyState_up  ) && in_bounds(key_state[256 + button].start_position, current_viewport.aabb()); }
bool mouse_held(u8 button) { return (key_state[256 + button].state & KeyState_held) && in_bounds(key_state[256 + button].start_position, current_viewport.aabb()); }

bool input_is_locked;
v2s input_lock_mouse_position;

void lock_input() {
	input_is_locked = true;
	input_lock_mouse_position = current_mouse_position;
}
void unlock_input() {
	input_is_locked = false;
}
