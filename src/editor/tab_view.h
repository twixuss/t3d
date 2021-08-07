#pragma once
#include "window.h"
#include "../blit.h"

struct TabView;

void move(TabView *tab_view, u32 tab_index, TabView *to);

struct DragDropTabInfo {
	TabView *tab_view;
	u32 tab_index;
};

struct TabMove {
	TabView *from;
	TabView *to;
	u32 tab_index;
	u32 direction;
};
List<TabMove> tab_moves;

struct TabView : EditorWindow {
	static constexpr s32 tab_height = 16;
	
	struct Tab {
		EditorWindow *window;
		bool needs_resize;
	};

	List<Tab> tabs;
	u32 selected_tab;
	v2u get_min_size() {
		return tabs[selected_tab].window->get_min_size() + v2u{0, (u32)tab_height};
	}
	void resize(tg::Viewport viewport) {
		this->viewport = viewport;

		for (auto &tab : tabs) {
			tab.needs_resize = true;
		}

		resize_selected_tab();
	}
	void resize_selected_tab() {
		auto tab_viewport = viewport;
		tab_viewport.max.y -= tab_height;
		tabs[selected_tab].window->resize(tab_viewport);
		tabs[selected_tab].needs_resize = false;
	}
	void render() {
		tg::set_render_target(tg::back_buffer);

		bool highlight_dnd = false;
		
		auto bar_viewport = viewport;
		bar_viewport.min.y = bar_viewport.max.y - tab_height;
		push_current_viewport(bar_viewport) {
			blit(background_color);

			if (drag_and_drop_kind == DragAndDrop_tab) {
				highlight_dnd = true;

				if (accept_drag_and_drop(DragAndDrop_tab)) {
					assert(drag_and_drop_data.size == sizeof(DragDropTabInfo));
					auto data = *(DragDropTabInfo *)drag_and_drop_data.data;

					if (data.tab_view != this) {
						tab_moves.add({
							.from = data.tab_view,
							.to = this,
							.tab_index = data.tab_index,
							.direction = (u32)-1,
						});
					}
				}
			}

			s32 tab_start_x = 2;

			auto font = get_font_at_size(font_collection, font_size);
			for (u32 tab_index = 0; tab_index < tabs.size; tab_index += 1) {
				auto &tab = tabs[tab_index];
				ensure_all_chars_present(tab.window->name, font);
				auto placed_chars = with(temporary_allocator, place_text(tab.window->name, font));

				auto tab_viewport = bar_viewport;
				tab_viewport.min.x += tab_start_x;
				tab_viewport.max.x = tab_viewport.min.x + placed_chars.back().position.max.x + 4;
				tab_start_x += tab_viewport.size().x + 2;

				push_current_viewport(tab_viewport) {

					auto theme = default_button_theme;
					theme.color = middle_color;
					theme.hover_enter_color = default_button_theme.hover_enter_color / default_button_theme.color * theme.color;
					theme.hover_stay_color  = default_button_theme.hover_stay_color  / default_button_theme.color * theme.color;
					theme.press_color       = default_button_theme.press_color       / default_button_theme.color * theme.color;
					if (button(tab.window->name, (umm)this, theme)) {
						selected_tab = tab_index;
					}
					//blit({.1,.1,.1,1});
					//draw_text(placed_chars, font, {.position = {2, 0}});

					if (begin_drag_and_drop(DragAndDrop_tab)) {
						DragDropTabInfo data;
						data.tab_view = this;
						data.tab_index = tab_index;
						drag_and_drop_data.set({(u8 *)&data, sizeof(data)});
					}
				}
			}
			if (highlight_dnd) {
				assert(drag_and_drop_data.size == sizeof(DragDropTabInfo));
				auto data = *(DragDropTabInfo *)drag_and_drop_data.data;
				
				if (data.tab_view != this) {
					blit({.1,1,.1,.2});
				}
			}
		}

		if (tabs[selected_tab].needs_resize) {
			resize_selected_tab();
		}
		tabs[selected_tab].window->render();

		
		if (highlight_dnd) {
			s32 const area_radius = 16;

			constexpr v2s offsets[] = {
				{ area_radius * 3, 0},
				{ 0, area_radius * 3},
				{ -area_radius * 3, 0},
				{ 0, -area_radius * 3},
			};

			for (u32 offset_index = 0; offset_index < 4; offset_index += 1) {
				v2s center = current_viewport.center() + offsets[offset_index];

				auto viewport = aabb_center_radius(center, V2s(area_radius));

				assert(drag_and_drop_data.size == sizeof(DragDropTabInfo));
				auto data = *(DragDropTabInfo *)drag_and_drop_data.data;

				if (data.tab_view == this && tabs.size == 1) {
				} else {
					push_current_viewport(viewport) {
						blit({.1,1,.1,.2});
					
						if (accept_drag_and_drop(DragAndDrop_tab)) {

							tab_moves.add({
								.from = data.tab_view,
								.to = this,
								.tab_index = data.tab_index,
								.direction = offset_index,
							});
						}
					}
				}
			}
		}
	}
	void debug_print() {
		for (auto &tab : tabs) {
			tab.window->debug_print();
		}
	}
};

TabView *create_tab_view(EditorWindow *child) {
	auto result = create_editor_window<TabView>(EditorWindow_tab_view);
	result->tabs.add({.window = child});
	child->parent = result;
	result->name = u8"Tab view"s;
	return result;
}
