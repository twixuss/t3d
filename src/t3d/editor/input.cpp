#include "input.h"
#include <t3d/shared.h>

void begin_input_user(bool focusable) {
	shared->input_user_index += 1;
	if (focusable) {
		shared->focusable_input_user_index += 1;
	}
}

bool key_down  (u8 key, InputQueryParams params) { return (shared->key_state[key].state & KeyState_down    ) && (params.invert != (params.anywhere || in_bounds(shared->key_state[key].start_position, shared->current_viewport))); }
bool key_up    (u8 key, InputQueryParams params) { return (shared->key_state[key].state & KeyState_up      ) && (params.invert != (params.anywhere || in_bounds(shared->key_state[key].start_position, shared->current_viewport))); }
bool key_repeat(u8 key, InputQueryParams params) { return (shared->key_state[key].state & KeyState_repeated) && (params.invert != (params.anywhere || in_bounds(shared->key_state[key].start_position, shared->current_viewport))); }
bool key_held  (u8 key, InputQueryParams params) { return (shared->key_state[key].state & KeyState_held    ) && (params.invert != (params.anywhere || in_bounds(shared->key_state[key].start_position, shared->current_viewport))); }

bool mouse_down_no_lock       (u8 button, InputQueryParams params) {
	bool was_in_bounds = in_bounds(shared->key_state[256 + button].start_position, shared->current_viewport);
	bool state = shared->key_state[256 + button].state & KeyState_down;
	return state && (params.invert != (params.anywhere || was_in_bounds));
}
bool mouse_up_no_lock         (u8 button, InputQueryParams params) { return (shared->key_state[256 + button].state & KeyState_up        ) && (params.invert != (params.anywhere || in_bounds(shared->key_state[256 + button].start_position, shared->current_viewport))); }
bool mouse_click_no_lock      (u8 button, InputQueryParams params) { return (shared->key_state[256 + button].state & KeyState_up && !(shared->key_state[256 + button].state & KeyState_clicked)) && (params.anywhere || (in_bounds(shared->current_mouse_position, shared->current_viewport) && in_bounds(shared->key_state[256 + button].start_position, shared->current_viewport))); }
bool mouse_held_no_lock       (u8 button, InputQueryParams params) { return (shared->key_state[256 + button].state & KeyState_held      ) && (params.invert != (params.anywhere || in_bounds(shared->key_state[256 + button].start_position, shared->current_viewport))); }
bool mouse_drag_no_lock       (u8 button, InputQueryParams params) { return (shared->key_state[256 + button].state & KeyState_drag      ) && (params.invert != (params.anywhere || in_bounds(shared->key_state[256 + button].start_position, shared->current_viewport))); }
bool mouse_begin_drag_no_lock (u8 button, InputQueryParams params) { return (shared->key_state[256 + button].state & KeyState_begin_drag) && (params.invert != (params.anywhere || in_bounds(shared->key_state[256 + button].start_position, shared->current_viewport))); }
bool mouse_end_drag_no_lock   (u8 button, InputQueryParams params) { return (shared->key_state[256 + button].state & KeyState_end_drag  ) && (params.invert != (params.anywhere || in_bounds(shared->key_state[256 + button].start_position, shared->current_viewport))); }

bool mouse_down(u8 button, InputQueryParams params) {
	if (!shared->input_is_locked || shared->input_user_index == shared->input_locker) {
		return mouse_down_no_lock(button, params);
	}
	return false;
}
bool mouse_up(u8 button, InputQueryParams params) {
	if (!shared->input_is_locked || shared->input_user_index == shared->input_locker) {
		return mouse_up_no_lock(button, params);
	}
	return false;
}
bool mouse_click     (u8 button, InputQueryParams params) {
	bool result = !shared->input_is_locked && mouse_click_no_lock(button, params);
	if (result) {
		shared->key_state[256 + button].state |= KeyState_clicked;
	}
	return result;
}
bool mouse_held      (u8 button, InputQueryParams params) { return !shared->input_is_locked && mouse_held_no_lock      (button, params); }
bool mouse_begin_drag(u8 button, InputQueryParams params) {
	if (!shared->input_is_locked || shared->input_user_index == shared->input_locker) {
		return mouse_begin_drag_no_lock(button, params);
	}
	return false;
}
bool mouse_drag(u8 button, InputQueryParams params) {
	if (!shared->input_is_locked || shared->input_user_index == shared->input_locker) {
		return mouse_drag_no_lock(button, params);
	}
	return false;
}
bool mouse_end_drag(u8 button, InputQueryParams params) {
	if (!shared->input_is_locked || shared->input_user_index == shared->input_locker) {
		return mouse_end_drag_no_lock(button, params);
	}
	return false;
}

void lock_input(v2s position) {
	shared->input_is_locked = true;
	shared->input_lock_mouse_position = position;
	shared->input_locker = shared->input_user_index;
}
void unlock_input_nocheck() {
	shared->should_unlock_input = true;
}
void unlock_input() {
	assert(shared->input_locker == shared->input_user_index);
	unlock_input_nocheck();
}

bool should_focus() {
	if (shared->should_switch_focus_to == shared->focusable_input_user_index) {
		shared->should_switch_focus_to = 0;
		return true;
	} else {
		return false;
	}
}

bool drag_and_dropping() {
	return shared->drag_and_drop_kind != DragAndDrop_none;
}

bool begin_drag_and_drop(DragAndDropKind kind) {
	if (shared->input_is_locked || drag_and_dropping()) {
		return false;
	}

	bool result = mouse_begin_drag(0);
	if (result) {
		shared->drag_and_drop_kind = kind;
		lock_input();
		return true;
	} else {
		return false;
	}
}

bool accept_drag_and_drop(DragAndDropKind kind) {
	if (!drag_and_dropping() || shared->drag_and_drop_kind != kind)
		return false;

	bool result = (shared->key_state[256 + 0].state & KeyState_up) && in_bounds(shared->current_mouse_position, shared->current_scissor);
	if (result) {
		shared->drag_and_drop_kind = DragAndDrop_none;
		unlock_input_nocheck();
		return true;
	} else {
		return false;
	}
}

v2s get_current_mouse_position() { return shared->current_mouse_position; }

