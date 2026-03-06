#include "nb.h"
#include "bender.h"
#include "renderman.h"

#include <stdlib.h>

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

float piece_colors[PIECE_COUNT+1][4] = {
    // { 0.678f, 0.847f, 0.902f, 1 },
    { 0.004f, 0.902f, 0.996f, 1 },
    { 0.094f, 0.004f, 1,      1 },
    { 1,      0.451f, 0.031f, 1 },
    { 1,      0.871f, 0,      1 },
    { 0.4f,   0.992f, 0,      1 },
    { 0.996f, 0.063f, 0.235f, 1 },
    { 0.722f, 0.008f, 0.992f, 1 },

    { 1, 1, 1, 1 }, // Border color.
};

u8 piece_shapes[PIECE_COUNT][4][4] = {
    {
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0},
    },

    {
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0},
    },

    {
        {0, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0},
    },

    {
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0},
    },

    {
        {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 0},
    },

    {
        {0, 0, 1, 0},
        {0, 1, 1, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0},
    },

    {
        {0, 0, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 0},
    },
};

#define PLAY_FIELD_WIDTH 12
#define PLAY_FIELD_HEIGHT 21
u8 play_field[PLAY_FIELD_HEIGHT][PLAY_FIELD_WIDTH];
u32 block_size = 32;

s32 line_indices[32];
s32 line_indices_count = 0;

inline void play_field_to_right_handed_coords(s32 x, s32 y, 
    s32 *x_return, s32 *y_return) {
    *x_return = x * block_size;
    *y_return = ((PLAY_FIELD_HEIGHT-1) * block_size) - (y * block_size);
}

inline void piece_rotate(s32 x, s32 y, s32 r, s32 *rx, s32 *ry) {
    switch (r % 4) {
        case 0: *rx = x;     *ry = y;     break;  // 0
        case 1: *rx = 3 - y; *ry = x;     break;  // 90
        case 2: *rx = 3 - x; *ry = 3 - y; break;  // 180
        case 3: *rx = y;     *ry = 3 - x; break;  // 270
    }
}

inline bool piece_test_occupancy(s32 piece_id, s32 piece_x, s32 piece_y, s32 rotation) {
    for (s32 y = 0; y < 4; ++y) {
        for (s32 x = 0; x < 4; ++x) {
            s32 rx = 0, ry = 0;
            piece_rotate(x, y, rotation, &rx, &ry);

            if ((piece_x + x) >= 0 && (piece_x + x) < PLAY_FIELD_WIDTH && 
                (piece_y + y) >= 0 && (piece_y + y) < PLAY_FIELD_HEIGHT) {

                u8 local_block  = piece_shapes[piece_id][ry][rx];
                u8 global_block = play_field[piece_y+y][piece_x+x];
                
                if (local_block != 0 && global_block != 0)
                    return false;
            }
        }
    }

    return true;
}


//
// This file is only here for the purpose of testing the engine API.
// In a real project, the entrypoint should be platform specific...
//

int main(void) {
    u32 id = bender_create_window("AJI", 640, block_size*PLAY_FIELD_HEIGHT, -1, -1, 0, 
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

    for (s32 y = 0; y < PLAY_FIELD_HEIGHT; ++y) {
        for (s32 x = 0; x < PLAY_FIELD_WIDTH; ++x) {
            if ((x == 0) || (x == PLAY_FIELD_WIDTH-1) || (y == PLAY_FIELD_HEIGHT-1))
                play_field[y][x] = 8;
            else
                play_field[y][x] = 0;
        }
    }

    u8 piece_id = rand() % 7;
    s32 piece_x = PLAY_FIELD_WIDTH / 2;
    s32 piece_y = 0;
    s32 piece_rotation = 0;

    s32 counter_fin = 20;
    s32 counter_current = 0;
    bool move_down = false;
    bool game_over = false;
    bool is_line_filled = false;
    float global_line_alpha = 1.0f;

    if (id != -1) {
        bool ap_running = true;
        while (ap_running) {
            bender_update_window_events();

            float mx = (float)input->mouse_x;
            float my = (float)(render_target_height - input->mouse_y);

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

                    if (event.key_pressed && event.key_code == 'P') {
                        piece_id = (piece_id + 1) % PIECE_COUNT;
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


            if (!game_over) {
                if (input->button_states[B_KEY_ARROW_LEFT] & B_KEY_STATE_START) {
                    if (piece_test_occupancy(piece_id, piece_x - 1, piece_y, piece_rotation))
                        piece_x -= 1;
                }
                if (input->button_states[B_KEY_ARROW_RIGHT] & B_KEY_STATE_START) {
                    if (piece_test_occupancy(piece_id, piece_x + 1, piece_y, piece_rotation))
                        piece_x += 1;
                }
                if (input->button_states[B_KEY_ARROW_DOWN] & B_KEY_STATE_START) {
                    if (piece_test_occupancy(piece_id, piece_x, piece_y + 1, piece_rotation))
                        piece_y += 1;
                }

                if (input->button_states['R'] & B_KEY_STATE_START) {
                    if (piece_test_occupancy(piece_id, piece_x, piece_y, piece_rotation + 1))
                        piece_rotation = (piece_rotation + 1) % 4;
                }

                counter_current += 1;
                move_down = (counter_current == counter_fin);

                if (move_down) {
                    if (piece_test_occupancy(piece_id, piece_x, piece_y + 1, piece_rotation))
                        piece_y += 1;
                    else {
                        // Place piece in play field.
                        for (s32 y = 0; y < 4; ++y) {
                            for (s32 x = 0; x < 4; ++x) {
                                s32 rx = 0, ry = 0;
                                piece_rotate(x, y, piece_rotation, &rx, &ry);

                                if (piece_shapes[piece_id][ry][rx] != 0)
                                    play_field[piece_y + y][piece_x + x] = (piece_id + 1);
                            }
                        }

                        // Check lines.
                        for (s32 y = 0; y < 4; ++y) {
                            if (piece_y + y < PLAY_FIELD_HEIGHT - 1) {
                                bool full_line = true;

                                for (s32 x = 1; x < PLAY_FIELD_WIDTH - 1; ++x) {
                                    full_line &= (play_field[piece_y + y][x] != 0);
                                }

                                if (full_line) {
                                    for (s32 x = 1; x < PLAY_FIELD_WIDTH - 1; ++x) {
                                        play_field[piece_y + y][x] = 8;
                                    }

                                    is_line_filled = true;
                                    line_indices[line_indices_count] = piece_y + y;
                                    line_indices_count += 1;
                                }
                            }
                        }

                        // Reset piece.
                        piece_id = rand() % 7;
                        piece_x = PLAY_FIELD_WIDTH / 2;
                        piece_y = 0;
                        piece_rotation = 0;

                        // Run out of space.
                        if (!piece_test_occupancy(piece_id, piece_x, piece_y, piece_rotation)) {
                            game_over = true;
                            nb_write_string("GAME OVER\n", false);
                        }
                    }

                    counter_current = 0;
                }
            }

            if (is_line_filled) {
                global_line_alpha -= 0.033f;
                if (global_line_alpha <= 0) {
                    is_line_filled = false;
                    global_line_alpha = 1;

                    for (s32 index = 0; index < line_indices_count; ++index) {
                        s32 row = line_indices[index];
                        for (s32 x = 1; x < PLAY_FIELD_WIDTH - 1; ++x) {
                            for (s32 y = row; y > 0; --y) {
                                play_field[y][x] = play_field[y-1][x];
                            }

                            play_field[0][x] = 0;
                        }
                    }

                    line_indices_count = 0;
                }
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

            float offset_x = (render_target_width * 0.5f) - ((PLAY_FIELD_WIDTH * block_size) * 0.5f);

            {
                float *c = piece_colors[piece_id];
                for (s32 y = 0; y < 4; ++y) {
                    for (s32 x = 0; x < 4; ++x) {
                        s32 rx = 0, ry = 0;
                        piece_rotate(x, y, piece_rotation, &rx, &ry);
                        
                        u8 block = piece_shapes[piece_id][ry][rx];
                        if (block) {
                            s32 x_coord = 0, y_coord = 0;
                            play_field_to_right_handed_coords(piece_x + x, piece_y + y, &x_coord, &y_coord);

                            float x0 = offset_x + x_coord;
                            float x1 = x0 + block_size;
                            float y0 = (float)y_coord;
                            float y1 = y0 + block_size;

                            rm_immediate_quad(x0, y0, x1, y1, c[0], c[1], c[2], c[3]);
                        }
                    }
                }
            }

            for (s32 y = 0; y < PLAY_FIELD_HEIGHT; ++y) {
                for (s32 x = 0; x < PLAY_FIELD_WIDTH; ++x) {
                    u8 block = play_field[y][x];
                    if (block) {
                        float *c = piece_colors[block-1];
                        s32 x_coord = 0, y_coord = 0;
                        play_field_to_right_handed_coords(x, y, &x_coord, &y_coord);

                        float x0 = offset_x + x_coord;
                        float x1 = x0 + block_size;
                        float y0 = (float)y_coord;
                        float y1 = y0 + block_size;

                        rm_immediate_quad(x0, y0, x1, y1, c[0], c[1], c[2], c[3]);
                    }
                }
            }


            // Hor grid lines.
            {
                float x0 = offset_x;
                float x1 = x0 + block_size * PLAY_FIELD_WIDTH;
                
                for (u32 y = 0; y < PLAY_FIELD_HEIGHT + 1; ++y) {
                    float y0 = (float)y * block_size;

                    rm_immediate_quad(x0, y0, x1, y0 + 1,  0, 0, 0, 1);
                }
            }

            // Ver grid lines.
            {
                float y0 = 0;
                float y1 = y0 + block_size * PLAY_FIELD_HEIGHT;

                for (u32 x = 0; x < PLAY_FIELD_WIDTH + 1; ++x) {
                    float x0 = offset_x + (float)x * block_size;

                    rm_immediate_quad(x0, y0, x0 + 1, y1,  0, 0, 0, 1);
                }
            }

            rm_immediate_frame_end();

            rm_swap_buffers(id);
            bender_sleep_ms(50);
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
