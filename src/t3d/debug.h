#pragma once

struct DebugPoint {
	v3f position;
	v3f color;
};

struct DebugLine {
	DebugPoint a;
	DebugPoint b;
	f32 time;
};

List<DebugLine> debug_lines;
tg::VertexBuffer *debug_lines_vertex_buffer;
tg::Shader *debug_line_shader;

inline void debug_line(v3f a, v3f b) {
	debug_lines.add({.a = {.position = a, .color = {1,1,1}}, .b = {.position = b, .color = {1,1,1}}, .time = 0});
}

inline void debug_line(v3f pa, v3f ca, v3f pb, v3f cb) {
	debug_lines.add({.a = {.position = pa, .color = ca}, .b = {.position = pb, .color = cb}, .time = 0});
}

inline void debug_line(v3f pa, v3f pb, v3f c) {
	debug_lines.add({.a = {.position = pa, .color = c}, .b = {.position = pb, .color = c}, .time = 0});
}

inline void debug_line(f32 time, v3f pa, v3f pb, v3f c) {
	debug_lines.add({.a = {.position = pa, .color = c}, .b = {.position = pb, .color = c}, .time = time});
}

inline void debug_draw_lines() {
	List<line<DebugPoint>> vertices;
	vertices.allocator = temporary_allocator;
	for (auto &line : debug_lines) {
		vertices.add({line.a, line.b});
	}

	if (vertices.size) {
		app->tg->set_topology(tg::Topology_line_list);
		app->tg->set_rasterizer({.depth_test = false});
		app->tg->disable_blend();
		app->tg->set_vertex_buffer(debug_lines_vertex_buffer);
		app->tg->update_vertex_buffer(debug_lines_vertex_buffer, as_bytes(vertices));
		app->tg->set_shader(debug_line_shader);
		app->tg->draw(vertices.size * 2);
	}
}

inline void debug_frame() {
	for (u32 line_index = 0; line_index < debug_lines.size; ++line_index) {
		auto &line = debug_lines[line_index];
		line.time -= app->frame_time;
		if (line.time <= 0) {
			erase_unordered_at(debug_lines, line_index);
			--line_index;
		}
	}
}
