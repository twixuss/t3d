#include "../include/t3d.h"

tl::umm get_hash(tl::Span<t3d::ElementType> types) {
	tl::umm hash = 0x13579BDF2468ACE;
	for (auto &type : types) {
		hash = tl::rotate_left(hash, 1) | type;
	}
	return hash;
}


#include "../dep/tl/include/tl/console.h"
#include "../dep/tl/include/tl/masked_block_list.h"
#include "../dep/tl/include/tl/hash_map.h"

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3dcompiler")

namespace t3d::d3d11 {

template <class T, class U>
T *QueryInterface(U *u) {
	T *t = 0;
	u->QueryInterface(&t);
	return t;
}

struct ShaderImpl : Shader {
	ID3D11VertexShader *vertex_shader = 0;
	ID3D11PixelShader *pixel_shader = 0;
	ID3D11Buffer *constant_buffer = 0;
	void *constant_buffer_data = 0;
};

struct InputLayout {
	ID3D11InputLayout *layout = 0;
	u32 stride = 0;
};

struct VertexBufferImpl : VertexBuffer {
	ID3D11Buffer *buffer = 0;
	InputLayout *layout = 0;
};

struct IndexBufferImpl : IndexBuffer {
	ID3D11Buffer *buffer = 0;
	DXGI_FORMAT format = {};
	u32 count = 0;
};

struct RenderTargetImpl : RenderTarget {
	ID3D11Texture2D *color_texture;
	ID3D11Texture2D *depth_texture;
	ID3D11RenderTargetView *color_target = 0;
	ID3D11DepthStencilView *depth_target = 0;
};

struct State {
	ID3D11Device *device = 0;
	ID3D11DeviceContext *immediate_context = 0;
	RenderTargetImpl back_buffer;
	IDXGISwapChain *swap_chain = 0;
	UINT sync_interval = 1;
	MaskedBlockList<ShaderImpl, 256> shaders;
	MaskedBlockList<VertexBufferImpl, 256> vertex_buffers;
	MaskedBlockList<IndexBufferImpl, 256> index_buffers;
	MaskedBlockList<RenderTargetImpl, 256> render_targets;
	LinearSet<RenderTargetImpl *> render_targets_resized_with_window;
	StaticHashMap<Span<ElementType>, InputLayout, 256> input_layouts;
};

static State *state;

bool create_back_buffer(u32 width, u32 height) {
	ID3D11Texture2D *back_buffer_texture = 0;
	if (FAILED(state->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer_texture)))) {
		return false;
	}

	if (FAILED(state->device->CreateRenderTargetView(back_buffer_texture, 0, &state->back_buffer.color_target))) {
		return false;
	}
	back_buffer_texture->Release();

	{
		auto format = DXGI_FORMAT_D32_FLOAT;
		ID3D11Texture2D *depth_texture = 0;
		{
			D3D11_TEXTURE2D_DESC desc = {
				.Width = width,
				.Height = height,
				.MipLevels = 1,
				.ArraySize = 1,
				.Format = format,
				.SampleDesc = {1, 0},
				.Usage = D3D11_USAGE_DEFAULT,
				.BindFlags = D3D11_BIND_DEPTH_STENCIL,
			};
			assert(SUCCEEDED(state->device->CreateTexture2D(&desc, 0, &depth_texture)));
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC desc = {
			.Format = format,
			.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
		};
		assert(SUCCEEDED(state->device->CreateDepthStencilView(depth_texture, &desc, &state->back_buffer.depth_target)));
	}

	state->immediate_context->OMSetRenderTargets(1, &state->back_buffer.color_target, state->back_buffer.depth_target);
	return true;
}

DXGI_FORMAT get_element_format(ElementType type) {
	switch (type) {
		case Element_f32x1: return DXGI_FORMAT_R32_FLOAT;
		case Element_f32x2: return DXGI_FORMAT_R32G32_FLOAT;
		case Element_f32x3: return DXGI_FORMAT_R32G32B32_FLOAT;
		case Element_f32x4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	}
	invalid_code_path();
	return DXGI_FORMAT_UNKNOWN;
}

u32 get_element_size(ElementType type) {
	switch (type) {
		case Element_f32x1: return 4;
		case Element_f32x2: return 8;
		case Element_f32x3: return 12;
		case Element_f32x4: return 16;
	}
	invalid_code_path();
	return 0;
}

DXGI_FORMAT get_index_format_from_size(u32 size) {
	switch (size) {
		case 2: return DXGI_FORMAT_R16_UINT;
		case 4: return DXGI_FORMAT_R32_UINT;
	}
	invalid_code_path();
	return DXGI_FORMAT_UNKNOWN;
}


char const *get_hlsl_type(ElementType type) {
	switch (type) {
		case Element_f32x1: return "float";
		case Element_f32x2: return "float2";
		case Element_f32x3: return "float3";
		case Element_f32x4: return "float4";
	}
	invalid_code_path();
	return 0;
}

ID3DBlob *compile_shader(Span<utf8> source, char const *name, char const *entry, char const *profile) {
	ID3DBlob *bytecode = 0;
	ID3DBlob *messages = 0;
	D3DCompile(source.data, source.size, name, 0, 0, entry, profile, 0, 0, &bytecode, &messages);
	if (messages) {
		print(Span((char *)messages->GetBufferPointer(), messages->GetBufferSize()));
		messages->Release();
		messages = 0;
	}
	return bytecode;
}

InputLayout *get_input_layout(Span<ElementType> vertex_descriptor) {
	auto &result = state->input_layouts.get_or_insert(vertex_descriptor);

	if (!result.layout) {
		List<D3D11_INPUT_ELEMENT_DESC> element_descs;
		element_descs.allocator = temporary_allocator;

		StringBuilder shader_builder;
		shader_builder.allocator = temporary_allocator;
		append(shader_builder, "struct Input{");

		for (u32 element_index = 0; element_index < vertex_descriptor.size; ++element_index) {
			auto &type = vertex_descriptor[element_index];
			auto semantic = (char)('A' + element_index);
			D3D11_INPUT_ELEMENT_DESC element = {
				.SemanticName = tformat("%%", semantic, '\0').data,
				.Format = get_element_format(type),
				.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
				.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
			};
			element_descs.add(element);
			result.stride += get_element_size(type);
			append_format(shader_builder, "% field%:%;", get_hlsl_type(type), element_index, semantic);
		}
		append(shader_builder, "};float4 main(in Input i):SV_Position{return 0;}");

		ID3DBlob *vertex_shader_bytecode = compile_shader((List<utf8>)to_string(shader_builder), "input_layout", "main", "vs_5_0");

		assert(SUCCEEDED(state->device->CreateInputLayout(element_descs.data, vertex_descriptor.size, vertex_shader_bytecode->GetBufferPointer(), vertex_shader_bytecode->GetBufferSize(), &result.layout)));
	}

	return &result;
}

DXGI_FORMAT get_format(TextureFormat format) {
	switch (format) {
		case t3d::TextureFormat_r_f32: return DXGI_FORMAT_R32_FLOAT;
	}
	invalid_code_path();
	return DXGI_FORMAT_UNKNOWN;
}

bool init(InitInfo init_info) {
	state = current_allocator.allocate<State>();

	IDXGIFactory *factory = 0;
	if (FAILED(CreateDXGIFactory(IID_PPV_ARGS(&factory)))) {
		return false;
	}
	defer {
		factory->Release();
	};

	IDXGIAdapter *adapter = 0;
	for (UINT adapter_index = 0; SUCCEEDED(factory->EnumAdapters(adapter_index, &adapter)); ++adapter_index) {
		if (auto adapter1 = QueryInterface<IDXGIAdapter1>(adapter)) {
			DXGI_ADAPTER_DESC1 desc;
			if (FAILED(adapter1->GetDesc1(&desc))) {
				continue;
			}

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
				continue;
			} else {
				break;
			}
		}
	}

	if (!adapter) {
		return false;
	}

	if (FAILED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, 0, init_info.debug ? D3D11_CREATE_DEVICE_DEBUG : 0, 0, 0, D3D11_SDK_VERSION, &state->device, 0, &state->immediate_context))) {
		return false;
	}

	DXGI_SWAP_CHAIN_DESC desc = {
		.BufferDesc = {
			.Width = init_info.window_size.x,
			.Height = init_info.window_size.y,
			.RefreshRate = {
				.Numerator = 1,
				.Denominator = 60,
			},
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		},
		.SampleDesc = {.Count = 1, .Quality = 0},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 1,
		.OutputWindow = (HWND)init_info.window,
		.Windowed = true,
	};

	if (FAILED(factory->CreateSwapChain(state->device, &desc, &state->swap_chain))) {
		return false;
	}

	if (!create_back_buffer(init_info.window_size.x, init_info.window_size.y)) {
		return false;
	}

	state->immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11RasterizerState *rasterizer_state = 0;
	{
		D3D11_RASTERIZER_DESC desc = {
			.FillMode = D3D11_FILL_SOLID,
			.CullMode = D3D11_CULL_FRONT,
			.AntialiasedLineEnable = false,
		};
		if (FAILED(state->device->CreateRasterizerState(&desc, &rasterizer_state))) {
			return false;
		}
	}
	state->immediate_context->RSSetState(rasterizer_state);

	_clear = [](RenderTarget *_render_target, ClearFlags flags, v4f color, f32 depth) {
		auto render_target = (RenderTargetImpl *)_render_target;
		if (!render_target) {
			render_target = &state->back_buffer;
		}
		if (flags & ClearFlags_color) state->immediate_context->ClearRenderTargetView(render_target->color_target, color.s);
		if (flags & ClearFlags_depth) state->immediate_context->ClearDepthStencilView(render_target->depth_target, D3D11_CLEAR_DEPTH, depth, 0);
	};
	_present = []() {
		assert(SUCCEEDED(state->swap_chain->Present(state->sync_interval, 0)));
	};
	_draw = [](u32 vertex_count, u32 start_vertex) {
		state->immediate_context->Draw(vertex_count, start_vertex);
	};
	_draw_indexed = [](u32 index_count) {
		state->immediate_context->DrawIndexed(index_count, 0, 0);
	};
	_set_viewport = [](u32 x, u32 y, u32 w, u32 h) {
		D3D11_VIEWPORT v = {
			.TopLeftX = (FLOAT)x,
			.TopLeftY = (FLOAT)y,
			.Width = (FLOAT)w,
			.Height = (FLOAT)h,
			.MinDepth = 0,
			.MaxDepth = 1,
		};
		state->immediate_context->RSSetViewports(1, &v);
	};
	/*
	_resize = [](RenderTarget *render_target, u32 w, u32 h) {
		if (render_target == 0) {
			state->back_buffer.color_target->Release();
			state->back_buffer.depth_target->Release();
			assert(SUCCEEDED(state->swap_chain->ResizeBuffers(1, w, h, DXGI_FORMAT_UNKNOWN, 0)));
			create_back_buffer(w, h);
		}
	};
	*/
	_set_shader = [](Shader *_shader) {
		auto &shader = *(ShaderImpl *)_shader;
		state->immediate_context->VSSetShader(shader.vertex_shader, 0, 0);
		state->immediate_context->VSSetConstantBuffers(0, 1, &shader.constant_buffer);

		state->immediate_context->PSSetShader(shader.pixel_shader, 0, 0);
		state->immediate_context->PSSetConstantBuffers(0, 1, &shader.constant_buffer);
	};
	//_set_value = [](Shader *_shader, ShaderValueLocation dest, void const *source) {
	//	auto &shader = *(ShaderImpl *)_shader;
	//	memcpy((u8 *)shader.constant_buffer_data + dest.start, source, dest.size);
	//	state->immediate_context->UpdateSubresource(shader.constant_buffer, 0, 0, shader.constant_buffer_data, 0, 0);
	//};
	_create_shader = [](Span<utf8> source) -> Shader * {
		auto &shader = state->shaders.add();
		if (&shader) {
			ID3DBlob *vertex_shader_bytecode = compile_shader(source, "vertex_shader", "vertex_main", "vs_5_0");
			if (FAILED(state->device->CreateVertexShader(vertex_shader_bytecode->GetBufferPointer(), vertex_shader_bytecode->GetBufferSize(), 0, &shader.vertex_shader))) {
				return false;
			}

			ID3DBlob *pixel_shader_bytecode = compile_shader(source, "pixel_shader", "pixel_main", "ps_5_0");
			if (FAILED(state->device->CreatePixelShader(pixel_shader_bytecode->GetBufferPointer(), pixel_shader_bytecode->GetBufferSize(), 0, &shader.pixel_shader))) {
				return false;
			}

			//{
			//	D3D11_BUFFER_DESC desc = {
			//		.ByteWidth = (UINT)values_size,
			//		.Usage = D3D11_USAGE_DEFAULT,
			//		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
			//	};
			//	if (FAILED(state->device->CreateBuffer(&desc, 0, &shader.constant_buffer))) {
			//		return false;
			//	}
			//
			//	shader.constant_buffer_data = default_allocator.allocate(values_size, 16);
			//}
		}
		return &shader;
	};
	_calculate_perspective_matrices = [](v3f position, v3f rotation, f32 aspect_ratio, f32 fov, f32 near_plane, f32 far_plane) {
		CameraMatrices result;
		result.mvp = m4::perspective_left_handed(aspect_ratio, fov, near_plane, far_plane)
				    * m4::rotation_r_yxz(rotation)
					* m4::translation(-position);
		return result;
	};
	_create_vertex_buffer = [](Span<u8> buffer, Span<ElementType> vertex_descriptor) -> VertexBuffer * {
		VertexBufferImpl &result = state->vertex_buffers.add();

		result.layout = get_input_layout(vertex_descriptor);

		D3D11_BUFFER_DESC desc = {
			.ByteWidth = (UINT)buffer.size,
			.Usage = D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_VERTEX_BUFFER,
			.StructureByteStride = result.layout->stride,
		};

		D3D11_SUBRESOURCE_DATA initial_data = {
			.pSysMem = buffer.data,
		};

		assert(SUCCEEDED(state->device->CreateBuffer(&desc, &initial_data, &result.buffer)));

		return &result;
	};
	_set_vertex_buffer = [](VertexBuffer *_buffer) {
		auto buffer = (VertexBufferImpl *)_buffer;
		UINT offset = 0;
		state->immediate_context->IASetVertexBuffers(0, 1, &buffer->buffer, &buffer->layout->stride, &offset);
		state->immediate_context->IASetInputLayout(buffer->layout->layout);
	};
	_create_index_buffer = [](Span<u8> buffer, u32 index_size) -> IndexBuffer * {
		IndexBufferImpl &result = state->index_buffers.add();

		result.format = get_index_format_from_size(index_size);

		D3D11_BUFFER_DESC desc = {
			.ByteWidth = (UINT)buffer.size,
			.Usage = D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_INDEX_BUFFER,
		};

		D3D11_SUBRESOURCE_DATA initial_data = {
			.pSysMem = buffer.data,
		};

		assert(SUCCEEDED(state->device->CreateBuffer(&desc, &initial_data, &result.buffer)));

		return &result;
	};
	_set_index_buffer = [](IndexBuffer *_buffer) {
		auto buffer = (IndexBufferImpl *)_buffer;
		state->immediate_context->IASetIndexBuffer(buffer->buffer, buffer->format, 0);
	};
	_set_vsync = [](bool enable) {
		state->sync_interval = enable ? 1 : 0;
	};
	_set_render_target = [](RenderTarget *_target) {
		auto target = (RenderTargetImpl *)_target;
		if (!target)
			target = &state->back_buffer;
		state->immediate_context->OMSetRenderTargets(1, &target->color_target, target->depth_target);
	};
	/*
	_create_render_target = [](CreateRenderTargetFlags flags, TextureFormat _format, TextureFiltering filtering, TextureComparison comparison, u32 width, u32 height) -> RenderTarget * {
		auto &result = state->render_targets.add();

		auto format = get_format(_format);

		if (flags & CreateRenderTarget_resize_with_window) {
			state->render_targets_resized_with_window.insert(&result);
		}

		{
			D3D11_TEXTURE2D_DESC desc = {
				.Width = width,
				.Height = height,
				.MipLevels = 1,
				.ArraySize = 1,
				.Format = format,
				.SampleDesc = {1, 0},
				.Usage = D3D11_USAGE_DEFAULT,
				.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
			};
			assert(SUCCEEDED(state->device->CreateTexture2D(&desc, 0, &result.color_texture)));
		}
		{
			D3D11_RENDER_TARGET_VIEW_DESC desc = {
				.Format = format,
				.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
			};
			assert(SUCCEEDED(state->device->CreateRenderTargetView(result.color_texture, &desc, &result.color_target)));
		}

		return &result;
	};
	*/

	return true;
}

}
