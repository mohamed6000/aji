#include "nb.h"
#include "bender.h"
#include "renderman.h"


enum Piece_Types {
    PIECE_I,
    PIECE_J,
    PIECE_L,
    PIECE_O,
    PIECE_S,
    PIECE_Z,
    PIECE_T,

    PIECE_COUNT,
};

float piece_colors[PIECE_COUNT][4] = {
    // { 0.678f, 0.847f, 0.902f, 1 },
    { 0.004f, 0.902f, 0.996f, 1 },
    { 0.094f, 0.004f, 1,      1 },
    { 1,      0.451f, 0.031f, 1 },
    { 1,      0.871f, 0,      1 },
    { 0.4f,   0.992f, 0,      1 },
    { 0.996f, 0.063f, 0.235f, 1 },
    { 0.722f, 0.008f, 0.992f, 1 },
};

u8 piece_shapes[PIECE_COUNT][4][4] = {
    {
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {1, 1, 1, 1},
        {0, 0, 0, 0},
    },

    {
        {0, 0, 0, 0},
        {2, 0, 0, 0},
        {2, 2, 2, 0},
        {0, 0, 0, 0},
    },

    {
        {0, 0, 0, 0},
        {0, 0, 3, 0},
        {3, 3, 3, 0},
        {0, 0, 0, 0},
    },

    {
        {4, 4, 0, 0},
        {4, 4, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
    },

    {
        {0, 5, 5, 0},
        {5, 5, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
    },

    {
        {6, 6, 0, 0},
        {0, 6, 6, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
    },

    {
        {0, 7, 0, 0},
        {7, 7, 7, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
    },
};

u8 play_field[40][10];
u32 block_size = 32;
u8 piece_id = PIECE_I;

inline void play_field_to_right_handed_coords(s32 x, s32 y, 
    s32 *x_return, s32 *y_return) {
    *x_return = x * block_size;
    *y_return = (39 * block_size) - (y * block_size);
}

inline u8 get_play_field_block(s32 x, s32 y) {
    u8 result = 0;
    if (x >= 0 && x < 10 && y >= 0 && y < 40) {
        result = play_field[y][x];
    }

    return result;
}

inline u8 get_piece_block(u8 piece, s32 x, s32 y) {
    u8 result = 0;

    if (x >= 0 && x < 4 && y >= 0 && y < 4) {
        result = piece_shapes[piece][y][x];
    }

    return result;
}


//
// This file is only here for the purpose of testing the engine API.
// In a real project, the entrypoint should be platform specific...
//

int main(void) {
    u32 id = bender_create_window("AJI", 640, block_size*20, -1, -1, 0, 
                                  0, B_WINDOW_BACKGROUND_COLOR);

    rm_init(id);

    RMShader *color_shader = rm_shader_create_from_file("data/shaders/color_vs.hlsl",
                                                        "data/shaders/color_ps.hlsl",
                                                        "Color Shader");
    if (!color_shader) return 0;

    u32 texture_id = rm_texture_create(RM_FORMAT_RGBA8, 4, 4, 1, false, false, null);
    if (texture_id == -1) return 0;

    s32 render_target_width  = 0;
    s32 render_target_height = 0;
    bender_get_window_size(id, &render_target_width, &render_target_height);

    bool is_fullscreen = false;

    BInput_State *input = bender_get_input_state();
    RMShader *argb_texture_shader = rm_render_presets_get(RM_PRESET_ARGB_TEXTURE);

    s32 move_dx = 0;

    s32 piece_x = 5;
    s32 piece_y = 19;
    float move_y = (float)piece_y;
    float move_down_speed = 1.0f;

    if (id != -1) {
        bool ap_running = true;
        while (ap_running) {
            bender_update_window_events();

            float mx = (float)input->mouse_x;
            float my = (float)(render_target_height - input->mouse_y);

            move_down_speed = 1;

            BEvent event;
            while (bender_get_next_event(input, &event)) {
                if (event.type == B_EVENT_QUIT) {
                    if (bender_messagebox_confirm("Quit AJI?", "Are you sure? You want to quit AJI?"))
                        ap_running = false;
                }

                if (event.type == B_EVENT_WINDOW_RESIZE) {
                    print("Window sized = %dx%d\n", event.x, event.y);

                    if (event.x != render_target_width ||
                        event.y != render_target_height) {
                        print("Resizing backbuffer: %dx%d\n", event.x, event.y);
                        rm_backbuffer_resize(event.x, event.y);
                    }

                    render_target_width  = event.x;
                    render_target_height = event.y;
                }

/*
                if (event.type == B_EVENT_TOUCH) {
                    const char *touch_event[4] = {"None", "Pressed", "Released", "Moved"};
                    print("Touch %u: %s (%d, %d)\n", 
                          event.touch_index, 
                          touch_event[event.touch_type],
                          event.x, event.y);
                }
*/

                if (event.type == B_EVENT_KEYBOARD) {
                    if (event.key_pressed && event.key_code == B_KEY_ESCAPE) ap_running = false;

                    if (!event.key_pressed && event.key_code == B_KEY_ENTER &&
                        event.modifier_flags & B_MOD_ALT_PRESSED) {
                        is_fullscreen = !is_fullscreen;
                        bender_toggle_fullscreen(id, is_fullscreen);
                    }

                    if (event.key_pressed && event.key_code == B_KEY_ARROW_LEFT) {
                        move_dx = -1;
                    }
                    if (event.key_pressed && event.key_code == B_KEY_ARROW_RIGHT) {
                        move_dx = 1;
                    }
                    if (event.key_pressed && event.key_code == B_KEY_ARROW_DOWN) {
                        move_down_speed = 6;
                    }
                }

                nb_reset_temporary_storage();
            }

            for (s32 index = 0; index < input->touch_pointer_count; ++index) {
                BTouch_Pointer *pointer = input->touch_pointers + index;

                const char *touch_event[4] = {"None", "Pressed", "Released", "Moved"};
                print("Touch %u: %s (%d, %d)\n", 
                      index, 
                      touch_event[pointer->type],
                      pointer->x, pointer->y);
            }


            move_y += 0.025f * move_down_speed;
            piece_y = (s32)move_y;

            bool place_in_field = false;
            bool can_move_piece = false;

            for (s32 y = 0; y < 4; ++y) {
                for (s32 x = 0; x < 4; ++x) {
                    s32 test_x = piece_x + x;
                    s32 test_y = piece_y + y;
                    u8 block = get_piece_block(piece_id, x, y);
                    if (block) {
                        if ((test_x + move_dx) < 0) {
                            can_move_piece = false;
                            piece_x = 0;
                            move_dx = 0;
                            // break;
                        }

                        if ((test_x + move_dx) > 9) {
                            can_move_piece = false;
                            piece_x = 9 - x;
                            move_dx = 0;
                            // break;
                        }

                        if (get_play_field_block(test_x + move_dx, test_y) == 0) {
                            can_move_piece = true;
                        } else {
                            can_move_piece = false;
                        }

                        if (test_y > 39) {
                            piece_y = 39 - y;
                            place_in_field = true;
                            // break;
                        }

                        if (get_play_field_block(test_x, test_y + 1) != 0) {
                            place_in_field = true;
                            // move_y = 0;
                            break;
                        }
                    }
                }
            }

            if (can_move_piece) {
                piece_x += move_dx;
            }
            move_dx = 0;

            if (place_in_field) {
                for (s32 y = 0; y < 4; ++y) {
                    for (s32 x = 0; x < 4; ++x) {
                        u8 block = get_piece_block(piece_id, x, y);
                        if (block)
                            play_field[piece_y+y][piece_x+x] = block;
                    }
                }

                piece_x = 5;
                piece_y = 19;
                move_y = (float)piece_y;
                piece_id = (piece_id + 1) % PIECE_COUNT;
            }


            rm_viewport_set(0,0, (float)render_target_width, (float)render_target_height);
            rm_clear_render_target(0.18f, 0.34f, 0.34f, 1, true, true);

            rm_begin_rendering_2d((float)render_target_width, (float)render_target_height);

            RMShader *shader = argb_texture_shader;
            rm_shader_set(shader);

            rm_shader_state_set_depth_test(shader, 0);
            rm_shader_state_set_cull_mode(shader, RM_CW);
            rm_shader_state_set_fill_mode(shader, RM_FILL_SOLID);
            rm_shader_state_set_blend_mode(shader, true, RM_ADD, RM_SRC_ALPHA, RM_ONE_MINUS_SRC_ALPHA);
            // rm_shader_state_set_alpha_to_coverage(shader, false);
            
            rm_shader_texture_set(shader, 0, texture_id);


            rm_immediate_quad(mx, my, mx+10.0f, my+10.0f, 0, 1, 0, 1);

            // Draw play field grid.
            float offset_x = (render_target_width * 0.5f) - ((10 * block_size) * 0.5f);

            for (u32 y = 0; y < 20; ++y) {
                for (u32 x = 0; x < 10; ++x) {
                    u8 block = get_play_field_block(x, 20 + y);
                    if (block) {
                        float *c = piece_colors[block-1];
                        s32 x_coord = 0, y_coord = 0;
                        play_field_to_right_handed_coords(x, 20 + y, &x_coord, &y_coord);

                        float x0 = offset_x + (float)x_coord;
                        float y0 = (float)y_coord;
                        float x1 = x0 + block_size;
                        float y1 = y0 + block_size;
                        rm_immediate_quad(x0, y0, x1, y1, c[0], c[1], c[2], c[3]);
                    }
                }
            }


            // Draw controlled pieces.
            {
                float *c = piece_colors[piece_id];
                for (s32 y = 0; y < 4; ++y) {
                    for (s32 x = 0; x < 4; ++x) {
                        u8 block = get_piece_block(piece_id, x, y);
                        if (block) {
                            s32 x_coord = 0, y_coord = 0;
                            play_field_to_right_handed_coords(piece_x + x, piece_y + y, &x_coord, &y_coord);

                            float x0 = offset_x + (float)x_coord;
                            float y0 = (float)y_coord;
                            float x1 = x0 + block_size;
                            float y1 = y0 + block_size;

                            rm_immediate_quad(x0, y0, x1, y1, c[0], c[1], c[2], c[3]);
                        }
                    }
                }
            }

            // Hor grid lines.
            {
                float x0 = offset_x;
                float x1 = x0 + block_size * 10;
                
                for (u32 y = 0; y < 20 + 1; ++y) {
                    float y0 = (float)y * block_size;

                    rm_immediate_quad(x0, y0, x1, y0 + 1,  0, 0, 0, 1);
                }
            }

            // Ver grid lines.
            {
                float y0 = 0;
                float y1 = y0 + block_size * 20;

                for (u32 x = 0; x < 10 + 1; ++x) {
                    float x0 = offset_x + (float)x * block_size;

                    rm_immediate_quad(x0, y0, x0 + 1, y1,  0, 0, 0, 1);
                }
            }

            rm_immediate_frame_end();

            rm_swap_buffers(id);
        }
    }

    rm_texture_free(texture_id);

    return 0;
}



#define NB_IMPLEMENTATION
#include "nb.h"
#include "bender/bender_windows.c"
#include "renderman/renderman_d3d9.cpp"
#include "renderman/dxerr.c"


#if OS_WINDOWS
// NB_EXTERN NB_EXPORT DWORD NvOptimusEnablement = 0x00000001;
// NB_EXTERN NB_EXPORT int AmdPowerXpressRequestHighPerformance = 1;
#endif
