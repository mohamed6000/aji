// #define NB_STRIP_GENERAL_PREFIX
#include "general.h"

#include "bender.h"

//
// This file is only here for the purpose of testing the engine API.
// In a real project, the entrypoint should be platform specific...
//

int main(void) {
    u32 id = bender_create_window("AJI", 640, 480, -1, -1, 0, 0, B_WINDOW_BACKGROUND_COLOR);
    if (id) {
        bool ap_running = true;
        while (ap_running) {
            bender_update_window_events();

            BEvent event;
            while (bender_get_next_event(&event)) {
                if (event.type == B_EVENT_QUIT) ap_running = false;
            }
        }
    }

    return 0;
}


#define NB_IMPLEMENTATION
#include "general.h"

#include "bender/bender_windows.c"