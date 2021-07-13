#define T3D_IMPL
#define STB_IMAGE_IMPLEMENTATION
#include "../include/t3d.h"
#include "../dep/tl/include/tl/console.h"

namespace t3d {

#define A(ret, name, args, values) ret (*_##name) args;
APIS(A)
#undef A

RenderTarget *back_buffer;
v2u min_texture_size;

namespace d3d11 { bool init(InitInfo init_info); }
namespace gl    { bool init(InitInfo init_info); }

static bool init_api(GraphicsApi api, InitInfo init_info) {
	switch (api) {
		case t3d::GraphicsApi_d3d11: return d3d11::init(init_info);
		case t3d::GraphicsApi_opengl: return gl::init(init_info);
	}
	return false;
}

static bool check_api() {
	bool result = true;
#define A(ret, name, args, values) if(!_##name){print("t3d::" #name " was not initialized.\n");result=false;}
APIS(A)
#undef A
	return result;
}

bool init(GraphicsApi api, InitInfo init_info) {
	return init_api(api, init_info) && check_api();
}

}

