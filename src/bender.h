#ifndef OS_INCLUDE_H
#define OS_INCLUDE_H
/*

    Bender the OS cross-platform layer.

*/

#include "general.h"

const float B_WINDOW_BACKGROUND_COLOR[3] = {0.15f, 0.15f, 0.15f};

struct BEvent;

NB_EXTERN u32 
bender_create_window(const char *title, s32 width, s32 height, 
                     s32 NB_DEFAULT_VALUE(window_x, -1), 
                     s32 NB_DEFAULT_VALUE(window_y, -1), 
                     u32 NB_DEFAULT_VALUE(window_parent_index, 0), 
                     u32 NB_DEFAULT_VALUE(window_creation_flags, 0), 
                     const float NB_DEFAULT_VALUE(background_color[3], B_WINDOW_BACKGROUND_COLOR));

NB_EXTERN void bender_update_window_events(void);

/*
    When using this function instead of iterating on events_this_frame array,
    we should handle freeing the dragged files array ourselves.

    when an EVENT_DRAG_AND_DROP_FILES is generated.
    call: array_free(&event.files);

    @Note: file names are never freed by our system, and it is up to the
    user to handle them.
*/
NB_EXTERN bool bender_get_next_event(struct BEvent *event);

NB_EXTERN void 
bender_get_window_size(u32 window_id, 
                       s32 *width_return, 
                       s32 *height_return);

NB_EXTERN void 
bender_get_mouse_pointer_position(u32 window_id, 
                                  s32 *x_return, 
                                  s32 *y_return);

NB_EXTERN void 
bender_get_mouse_pointer_position_right_handed(u32 window_id, 
                                               s32 *x_return, 
                                               s32 *y_return);

NB_EXTERN void bender_sleep_ms(u32 ms);

NB_EXTERN void bender_toggle_fullscreen(u32 window_id, bool want_fullscreen);

enum Bender_States {
    B_STATE_NONE,

    B_STATE_ABORT,
    B_STATE_RETRY,
    B_STATE_IGNORE,
};

NB_EXTERN void bender_messagebox_info(const char *title, const char *message);
NB_EXTERN bool bender_messagebox_confirm(const char *title, const char *message);
NB_EXTERN u32  bender_messagebox_abort(const char *title, const char *message);

enum Bender_Window_Creation_Flags {
    B_WINDOW_CREATE_NONE          = 0x0,
    B_WINDOW_CREATE_MAXIMIZED     = 0x1,
    B_WINDOW_CREATE_BORDERLESS    = 0x2,
    B_WINDOW_CREATE_DRAG_AND_DROP = 0x4,
};


// Event system.
typedef enum 
BEvent_Type NB_ENUM_TYPE(u8) {
    B_EVENT_NONE          = 0,
    B_EVENT_QUIT          = 1,
    B_EVENT_WINDOW_RESIZE = 2,
    B_EVENT_KEYBOARD      = 3,
    B_EVENT_TEXT_INPUT    = 4,
    B_EVENT_MOUSE_WHEEL   = 5,
    B_EVENT_MOUSE_V_WHEEL = B_EVENT_MOUSE_WHEEL,
    B_EVENT_MOUSE_H_WHEEL = 6,
    B_EVENT_TOUCH         = 7,
    B_EVENT_DRAG_AND_DROP_FILES = 8,

    B_EVENT_COUNT
} BEvent_Type;

typedef enum 
BKey_Code NB_ENUM_TYPE(u16) {
    B_KEY_UNKNOWN = 0,  // Not assigned.

    B_KEY_BACKSPACE = 8,
    B_KEY_TAB       = 9,
    B_KEY_LINEFEED  = 10,
    B_KEY_ENTER     = 13,
    B_KEY_ESCAPE    = 21,
    B_KEY_SPACE     = 32,

    B_KEY_APOSTROPHE = 39,

    B_KEY_PLUS  = 43,
    B_KEY_COMMA = 44,
    B_KEY_DASH  = 45,
    B_KEY_DOT   = 46,
    B_KEY_FORWARD_SLASH = 47,

    // ASCII Input keys.
    // 0..9 -> 48..57
    // A..Z -> 65..90

    B_KEY_NUMBER_0 = 48,
    B_KEY_NUMBER_1 = 49,
    B_KEY_NUMBER_2 = 50,
    B_KEY_NUMBER_3 = 51,
    B_KEY_NUMBER_4 = 52,
    B_KEY_NUMBER_5 = 53,
    B_KEY_NUMBER_6 = 54,
    B_KEY_NUMBER_7 = 55,
    B_KEY_NUMBER_8 = 56,
    B_KEY_NUMBER_9 = 57,

    B_KEY_COLON      = 58,
    B_KEY_SEMI_COLON = 59,

    B_KEY_A = 65,
    B_KEY_B = 66,
    B_KEY_C = 67,
    B_KEY_D = 68,
    B_KEY_E = 69,
    B_KEY_F = 70,
    B_KEY_G = 71,
    B_KEY_H = 72,
    B_KEY_I = 73,
    B_KEY_J = 74,
    B_KEY_K = 75,
    B_KEY_L = 76,
    B_KEY_M = 77,
    B_KEY_N = 78,
    B_KEY_O = 79,
    B_KEY_P = 80,
    B_KEY_Q = 81,
    B_KEY_R = 82,
    B_KEY_S = 83,
    B_KEY_T = 84,
    B_KEY_U = 85,
    B_KEY_V = 86,
    B_KEY_W = 87,
    B_KEY_X = 88,
    B_KEY_Y = 89,
    B_KEY_Z = 90,

    B_KEY_LEFT_BRACKET  = 91,
    B_KEY_BACK_SLASH    = 92,
    B_KEY_RIGHT_BRACKET = 93,

    B_KEY_BACK_TICK = 96,
    
    B_KEY_DELETE    = 127,

    B_KEY_ARROW_LEFT,
    B_KEY_ARROW_UP,
    B_KEY_ARROW_RIGHT,
    B_KEY_ARROW_DOWN,

    B_KEY_SHIFT,
    B_KEY_CTRL,
    B_KEY_ALT,
    B_KEY_CMD,
    B_KEY_META = B_KEY_CMD,

    B_KEY_PAUSE,
    B_KEY_CAPS_LOCK,

    B_KEY_PAGE_UP,
    B_KEY_PAGE_DOWN,
    B_KEY_HOME,
    B_KEY_END,
    
    B_KEY_PRINT_SCREEN,

    B_KEY_INSERT,

    B_KEY_NUMPAD_0,
    B_KEY_NUMPAD_1,
    B_KEY_NUMPAD_2,
    B_KEY_NUMPAD_3,
    B_KEY_NUMPAD_4,
    B_KEY_NUMPAD_5,
    B_KEY_NUMPAD_6,
    B_KEY_NUMPAD_7,
    B_KEY_NUMPAD_8,
    B_KEY_NUMPAD_9,

    B_KEY_NUMPAD_MULTIPLY,
    B_KEY_NUMPAD_ADD,
    B_KEY_NUMPAD_SUBTRACT,
    B_KEY_NUMPAD_DECIMAL,
    B_KEY_NUMPAD_DIVIDE,
    B_KEY_NUMPAD_ENTER,

    B_KEY_F1,
    B_KEY_F2,
    B_KEY_F3,
    B_KEY_F4,
    B_KEY_F5,
    B_KEY_F6,
    B_KEY_F7,
    B_KEY_F8,
    B_KEY_F9,
    B_KEY_F10,
    B_KEY_F11,
    B_KEY_F12,
    B_KEY_F13,
    B_KEY_F14,
    B_KEY_F15,
    B_KEY_F16,
    B_KEY_F17,
    B_KEY_F18,
    B_KEY_F19,
    B_KEY_F20,
    B_KEY_F21,
    B_KEY_F22,
    B_KEY_F23,
    B_KEY_F24,

    B_KEY_NUM_LOCK,
    B_KEY_SCROLL_LOCK,

    B_MOUSE_BUTTON_LEFT,
    B_MOUSE_BUTTON_MIDDLE,
    B_MOUSE_BUTTON_RIGHT,

    B_MOUSE_BUTTON_X1,
    B_MOUSE_BUTTON_X2,

    B_KEY_CODE_COUNT
} BKey_Code;

typedef enum 
BTouch_Type NB_ENUM_TYPE(u8) {
    B_TOUCH_PRESSED  = 1,
    B_TOUCH_RELEASED = 2,
    B_TOUCH_MOVED    = 3,
} BTouch_Type;

// @Todo: packing.
typedef struct BEvent {
    BEvent_Type NB_DEFAULT_VALUE(type, B_EVENT_NONE);
    
    // Modifiers state, maybe replace those with an enum.
    bool alt_pressed;
    bool cmd_pressed;  // CMD on MacOS, Win on Windows, Meta on linux.
    bool ctrl_pressed;
    bool shift_pressed;

    void *os_handle;  // Low level handle of the platform window.

    // Window event.
    s32 x;
    s32 y;

    // Keyboard event.
    BKey_Code key_code;
    bool key_pressed;
    bool repeat;

    // Text input event.
    u32 utf32;

    // Mouse wheel event.
    s32 wheel_delta;

    // Touch event.
    BTouch_Type touch_type;
    u32 touch_index;

    // Drag and drop files event.
    // Array<char *> files;
} BEvent;

enum Bender_Key_State {
    B_KEY_STATE_NONE  = 0x0,
    B_KEY_STATE_DOWN  = 0x1,
    B_KEY_STATE_START = 0x2,
    B_KEY_STATE_END   = 0x4,
};

typedef struct BWheel_Delta {
    s32 vertical;
    s32 horizontal;
} BWheel_Delta;

typedef struct BTouch_Pointer {
    s32 x;
    s32 y;
    BTouch_Type type;
} BTouch_Pointer;


extern u32 b_input_button_states[B_KEY_CODE_COUNT];
extern BWheel_Delta b_mouse_wheel_delta;
extern s32 b_typical_wheel_delta;

extern s32 b_mouse_delta_x;
extern s32 b_mouse_delta_y;

extern s32 b_touch_pointer_count;
extern BTouch_Pointer b_touch_pointers[2];

extern bool b_input_application_has_focus;

NB_INLINE u32 
bender_get_input_button_state(BKey_Code key_code) {
    return b_input_button_states[key_code];
}

// NB_EXTERN bool bender_window_has_handle(Window_Type *window, void *handle);

#endif  // OS_INCLUDE_H
