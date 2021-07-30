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
	KeyState_clicked    = 0x80,
};

struct KeyInputState {
	KeyState state;
	v2s start_position;
};

::KeyInputState key_state[256 + 3];

struct InputQueryParams {
	bool anywhere = false;
	bool invert   = false;
};

bool input_is_locked;
v2s input_lock_mouse_position;

List<utf8> input_string;

u32 input_user_index;
u32 focusable_input_user_index;
u32 should_switch_focus_to = -1;
u32 input_locker;

bool drag_and_dropping;
List<utf8> drag_and_drop_path;

void begin_input_user(bool focusable = false) {
	input_user_index += 1;
	if (focusable) {
		focusable_input_user_index += 1;
	}
}

bool key_down  (u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_down    ) && (params.invert != (params.anywhere || in_bounds(key_state[key].start_position, current_viewport))); }
bool key_up    (u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_up      ) && (params.invert != (params.anywhere || in_bounds(key_state[key].start_position, current_viewport))); }
bool key_repeat(u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_repeated) && (params.invert != (params.anywhere || in_bounds(key_state[key].start_position, current_viewport))); }
bool key_held  (u8 key, InputQueryParams params = {}) { return (key_state[key].state & KeyState_held    ) && (params.invert != (params.anywhere || in_bounds(key_state[key].start_position, current_viewport))); }

bool mouse_down_no_lock       (u8 button, InputQueryParams params = {}) {
	bool was_in_bounds = in_bounds(key_state[256 + button].start_position, current_viewport);
	bool state = key_state[256 + button].state & KeyState_down;
	return state && (params.invert != (params.anywhere || was_in_bounds));
}
bool mouse_up_no_lock         (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_up        ) && (params.invert != (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport))); }
bool mouse_click_no_lock      (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_up && !(key_state[256 + button].state & KeyState_clicked)) && (params.anywhere || (in_bounds(current_mouse_position, current_viewport) && in_bounds(key_state[256 + button].start_position, current_viewport))); }
bool mouse_held_no_lock       (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_held      ) && (params.invert != (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport))); }
bool mouse_drag_no_lock       (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_drag      ) && (params.invert != (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport))); }
bool mouse_begin_drag_no_lock (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_begin_drag) && (params.invert != (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport))); }
bool mouse_end_drag_no_lock   (u8 button, InputQueryParams params = {}) { return (key_state[256 + button].state & KeyState_end_drag  ) && (params.invert != (params.anywhere || in_bounds(key_state[256 + button].start_position, current_viewport))); }

bool mouse_down(u8 button, InputQueryParams params = {}) {
	if (!input_is_locked || input_user_index == input_locker) {
		return mouse_down_no_lock(button, params);
	}
	return false;
}
bool mouse_up(u8 button, InputQueryParams params = {}) {
	if (!input_is_locked || input_user_index == input_locker) {
		return mouse_up_no_lock(button, params);
	}
	return false;
}
bool mouse_click     (u8 button, InputQueryParams params = {}) {
	bool result = !input_is_locked && mouse_click_no_lock(button, params);
	if (result) {
		key_state[256 + button].state |= KeyState_clicked;
	}
	return result;
}
bool mouse_held      (u8 button, InputQueryParams params = {}) { return !input_is_locked && mouse_held_no_lock      (button, params); }
bool mouse_begin_drag(u8 button, InputQueryParams params = {}) {
	if (!input_is_locked || input_user_index == input_locker) {
		return mouse_begin_drag_no_lock(button, params);
	}
	return false;
}
bool mouse_drag(u8 button, InputQueryParams params = {}) {
	if (!input_is_locked || input_user_index == input_locker) {
		return mouse_drag_no_lock(button, params);
	}
	return false;
}
bool mouse_end_drag(u8 button, InputQueryParams params = {}) {
	if (!input_is_locked || input_user_index == input_locker) {
		return mouse_end_drag_no_lock(button, params);
	}
	return false;
}

void lock_input(v2s position = current_mouse_position) {
	input_is_locked = true;
	input_lock_mouse_position = position;
	input_locker = input_user_index;
}
void unlock_input_nocheck() {
	input_is_locked = false;
	input_locker = 0;
}
void unlock_input() {
	assert(input_locker == input_user_index);
	unlock_input_nocheck();
}

bool should_focus() {
	if (should_switch_focus_to == focusable_input_user_index) {
		should_switch_focus_to = 0;
		return true;
	} else {
		return false;
	}
}

bool begin_drag_and_drop() {
	if (drag_and_dropping) {
		return false;
	}

	bool result = mouse_begin_drag(0);
	if (result) {
		drag_and_dropping = true;
		lock_input();
		return true;
	} else {
		return false;
	}
}

void set_drag_and_drop_file(Span<utf8> path) {
	drag_and_drop_path = as_list(path);
}

bool accept_drag_and_drop() {
	if (!drag_and_dropping)
		return false;

	bool result = (key_state[256 + 0].state & KeyState_up) && in_bounds(current_mouse_position, current_scissor);
	if (result) {
		drag_and_dropping = false;
		unlock_input_nocheck();
		return true;
	} else {
		return false;
	}
}

List<utf8> get_drag_and_drop_file() {
	return drag_and_drop_path;
}

