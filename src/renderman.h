#ifndef RENDERMAN_INCLUDE_H
#define RENDERMAN_INCLUDE_H
/*

    Renderman is the rendering engine.

*/

#include "nb.h"

NB_EXTERN bool rm_init(u32 window_id);

// Present the frame buffer to the specified window.
// (The window must be set for rendering)
NB_EXTERN void rm_swap_buffers(u32 window_id);

// Resize the rendering buffer.
NB_EXTERN void rm_backbuffer_resize(s32 width, s32 height);

// Clear the current render target to the specified color.
NB_EXTERN void rm_clear_render_target(float r, float g, float b, float a);



// Pushes a right handed orthographic matrix for rendering.
NB_EXTERN void rm_begin_rendering_2d(float render_target_width, 
                                     float render_target_height);

// Draws the entire frame.
NB_EXTERN void rm_immediate_frame_end(void);

// Sets the current drawing view.
NB_EXTERN void rm_viewport_set(float x0, float y0, float x1, float y1);

// Pushes a quad to the immediate vertex buffer.
NB_EXTERN void rm_immediate_quad(float x0, float y0, float x1, float y1,
                                 float r, float g, float b, float a);



// GPU resource management.

// RenderMan texture formats.
typedef enum {
    RM_FORMAT_R8,
    RM_FORMAT_RG8,
    RM_FORMAT_RGB8,
    RM_FORMAT_RGBA8,

    RM_FORMAT_R16,
    RM_FORMAT_RG16,
    RM_FORMAT_RGB16,
    RM_FORMAT_RGBA16,

    RM_FORMAT_R32,
    RM_FORMAT_RG32,
    RM_FORMAT_RGB32,
    RM_FORMAT_RGBA32,

    RM_FORMAT_DEPTH16,
    RM_FORMAT_DEPTH32,
    RM_FORMAT_DEPTH24_STENCIL8,
} Renderman_Format;

// Return the size in bytes.
NB_INLINE u32 rm_get_format_size(Renderman_Format format) {
    u32 result;

    switch (format) {
        case RM_FORMAT_R8:      result = 1; break;
        case RM_FORMAT_RG8:     result = 2; break;
        case RM_FORMAT_RGB8:    result = 3; break;
        case RM_FORMAT_RGBA8:   result = 4; break;

        case RM_FORMAT_R16:     result = 2; break;
        case RM_FORMAT_RG16:    result = 4; break;
        case RM_FORMAT_RGB16:   result = 6; break;
        case RM_FORMAT_RGBA16:  result = 8; break;

        case RM_FORMAT_R32:     result = 4; break;
        case RM_FORMAT_RG32:    result = 8; break;
        case RM_FORMAT_RGB32:   result = 12; break;
        case RM_FORMAT_RGBA32:  result = 16; break;

        case RM_FORMAT_DEPTH16:          result = 2; break;
        case RM_FORMAT_DEPTH32:          result = 4; break;
        case RM_FORMAT_DEPTH24_STENCIL8: result = 4; break;

        default: result = 0; break;
    }

    return result;
}

#define RM_CREATE_CUBE_MAP (u32)-1

NB_EXTERN u32 rm_texture_create(Renderman_Format format, u32 x, u32 y, u32 z, bool filter, bool wrap, void *data);
NB_EXTERN void rm_texture_free(u32 texture_id);
NB_EXTERN void rm_texture_update(u32 texture_id, Renderman_Format format, 
                                 u32 x_offset, u32 y_offset, u32 z_offset, 
                                 u32 x, u32 y, u32 z, void *data);


// Shaders.

typedef struct RMShader RMShader;

// Compiles and creates a shader object from vertex and pixel shader text sources.
// This uses the default bound allocator.
NB_EXTERN RMShader *rm_shader_create(const char *vertex_shader_source,
                                     const char *pixel_shader_source,
                                     const char *shader_name);

// Free the shader resources.
NB_EXTERN void rm_shader_free(RMShader *shader);

// Bind the current shader.
NB_EXTERN void rm_shader_set(RMShader *shader);

// Shader states.

typedef enum {
    RM_NEVER = 1,
    RM_LESS,
    RM_EQUAL,
    RM_LESSEQUAL,
    RM_GREATER,
    RM_NOTEQUAL,
    RM_GREATEREQUAL,
    RM_ALWAYS,
} RMFunc;

typedef enum {
    RM_CULL_NONE = 0,  // Do not cull back faces.
    RM_CULL_CW,        // Cull back faces with clockwise vertices.
    RM_CULL_CCW,       // Cull back faces with counterclockwise vertices.
} RMCull;

typedef enum {
    RM_FILL_SOLID = 0,
    RM_FILL_WIREFRAME,
    RM_FILL_POINT,
} RMFill;

typedef enum {
    RM_BLEND_ZERO = 1,
    RM_BLEND_ONE,
    RM_BLEND_SRCCOLOR,
    RM_BLEND_INVSRCCOLOR,
    RM_BLEND_SRCALPHA,
    RM_BLEND_INVSRCALPHA,
    RM_BLEND_DESTALPHA,
    RM_BLEND_INVDESTALPHA,
    RM_BLEND_DESTCOLOR,
    RM_BLEND_INVDESTCOLOR,
} RMBlend;

typedef enum {
    RM_BLENDOP_ADD = 1,
    RM_BLENDOP_SUBTRACT,
    RM_BLENDOP_REVSUBTRACT,
    RM_BLENDOP_MIN,
    RM_BLENDOP_MAX,
} RMBlend_Op;

NB_EXTERN void rm_shader_state_set_depth_test(RMShader *shader, u32 depth_test);
NB_EXTERN void rm_shader_state_set_cull_mode(RMShader *shader, u32 cull_mode);
NB_EXTERN void rm_shader_state_set_fill_mode(RMShader *shader, u32 fill_mode);

// If blend_op is 0, the blending is disabled and you can pass 0 to the rest of the params.
NB_EXTERN void rm_shader_state_set_blend_mode(RMShader *shader, u32 blend_op, u32 blend_src, u32 blend_dest);

// Enable Alpha To Coverage for MSAA, if it's supported by the GPU.
NB_EXTERN void rm_shader_state_set_alpha_to_coverage(RMShader *shader, bool alpha_to_coverage);

#endif  // RENDERMAN_INCLUDE_H