// #define NB_STRIP_GENERAL_PREFIX
#include "nb.h"

#include "bender.h"

#include "renderman.h"

//
// This file is only here for the purpose of testing the engine API.
// In a real project, the entrypoint should be platform specific...
//

int main(void) {
    u32 id = bender_create_window("AJI", 640, 480, -1, -1, 0, 
                                  0, 
                                  B_WINDOW_BACKGROUND_COLOR);

    rm_init(id);
    
    // u32 win2_id = bender_create_window("child", 200, 200, -1, -1, id, 0, B_WINDOW_BACKGROUND_COLOR);

    s32 render_target_width  = 0;
    s32 render_target_height = 0;
    bender_get_window_size(id, &render_target_width, &render_target_height);
    print("window = %dx%d\n", render_target_width, render_target_height);

    bool is_fullscreen = false;


    if (id) {
        bool ap_running = true;
        while (ap_running) {
            bender_update_window_events();

            // s32 mx = 0, my = 0;
            // bender_get_mouse_pointer_position_right_handed(id, &mx, &my);
            // print("window mouse pos = %dx%d\n", mx, my);

            float mx = (float)b_input_state.mouse_x;
            float my = (float)(render_target_height - b_input_state.mouse_y);

            BEvent event;
            while (bender_get_next_event(&event)) {
                if (event.type == B_EVENT_QUIT) {
                    if (bender_messagebox_confirm("Quit AJI?", "Are you sure? You want to quit AJI?"))
                        ap_running = false;
                }

                if (event.type == B_EVENT_WINDOW_RESIZE) {
                    print("Window sized = %dx%d\n", event.x, event.y);
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

                    if (event.key_pressed && event.key_code == B_KEY_ENTER) {
                        nb_write_string("ENTER press.\n", false);
                    }

                    if (event.key_pressed && event.key_code == B_KEY_ENTER && (event.modifier_flags & B_MOD_ALT_PRESSED)) {
                        nb_write_string("Enter Fullscreen.\n", false);
                    }

                    if (event.key_pressed && event.key_code == 'C' && (event.modifier_flags & B_MOD_CTRL_PRESSED)) {
                        nb_write_string("Copy.\n", false);
                    }
                    if (event.key_pressed && event.key_code == 'X' && (event.modifier_flags & B_MOD_CTRL_PRESSED)) {
                        nb_write_string("Cut.\n", false);
                    }
                    if (event.key_pressed && event.key_code == 'V' && (event.modifier_flags & B_MOD_CTRL_PRESSED)) {
                        nb_write_string("Paste.\n", false);
                    }

                    if (event.key_pressed && event.key_code == 'Z' && (event.modifier_flags & B_MOD_CTRL_PRESSED)) {
                        if (event.modifier_flags & B_MOD_SHIFT_PRESSED) {
                            nb_write_string("Redo.\n", false);
                        } else {
                            nb_write_string("Undo.\n", false);
                        }
                    }

                    if (event.key_pressed && event.key_code == B_MOUSE_BUTTON_RIGHT) {
                        nb_write_string("Left mouse.\n", false);
                    }
                }

                nb_reset_temporary_storage();
            }

            if (b_input_state.button_states[B_MOUSE_BUTTON_LEFT] & B_KEY_STATE_START) {
                nb_write_string("BUTTON INPUT\n", false);
            }

            if (b_input_state.mouse_wheel_delta.vertical) {
                print("Wheel vertical = %d\n", (s32)(b_input_state.mouse_wheel_delta.vertical/b_input_state.typical_wheel_delta));
            }

            for (s32 index = 0; index < b_input_state.touch_pointer_count; ++index) {
                BTouch_Pointer *pointer = b_input_state.touch_pointers + index;

                const char *touch_event[4] = {"None", "Pressed", "Released", "Moved"};
                print("Touch %u: %s (%d, %d)\n", 
                      index, 
                      touch_event[pointer->type],
                      pointer->x, pointer->y);
            }


            rm_clear_render_target(0.18f, 0.34f, 0.34f, 1);

            rm_begin_rendering_2d((float)render_target_width, (float)render_target_height);

            rm_immediate_quad(mx, my, mx+10.0f, my+10.0f, 0, 1, 0, 1);

            rm_swap_buffers(id);
        }
    }

    return 0;
}


#define NB_IMPLEMENTATION
#include "nb.h"

#include "bender/bender_windows.c"

#include "renderman/renderman_d3d9.cpp"