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

// Clear the current render target to the specified color.
NB_EXTERN void rm_clear_render_target(float r, float g, float b, float a);

#endif  // RENDERMAN_INCLUDE_H