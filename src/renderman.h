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

NB_EXTERN void rm_backbuffer_resize(s32 width, s32 height);

// Clear the current render target to the specified color.
NB_EXTERN void rm_clear_render_target(float r, float g, float b, float a);



// Pushes a right handed orthographic matrix for rendering.
NB_EXTERN void rm_begin_rendering_2d(float render_target_width, 
                                     float render_target_height);

// Draws the entire frame.
NB_EXTERN void rm_end_frame(void);

NB_EXTERN void rm_set_viewport(float x0, float y0, float x1, float y1);

// Pushes a quad to the immediate vertex buffer.
NB_EXTERN void rm_immediate_quad(float x0, float y0, float x1, float y1,
                                 float r, float g, float b, float a);

#endif  // RENDERMAN_INCLUDE_H