// #define NB_STRIP_GENERAL_PREFIX
#include "nb.h"

#include "bender.h"

//
// This file is only here for the purpose of testing the engine API.
// In a real project, the entrypoint should be platform specific...
//

int main(void) {
    u32 id = bender_create_window("AJI", 640, 480, -1, -1, 0, 
                                  0, 
                                  B_WINDOW_BACKGROUND_COLOR);
    u32 win2_id = bender_create_window("child", 200, 200, -1, -1, id, 0, 
                                       B_WINDOW_BACKGROUND_COLOR);

    s32 w = 0, h = 0;
    bender_get_window_size(win2_id, &w, &h);
    print("child window = %dx%d\n", w, h);

    if (id) {
        bool ap_running = true;
        while (ap_running) {
            bender_update_window_events();

            BEvent event;
            while (bender_get_next_event(&event)) {
                if (event.type == B_EVENT_QUIT) ap_running = false;

                if (event.type == B_EVENT_WINDOW_RESIZE) {
                    print("Window sized = %dx%d\n", event.x, event.y);
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
        }
    }

    return 0;
}


#define NB_IMPLEMENTATION
#include "nb.h"

#include "bender/bender_windows.c"