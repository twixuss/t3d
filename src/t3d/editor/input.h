#pragma once
#include <t3d/common.h>
#include <t3d/input.h>
#include <t3d/editor/current.h>

struct KeyInputState {
	KeyState state;
	v2s start_position;
};

struct InputQueryParams {
	bool anywhere = false;
	bool invert   = false;
};

enum DragAndDropKind {
	DragAndDrop_none,
	DragAndDrop_file,
	DragAndDrop_tab,
};

void begin_input_user(bool focusable = false);

bool key_down  (u8 key, InputQueryParams params = {});
bool key_up    (u8 key, InputQueryParams params = {});
bool key_repeat(u8 key, InputQueryParams params = {});
bool key_held  (u8 key, InputQueryParams params = {});

bool mouse_down_no_lock (u8 button, InputQueryParams params = {});
bool mouse_up_no_lock   (u8 button, InputQueryParams params = {});
bool mouse_click_no_lock(u8 button, InputQueryParams params = {});
bool mouse_held_no_lock (u8 button, InputQueryParams params = {});
bool mouse_drag_no_lock      (u8 button, InputQueryParams params = {});
bool mouse_begin_drag_no_lock(u8 button, InputQueryParams params = {});
bool mouse_end_drag_no_lock  (u8 button, InputQueryParams params = {});

bool mouse_down (u8 button, InputQueryParams params = {});
bool mouse_up   (u8 button, InputQueryParams params = {});
bool mouse_click(u8 button, InputQueryParams params = {});
bool mouse_held (u8 button, InputQueryParams params = {});
bool mouse_begin_drag(u8 button, InputQueryParams params = {});
bool mouse_drag      (u8 button, InputQueryParams params = {});
bool mouse_end_drag  (u8 button, InputQueryParams params = {});

v2s get_current_mouse_position();

void lock_input(v2s position = get_current_mouse_position());
void unlock_input_nocheck();
void unlock_input();

bool should_focus();

bool drag_and_dropping();
bool begin_drag_and_drop(DragAndDropKind kind);
bool accept_drag_and_drop(DragAndDropKind kind);
