// #define NB_STRIP_GENERAL_PREFIX
#include "general.h"

#include "bender.h"

//
// This file is only here for the purpose of testing the engine API.
// In a real project, the entrypoint should be platform specific...
//

int main(void) {
    u32 id = bender_create_window("AJI", 640, 480, -1, -1, 0, 
                                  B_WINDOW_CREATE_MAXIMIZED, 
                                  B_WINDOW_BACKGROUND_COLOR);
    if (id) {
        bool ap_running = true;
        while (ap_running) {
            bender_update_window_events();

            BEvent event;
            while (bender_get_next_event(&event)) {
                if (event.type == B_EVENT_QUIT) ap_running = false;

/*
                switch (event.type) {
                    case B_EVENT_QUIT: nb_write_string("QUIT\n", false); break;
                    case B_EVENT_WINDOW_RESIZE: nb_write_string("WINDOW_RESIZE\n", false); break;
                    case B_EVENT_KEYBOARD: nb_write_string("KEYBOARD\n", false); break;
                    case B_EVENT_TEXT_INPUT: nb_write_string("TEXT_INPUT\n", false); break;
                    case B_EVENT_MOUSE_WHEEL: nb_write_string("MOUSE_WHEEL\n", false); break;
                    case B_EVENT_MOUSE_H_WHEEL: nb_write_string("MOUSE_H_WHEEL\n", false); break;
                    case B_EVENT_TOUCH: nb_write_string("TOUCH\n", false); break;
                    case B_EVENT_DRAG_AND_DROP_FILES: nb_write_string("DRAG_AND_DROP_FILES\n", false); break;
                }
*/

                if (event.type == B_EVENT_WINDOW_RESIZE) {
                    print("Window sized = %dx%d\n", event.x, event.y);
                }

                if (event.type == B_EVENT_KEYBOARD) {
                    if (event.key_pressed && event.key_code == B_KEY_ESCAPE) ap_running = false;

                    if (event.key_pressed && event.key_code == B_KEY_ENTER) {
                        nb_write_string("ENTER press.\n", false);
                    }

                    if (event.key_pressed && event.key_code == B_KEY_ENTER && event.alt_pressed) {
                        nb_write_string("Enter Fullscreen.\n", false);
                    }

                    if (event.key_pressed && event.key_code == 'C' && event.ctrl_pressed) {
                        nb_write_string("Copy.\n", false);
                    }
                    if (event.key_pressed && event.key_code == 'X' && event.ctrl_pressed) {
                        nb_write_string("Cut.\n", false);
                    }
                    if (event.key_pressed && event.key_code == 'V' && event.ctrl_pressed) {
                        nb_write_string("Paste.\n", false);
                    }

                    if (event.key_pressed && event.key_code == 'Z' && event.ctrl_pressed) {
                        if (event.shift_pressed) {
                            nb_write_string("Redo.\n", false);
                        } else {
                            nb_write_string("Undo.\n", false);
                        }
                    }

                    if (event.key_pressed && event.key_code == B_MOUSE_BUTTON_RIGHT) {
                        nb_write_string("Left mouse.\n", false);
                    }
                }
            }

            if (b_input_button_states[B_MOUSE_BUTTON_LEFT] & B_KEY_STATE_START) {
                nb_write_string("BUTTON INPUT\n", false);
            }

            if (b_mouse_wheel_delta.vertical) {
                print("Wheel vertical = %d\n", (s32)(b_mouse_wheel_delta.vertical/b_typical_wheel_delta));
            }
        }
    }

    return 0;
}


#define NB_IMPLEMENTATION
#include "general.h"

#include "bender/bender_windows.c"