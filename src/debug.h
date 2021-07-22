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
t3d::VertexBuffer *debug_lines_vertex_buffer;
t3d::Shader *debug_line_shader;

void debug_line(v3f a, v3f b) {
	debug_lines.add({.a = {.position = a}, .b = {.position = b}, .time = 0});
}

void debug_line(v3f pa, v3f ca, v3f pb, v3f cb) {
	debug_lines.add({.a = {.position = pa, .color = ca}, .b = {.position = pb, .color = cb}, .time = 0});
}

void debug_line(v3f pa, v3f pb, v3f c) {
	debug_lines.add({.a = {.position = pa, .color = c}, .b = {.position = pb, .color = c}, .time = 0});
}

void debug_line(f32 time, v3f pa, v3f pb, v3f c) {
	debug_lines.add({.a = {.position = pa, .color = c}, .b = {.position = pb, .color = c}, .time = time});
}

void debug_draw_lines() {
	List<line<DebugPoint>> vertices;
	vertices.allocator = temporary_allocator;
	for (auto &line : debug_lines) {
		vertices.add({line.a, line.b});
	}

	t3d::set_topology(t3d::Topology_line_list);
	t3d::set_rasterizer({.depth_test = false});
	t3d::set_blend(t3d::BlendFunction_disable, {}, {});
	t3d::set_vertex_buffer(debug_lines_vertex_buffer);
	t3d::update_vertex_buffer(debug_lines_vertex_buffer, as_bytes(vertices));
	t3d::set_shader(debug_line_shader);
	t3d::draw(vertices.size * 2);
}

void debug_frame() {
	for (u32 line_index = 0; line_index < debug_lines.size; ++line_index) {
		auto &line = debug_lines[line_index];
		line.time -= frame_time;
		if (line.time <= 0) {
			erase_unordered_at(debug_lines, line_index);
			--line_index;
		}
	}
}