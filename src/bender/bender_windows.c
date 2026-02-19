#include "bender.h"

BInput_State b_input_state;


#if OS_WINDOWS

#ifdef NB_INCLUDE_WINDEFS
#include "windefs.h"
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define UNICODE
#define _UNICODE

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#endif

#if COMPILER_CL
#pragma comment(lib, "Gdi32.lib")    // For CreateSolidBrush.
#pragma comment(lib, "Shell32.lib")  // For ExtractIconW.
#endif


typedef struct {
    HWND handle;
    RECT rect;
    u32 style;
    u32 ex_style;
} Bender_Window_Record;

static Bender_Window_Record *b_window_record_storage;
static u32 b_window_record_count = 1;
static u32 b_window_record_allocated;
static HINSTANCE b_w32_instance;
static WCHAR BENDER_DEFAULT_WINDOW_CLASS_NAME[] = L"BENDER_DEFAULT_WINDOW_CLASS";

static bool b_initted = false;

static u32 b_w32_high_surrogate;

static int b_w32_mouse_abs_x = 0;
static int b_w32_mouse_abs_y = 0;

#define B_MAX_TOUCH_POINT_INPUT_COUNT 4
static TOUCHINPUT b_w32_touch_inputs[B_MAX_TOUCH_POINT_INPUT_COUNT];

// static DEVMODEW b_w32_target_device_mode;
// static WCHAR b_w32_target_device_name[32];
// #define BENDER_TARGET_FREQUENCY_MIN 59
// #define BENDER_TARGET_FREQUENCY_MAX 60

NB_INLINE void b_push_event(BEvent event) {
    assert(b_input_state.event_count < nb_array_count(b_input_state.events_this_frame));
    u32 index = b_input_state.event_count;
    b_input_state.event_count += 1;
    b_input_state.events_this_frame[index] = event;
}

NB_INLINE Bender_Window_Record *
b_get_window_record(u32 index) {
    assert(index < b_window_record_count);
    Bender_Window_Record *result = null;
    if (index) {
        result = b_window_record_storage + index;
    }

    return result;
}

NB_EXTERN void *
b_get_window_handle(u32 index) {
    void *result = null;

    Bender_Window_Record *record = b_get_window_record(index);
    if (record) {
        result = record->handle;
    }

    return result;
}

NB_INLINE u32 b_float_to_u32_color_channel(float f) {
    u32 u = (u32)(f * 255);
    if (u < 0)   u = 0;
    if (u > 255) u = 255;

    return u;
}

static s32 b_w32_vk_codes[B_KEY_CODE_COUNT] = {
    0,  // B_KEY_UNKNOWN,  // Not assigned.

    0, 0, 0, 0, 0, 0, 0,

    VK_BACK, // KEY_BACKSPACE = 8,
    VK_TAB,  // KEY_TAB       = 9,
    10,      // KEY_LINEFEED  = 10,

    0, 0,

    VK_RETURN, // KEY_ENTER = 13,

    0, 0, 0, 0, 0, 0, 0,

    VK_ESCAPE,  // KEY_ESCAPE = 21,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    VK_SPACE,  // KEY_SPACE = 32,

    0, 0, 0, 0, 0, 0,

    VK_OEM_7,      // ''' KEY_APOSTROPHE
    0, 0, 0, 
    VK_OEM_PLUS,   // '+' KEY_PLUS
    VK_OEM_COMMA,  // ',' KEY_COMMA
    VK_OEM_MINUS,  // '-' KEY_DASH
    VK_OEM_PERIOD, // '.' KEY_DOT
    VK_OEM_2,      // '/' KEY_FORWARD_SLASH

    // ASCII Input keys.
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57,  // 0..9 -> 48..57

    0,        // ':' KEY_COLON
    VK_OEM_1, // ';' KEY_SEMI_COLON

    0, 0, 0, 0, 0,

    // A..Z -> 65..90
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 
    75, 76, 77, 78, 79, 80, 81, 82, 83, 84,
    85, 86, 87, 88, 89, 90,

    VK_OEM_4, // '[' KEY_LEFT_BRACKET
    VK_OEM_5, // '\' KEY_BACK_SLASH
    VK_OEM_6, // ']' KEY_RIGHT_BRACKET
    0, 0, 
    VK_OEM_3, // '`' KEY_BACK_TICK
    
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    
    VK_DELETE,  // KEY_DELETE = 127,

    VK_LEFT,  // KEY_ARROW_LEFT,
    VK_UP,    // KEY_ARROW_UP,
    VK_RIGHT, // KEY_ARROW_RIGHT,
    VK_DOWN,  // KEY_ARROW_DOWN,

    VK_SHIFT,    // B_KEY_SHIFT,
    VK_CONTROL,  // B_KEY_CTRL,
    VK_MENU,     // B_KEY_ALT,
    VK_APPS,     // B_KEY_CMD,

    VK_PAUSE,    // KEY_PAUSE,
    VK_CAPITAL,  // KEY_CAPS_LOCK,

    VK_PRIOR, // KEY_PAGE_UP,
    VK_NEXT,  // KEY_PAGE_DOWN,
    VK_HOME,  // KEY_HOME,
    VK_END,   // KEY_END,
    
    VK_SNAPSHOT, // KEY_PRINT_SCREEN,
    VK_INSERT,   // KEY_INSERT,

    VK_NUMPAD0,  // KEY_NUMPAD_0,
    VK_NUMPAD1,  // KEY_NUMPAD_1,
    VK_NUMPAD2,  // KEY_NUMPAD_2,
    VK_NUMPAD3,  // KEY_NUMPAD_3,
    VK_NUMPAD4,  // KEY_NUMPAD_4,
    VK_NUMPAD5,  // KEY_NUMPAD_5,
    VK_NUMPAD6,  // KEY_NUMPAD_6,
    VK_NUMPAD7,  // KEY_NUMPAD_7,
    VK_NUMPAD8,  // KEY_NUMPAD_8,
    VK_NUMPAD9,  // KEY_NUMPAD_9,

    VK_MULTIPLY,  // KEY_NUMPAD_MULTIPLY,
    VK_ADD,       // KEY_NUMPAD_ADD,
    VK_SUBTRACT,  // KEY_NUMPAD_SUBTRACT,
    VK_DECIMAL,   // KEY_NUMPAD_DECIMAL,
    VK_DIVIDE,    // KEY_NUMPAD_DIVIDE,
    VK_RETURN,    // KEY_NUMPAD_ENTER,

    VK_F1,       // KEY_F1,
    VK_F2,       // KEY_F2,
    VK_F3,       // KEY_F3,
    VK_F4,       // KEY_F4,
    VK_F5,       // KEY_F5,
    VK_F6,       // KEY_F6,
    VK_F7,       // KEY_F7,
    VK_F8,       // KEY_F8,
    VK_F9,       // KEY_F9,
    VK_F10,      // KEY_F10,
    VK_F11,      // KEY_F11,
    VK_F12,      // KEY_F12,
    VK_F13,      // KEY_F13,
    VK_F14,      // KEY_F14,
    VK_F15,      // KEY_F15,
    VK_F16,      // KEY_F16,
    VK_F17,      // KEY_F17,
    VK_F18,      // KEY_F18,
    VK_F19,      // KEY_F19,
    VK_F20,      // KEY_F20,
    VK_F21,      // KEY_F21,
    VK_F22,      // KEY_F22,
    VK_F23,      // KEY_F23,
    VK_F24,      // KEY_F24,

    VK_NUMLOCK,  // KEY_NUM_LOCK,
    VK_SCROLL,   // KEY_SCROLL_LOCK,

    VK_LBUTTON,  // B_MOUSE_BUTTON_LEFT
    VK_MBUTTON,  // B_MOUSE_BUTTON_MIDDLE
    VK_RBUTTON,  // B_MOUSE_BUTTON_RIGHT
    VK_XBUTTON1, // B_MOUSE_BUTTON_X1
    VK_XBUTTON2, // B_MOUSE_BUTTON_X2
};

static BKey_Code b_w32_key_codes[] = {
    B_KEY_UNKNOWN,

    B_MOUSE_BUTTON_LEFT,   //VK_LBUTTON  0x01    Left mouse button
    B_MOUSE_BUTTON_RIGHT,  //VK_RBUTTON  0x02    Right mouse button
    B_KEY_UNKNOWN,         //VK_CANCEL   0x03    Control-break processing
    B_MOUSE_BUTTON_MIDDLE, //VK_MBUTTON  0x04    Middle mouse button
    B_MOUSE_BUTTON_X1,     //VK_XBUTTON1     0x05    X1 mouse button
    B_MOUSE_BUTTON_X2,     //VK_XBUTTON2     0x06    X2 mouse button
    
    B_KEY_UNKNOWN, //0x07    Reserved
    
    B_KEY_BACKSPACE,   //VK_BACK     0x08    Backspace key
    B_KEY_TAB,         //VK_TAB  0x09    Tab key
    B_KEY_LINEFEED, B_KEY_UNKNOWN, //0x0A-0B     Reserved
    B_KEY_UNKNOWN,     //VK_CLEAR    0x0C    Clear key
    B_KEY_ENTER,       //VK_RETURN   0x0D    Enter key
        
    B_KEY_UNKNOWN, B_KEY_UNKNOWN, //0x0E-0F     Unassigned
    B_KEY_SHIFT, //VK_SHIFT    0x10    Shift key
    B_KEY_CTRL,  //VK_CONTROL  0x11    Ctrl key
    B_KEY_ALT,   //VK_MENU     0x12    Alt key
    B_KEY_PAUSE, //VK_PAUSE    0x13    Pause key
    B_KEY_CAPS_LOCK, //VK_CAPITAL  0x14    Caps lock key
    
    B_KEY_UNKNOWN, //VK_KANA     0x15    IME Kana mode
                 //VK_HANGUL   0x15    IME Hangul mode
    B_KEY_UNKNOWN, //VK_IME_ON   0x16    IME On
    B_KEY_UNKNOWN, //VK_JUNJA    0x17    IME Junja mode
    B_KEY_UNKNOWN, //VK_FINAL    0x18    IME final mode
    B_KEY_UNKNOWN, //VK_HANJA    0x19    IME Hanja mode
                 //VK_KANJI    0x19    IME Kanji mode
    B_KEY_UNKNOWN, //VK_IME_OFF  0x1A    IME Off
    
    B_KEY_ESCAPE, //VK_ESCAPE   0x1B    Esc key
    
    B_KEY_UNKNOWN, //VK_CONVERT  0x1C    IME convert
    B_KEY_UNKNOWN, //VK_NONCONVERT   0x1D    IME nonconvert
    B_KEY_UNKNOWN, //VK_ACCEPT   0x1E    IME accept
    B_KEY_UNKNOWN, //VK_MODECHANGE   0x1F    IME mode change request
    
    B_KEY_SPACE,     //VK_SPACE    0x20    Spacebar key
    B_KEY_PAGE_UP,   //VK_PRIOR    0x21    Page up key
    B_KEY_PAGE_DOWN, //VK_NEXT     0x22    Page down key
    B_KEY_END,       //VK_END  0x23    End key
    B_KEY_HOME,      //VK_HOME     0x24    Home key
    
    B_KEY_ARROW_LEFT,  //VK_LEFT     0x25    Left arrow key
    B_KEY_ARROW_UP,    //VK_UP   0x26    Up arrow key
    B_KEY_ARROW_RIGHT, //VK_RIGHT    0x27    Right arrow key
    B_KEY_ARROW_DOWN,  //VK_DOWN     0x28    Down arrow key
    
    B_KEY_UNKNOWN, //VK_SELECT   0x29    Select key
    B_KEY_UNKNOWN, //VK_PRINT    0x2A    Print key
    B_KEY_UNKNOWN, //VK_EXECUTE  0x2B    Execute key
    
    B_KEY_PRINT_SCREEN, //VK_SNAPSHOT     0x2C    Print screen key
    B_KEY_INSERT,       //VK_INSERT   0x2D    Insert key
    B_KEY_DELETE,       //VK_DELETE   0x2E    Delete key
    B_KEY_UNKNOWN,      //VK_HELP     0x2F    Help key

    B_KEY_NUMBER_0,    // 0 key
    B_KEY_NUMBER_1,    // 1 key
    B_KEY_NUMBER_2,    // 2 key
    B_KEY_NUMBER_3,    // 3 key
    B_KEY_NUMBER_4,    // 4 key
    B_KEY_NUMBER_5,    // 5 key
    B_KEY_NUMBER_6,    // 6 key
    B_KEY_NUMBER_7,    // 7 key
    B_KEY_NUMBER_8,    // 8 key
    B_KEY_NUMBER_9,    // 9 key

    // 0x3A-40     Undefined
    B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, 
    B_KEY_UNKNOWN, B_KEY_UNKNOWN,

    B_KEY_A,    // A key
    B_KEY_B,    // B key
    B_KEY_C,    // C key
    B_KEY_D,    // D key
    B_KEY_E,    // E key
    B_KEY_F,    // F key
    B_KEY_G,    // G key
    B_KEY_H,    // H key
    B_KEY_I,    // I key
    B_KEY_J,    // J key
    B_KEY_K,    // K key
    B_KEY_L,    // L key
    B_KEY_M,    // M key
    B_KEY_N,    // N key
    B_KEY_O,    // O key
    B_KEY_P,    // P key
    B_KEY_Q,    // Q key
    B_KEY_R,    // R key
    B_KEY_S,    // S key
    B_KEY_T,    // T key
    B_KEY_U,    // U key
    B_KEY_V,    // V key
    B_KEY_W,    // W key
    B_KEY_X,    // X key
    B_KEY_Y,    // Y key
    B_KEY_Z,    // Z key

    B_KEY_CMD, // VK_LWIN     0x5B    Left Windows logo key
    B_KEY_CMD, // VK_RWIN     0x5C    Right Windows logo key
    B_KEY_CMD, // VK_APPS     0x5D    Application key
    
    B_KEY_UNKNOWN, //0x5E    Reserved
    B_KEY_UNKNOWN, //VK_SLEEP    0x5F    Computer Sleep key
    
    B_KEY_NUMPAD_0, // VK_NUMPAD0  0x60    Numeric keypad 0 key
    B_KEY_NUMPAD_1, // VK_NUMPAD1  0x61    Numeric keypad 1 key
    B_KEY_NUMPAD_2, // VK_NUMPAD2  0x62    Numeric keypad 2 key
    B_KEY_NUMPAD_3, // VK_NUMPAD3  0x63    Numeric keypad 3 key
    B_KEY_NUMPAD_4, // VK_NUMPAD4  0x64    Numeric keypad 4 key
    B_KEY_NUMPAD_5, // VK_NUMPAD5  0x65    Numeric keypad 5 key
    B_KEY_NUMPAD_6, // VK_NUMPAD6  0x66    Numeric keypad 6 key
    B_KEY_NUMPAD_7, // VK_NUMPAD7  0x67    Numeric keypad 7 key
    B_KEY_NUMPAD_8, // VK_NUMPAD8  0x68    Numeric keypad 8 key
    B_KEY_NUMPAD_9, // VK_NUMPAD9  0x69    Numeric keypad 9 key
    B_KEY_NUMPAD_MULTIPLY, // VK_MULTIPLY     0x6A    Multiply key
    B_KEY_NUMPAD_ADD,      // VK_ADD  0x6B    Add key
    
    B_KEY_UNKNOWN, //VK_SEPARATOR    0x6C    Separator key
    
    B_KEY_NUMPAD_SUBTRACT, // VK_SUBTRACT     0x6D    Subtract key
    B_KEY_NUMPAD_DECIMAL,  // VK_DECIMAL  0x6E    Decimal key
    B_KEY_NUMPAD_DIVIDE,   // VK_DIVIDE   0x6F    Divide key
    
    B_KEY_F1,  // VK_F1   0x70    F1 key
    B_KEY_F2,  // VK_F2   0x71    F2 key
    B_KEY_F3,  // VK_F3   0x72    F3 key
    B_KEY_F4,  // VK_F4   0x73    F4 key
    B_KEY_F5,  // VK_F5   0x74    F5 key
    B_KEY_F6,  // VK_F6   0x75    F6 key
    B_KEY_F7,  // VK_F7   0x76    F7 key
    B_KEY_F8,  // VK_F8   0x77    F8 key
    B_KEY_F9,  // VK_F9   0x78    F9 key
    B_KEY_F10, // VK_F10  0x79    F10 key
    B_KEY_F11, // VK_F11  0x7A    F11 key
    B_KEY_F12, // VK_F12  0x7B    F12 key
    B_KEY_F13, // VK_F13  0x7C    F13 key
    B_KEY_F14, // VK_F14  0x7D    F14 key
    B_KEY_F15, // VK_F15  0x7E    F15 key
    B_KEY_F16, // VK_F16  0x7F    F16 key
    B_KEY_F17, // VK_F17  0x80    F17 key
    B_KEY_F18, // VK_F18  0x81    F18 key
    B_KEY_F19, // VK_F19  0x82    F19 key
    B_KEY_F20, // VK_F20  0x83    F20 key
    B_KEY_F21, // VK_F21  0x84    F21 key
    B_KEY_F22, // VK_F22  0x85    F22 key
    B_KEY_F23, // VK_F23  0x86    F23 key
    B_KEY_F24, // VK_F24  0x87    F24 key

    // 0x88-8F     Reserved
    B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, 
    B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN,

    B_KEY_NUM_LOCK,    // VK_NUMLOCK  0x90    Num lock key
    B_KEY_SCROLL_LOCK, // VK_SCROLL   0x91    Scroll lock key
    
    // 0x92-96     OEM specific
    B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN,
    // 0x97-9F     Unassigned
    B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, 
    B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN, B_KEY_UNKNOWN,

    B_KEY_SHIFT, // VK_LSHIFT   0xA0    Left Shift key
    B_KEY_SHIFT, // VK_RSHIFT   0xA1    Right Shift key
    B_KEY_CTRL,  // VK_LCONTROL     0xA2    Left Ctrl key
    B_KEY_CTRL,  // VK_RCONTROL     0xA3    Right Ctrl key
    B_KEY_CMD,   // VK_LMENU    0xA4    Left Alt key
    B_KEY_CMD,   // VK_RMENU    0xA5    Right Alt key
    
    B_KEY_UNKNOWN, // VK_BROWSER_BACK     0xA6    Browser Back key
    B_KEY_UNKNOWN, // VK_BROWSER_FORWARD  0xA7    Browser Forward key
    B_KEY_UNKNOWN, // VK_BROWSER_REFRESH  0xA8    Browser Refresh key
    B_KEY_UNKNOWN, // VK_BROWSER_STOP     0xA9    Browser Stop key
    B_KEY_UNKNOWN, // VK_BROWSER_SEARCH   0xAA    Browser Search key
    B_KEY_UNKNOWN, // VK_BROWSER_FAVORITES    0xAB    Browser Favorites key
    B_KEY_UNKNOWN, // VK_BROWSER_HOME     0xAC    Browser Start and Home key
    B_KEY_UNKNOWN, // VK_VOLUME_MUTE  0xAD    Volume Mute key
    B_KEY_UNKNOWN, // VK_VOLUME_DOWN  0xAE    Volume Down key
    B_KEY_UNKNOWN, // VK_VOLUME_UP    0xAF    Volume Up key
    B_KEY_UNKNOWN, // VK_MEDIA_NEXT_TRACK     0xB0    Next Track key
    B_KEY_UNKNOWN, // VK_MEDIA_PREV_TRACK     0xB1    Previous Track key
    B_KEY_UNKNOWN, // VK_MEDIA_STOP   0xB2    Stop Media key
    B_KEY_UNKNOWN, // VK_MEDIA_PLAY_PAUSE     0xB3    Play/Pause Media key
    B_KEY_UNKNOWN, // VK_LAUNCH_MAIL  0xB4    Start Mail key
    B_KEY_UNKNOWN, // VK_LAUNCH_MEDIA_SELECT  0xB5    Select Media key
    B_KEY_UNKNOWN, // VK_LAUNCH_APP1  0xB6    Start Application 1 key
    B_KEY_UNKNOWN, // VK_LAUNCH_APP2  0xB7    Start Application 2 key

    // 0xB8-B9     Reserved
    B_KEY_UNKNOWN, B_KEY_UNKNOWN,

    B_KEY_SEMI_COLON, // VK_OEM_1    0xBA    It can vary by keyboard. For the US ANSI keyboard , the Semiсolon and Colon key
    B_KEY_PLUS, // VK_OEM_PLUS     0xBB    For any country/region, the Equals and Plus key
    B_KEY_COMMA, // VK_OEM_COMMA    0xBC    For any country/region, the Comma and Less Than key
    B_KEY_DASH, // VK_OEM_MINUS    0xBD    For any country/region, the Dash and Underscore key
    B_KEY_DOT, // VK_OEM_PERIOD   0xBE    For any country/region, the Period and Greater Than key
    B_KEY_FORWARD_SLASH, // VK_OEM_2    0xBF    It can vary by keyboard. For the US ANSI keyboard, the Forward Slash and Question Mark key
    B_KEY_BACK_TICK, // VK_OEM_3    0xC0    It can vary by keyboard. For the US ANSI keyboard, the Grave Accent and Tilde key
    
    B_KEY_UNKNOWN, B_KEY_UNKNOWN, // 0xC1-C2     Reserved
    
    B_KEY_UNKNOWN, // VK_GAMEPAD_A    0xC3    Gamepad A button
    B_KEY_UNKNOWN, // VK_GAMEPAD_B    0xC4    Gamepad B button
    B_KEY_UNKNOWN, // VK_GAMEPAD_X    0xC5    Gamepad X button
    B_KEY_UNKNOWN, // VK_GAMEPAD_Y    0xC6    Gamepad Y button
    B_KEY_UNKNOWN, // VK_GAMEPAD_RIGHT_SHOULDER   0xC7    Gamepad Right Shoulder button
    B_KEY_UNKNOWN, // VK_GAMEPAD_LEFT_SHOULDER    0xC8    Gamepad Left Shoulder button
    B_KEY_UNKNOWN, // VK_GAMEPAD_LEFT_TRIGGER     0xC9    Gamepad Left Trigger button
    B_KEY_UNKNOWN, // VK_GAMEPAD_RIGHT_TRIGGER    0xCA    Gamepad Right Trigger button
    B_KEY_UNKNOWN, // VK_GAMEPAD_DPAD_UP  0xCB    Gamepad D-pad Up button
    B_KEY_UNKNOWN, // VK_GAMEPAD_DPAD_DOWN    0xCC    Gamepad D-pad Down button
    B_KEY_UNKNOWN, // VK_GAMEPAD_DPAD_LEFT    0xCD    Gamepad D-pad Left button
    B_KEY_UNKNOWN, // VK_GAMEPAD_DPAD_RIGHT   0xCE    Gamepad D-pad Right button
    B_KEY_UNKNOWN, // VK_GAMEPAD_MENU     0xCF    Gamepad Menu/Start button
    B_KEY_UNKNOWN, // VK_GAMEPAD_VIEW     0xD0    Gamepad View/Back button
    B_KEY_UNKNOWN, // VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON   0xD1    Gamepad Left Thumbstick button
    B_KEY_UNKNOWN, // VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON  0xD2    Gamepad Right Thumbstick button
    B_KEY_UNKNOWN, // VK_GAMEPAD_LEFT_THUMBSTICK_UP   0xD3    Gamepad Left Thumbstick up
    B_KEY_UNKNOWN, // VK_GAMEPAD_LEFT_THUMBSTICK_DOWN     0xD4    Gamepad Left Thumbstick down
    B_KEY_UNKNOWN, // VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT    0xD5    Gamepad Left Thumbstick right
    B_KEY_UNKNOWN, // VK_GAMEPAD_LEFT_THUMBSTICK_LEFT     0xD6    Gamepad Left Thumbstick left
    B_KEY_UNKNOWN, // VK_GAMEPAD_RIGHT_THUMBSTICK_UP  0xD7    Gamepad Right Thumbstick up
    B_KEY_UNKNOWN, // VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN    0xD8    Gamepad Right Thumbstick down
    B_KEY_UNKNOWN, // VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT   0xD9    Gamepad Right Thumbstick right
    B_KEY_UNKNOWN, // VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT    0xDA    Gamepad Right Thumbstick left
    
    B_KEY_LEFT_BRACKET,  // VK_OEM_4    0xDB    It can vary by keyboard. For the US ANSI keyboard, the Left Brace key
    B_KEY_BACK_SLASH,    // VK_OEM_5    0xDC    It can vary by keyboard. For the US ANSI keyboard, the Backslash and Pipe key
    B_KEY_RIGHT_BRACKET, // VK_OEM_6    0xDD    It can vary by keyboard. For the US ANSI keyboard, the Right Brace key
    B_KEY_APOSTROPHE,    // VK_OEM_7    0xDE    It can vary by keyboard. For the US ANSI keyboard, the Apostrophe and Double Quotation Mark key
};

NB_INLINE BKey_Code 
b_w32_get_key_code(WPARAM vk_code, bool extended) {
    if (extended) {
        if (vk_code == VK_RETURN) return B_KEY_NUMPAD_ENTER;
    }

    return b_w32_key_codes[vk_code];
}

NB_INLINE void 
b_w32_send_keyboard_event(BKey_Code key_code, 
                          bool is_down, 
                          bool repeat, 
                          u32  key_current_state) {
    if (!is_down && !repeat) return; // Redundant key release.

    BEvent event;
    event.type = B_EVENT_KEYBOARD;
    event.x = 0;
    event.y = 0;
    event.utf32 = 0;

    event.key_code    = key_code;
    event.key_pressed = is_down;
    event.repeat      = repeat;
    
    event.modifier_flags = 0;
    event.modifier_flags |= b_input_state.alt_state   ? B_MOD_ALT_PRESSED   : 0;
    event.modifier_flags |= b_input_state.cmd_state   ? B_MOD_CMD_PRESSED   : 0;
    event.modifier_flags |= b_input_state.ctrl_state  ? B_MOD_CTRL_PRESSED  : 0;
    event.modifier_flags |= b_input_state.shift_state ? B_MOD_SHIFT_PRESSED : 0;
    
    b_push_event(event);

    b_input_state.button_states[key_code] |= key_current_state;
}

static void 
b_w32_process_raw_input(HRAWINPUT handle) {
    UNUSED(handle);
#if 0
    UINT data_size;
    GetRawInputData(handle, RID_INPUT, null, &data_size, size_of(RAWINPUTHEADER));

    if (raw_input_buffer.count < data_size) {
        if (raw_input_buffer.allocated < data_size) {
            // array_resize(&raw_input_buffer, data_size);
        }
        raw_input_buffer.count = data_size;
    }

    UINT copied_bytes = GetRawInputData(handle, RID_INPUT, raw_input_buffer.data, &data_size, size_of(RAWINPUTHEADER));
    assert(copied_bytes <= data_size);

    RAWINPUT *raw = (RAWINPUT *)raw_input_buffer.data;
    if (raw->header.dwType == RIM_TYPEMOUSE) {
        RAWMOUSE *mouse_data = &raw->data.mouse;

        if (mouse_data->usFlags & MOUSE_MOVE_ABSOLUTE) {
            // @Note: This doesn't account for virtual desktop mapping (multiple monitor system).
            int screen_x = GetSystemMetrics(SM_CXSCREEN);
            int screen_y = GetSystemMetrics(SM_CYSCREEN);

            s32 x = (s32)((s64)(mouse_data->lLastX * screen_x) / MAX_U16);
            s32 y = (s32)((s64)(mouse_data->lLastY * screen_y) / MAX_U16);

            mouse_delta_x += x - w32_mouse_abs_x;
            mouse_delta_y += y - w32_mouse_abs_y;

            w32_mouse_abs_x = x;
            w32_mouse_abs_y = y;
        } else {
            mouse_delta_x += mouse_data->lLastX;
            mouse_delta_y += mouse_data->lLastY;
        }
    } else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        RAWKEYBOARD *keyboard_data = &raw->data.keyboard;

        // Check the position of the key we're handling..
        // bool is_left  = (keyboard_data->Flags & RI_KEY_E0) != 0;
        // bool is_right = (keyboard_data->Flags & RI_KEY_E1) != 0;

        // The raw keyboard scan code.
        // u16 scan_code = keyboard_data->MakeCode;

        bool is_down  = (keyboard_data->Flags & RI_KEY_BREAK) == 0;
        u16 vk_code   = keyboard_data->VKey;

        if (vk_code == VK_SNAPSHOT) {
            bool repeat = is_down && ((b_input_state.button_states[KEY_PRINT_SCREEN] & B_KEY_STATE_DOWN) != 0);
            b_w32_send_keyboard_event(KEY_PRINT_SCREEN, is_down, repeat, 
                                    is_down ? (B_KEY_STATE_START|B_KEY_STATE_DOWN) : B_KEY_STATE_END);
        }
    }
#endif
}

NB_EXTERN char *
b_w32_wide_to_utf8(WCHAR *s, 
                   size_t src_length, 
                   NB_Allocator allocator) {
    if (!s) return null;

    if (!src_length) src_length = wcslen(s);
    int required_length = WideCharToMultiByte(CP_UTF8,
                                              0,
                                              s,
                                              (int)src_length,
                                              null,
                                              0,
                                              null, null);
    if (required_length) {
        char *result = nb_new_array(char, required_length+1, allocator);
        WideCharToMultiByte(CP_UTF8,
                            0,
                            s,
                            (int)src_length,
                            result,
                            required_length,
                            null, null);
        result[required_length] = 0;
        return result;
    }

    return null;
}


// https://learn.microsoft.com/en-us/windows/win32/tablet/system-events-and-mouse-messages?redirectedfrom=MSDN
#define B_MOUSEEVENTF_FROMTOUCH      0xFF515700
#define B_MOUSEEVENTF_FROMTOUCH_MASK 0xFFFFFF00
#define B_IsTouchEvent(dw) (((dw)&B_MOUSEEVENTF_FROMTOUCH_MASK) == B_MOUSEEVENTF_FROMTOUCH)

#include <shellapi.h>

static LRESULT CALLBACK 
b_w32_main_window_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        // WM_ACTIVATE is sent when the window is activated or deactivated.  
        // We pause the game when the window is deactivated and unpause it 
        // when it becomes active.
        case WM_ACTIVATE:
            b_input_state.is_app_paused = (LOWORD(wparam) == WA_INACTIVE);
            return 0;

        case WM_ACTIVATEAPP:
            if (wparam) {
                // Maybe we should keep track of held down keystrokes instead of doing this...
                for (s64 index = 0; index < B_KEY_CODE_COUNT; index++) {
                    u32 *it = &b_input_state.button_states[index];
                    if (!(*it & B_KEY_STATE_DOWN)) continue;

                    assert(index < nb_array_count(b_w32_vk_codes));
                    int vk_code = b_w32_vk_codes[index];
                    if (!vk_code) continue;

                    SHORT state = GetAsyncKeyState(vk_code);
                    if (!(state & 0x8000)) {
                        // Release key event.

                        BKey_Code key_code = (BKey_Code)index;

                        if (key_code == B_KEY_ALT)   b_input_state.alt_state   = false;
                        if (key_code == B_KEY_CMD)   b_input_state.cmd_state   = false;
                        if (key_code == B_KEY_CTRL)  b_input_state.ctrl_state  = false;
                        if (key_code == B_KEY_SHIFT) b_input_state.shift_state = false;

                        b_w32_send_keyboard_event(key_code, false, true, B_KEY_STATE_END);

/*
                        Event event;
                        event.type = EVENT_KEYBOARD;
                        event.key_code      = key_code;
                        event.key_pressed   = false;
                        event.repeat        = true;
                        event.alt_pressed   = b_input_state.alt_state;
                        event.cmd_pressed   = b_input_state.cmd_state;
                        event.ctrl_pressed  = b_input_state.ctrl_state;
                        event.shift_pressed = b_input_state.shift_state;
                        array_add(&events_this_frame, event);

                        b_input_state.button_states[key_code] |= B_KEY_STATE_END;
*/
                    }
                }
            }
            return DefWindowProcW(hwnd, msg, wparam, lparam);

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            u32 vk_code = (u32)wparam;
            bool was_down = (lparam & (1 << 30)) != 0; // Was key down last frame.
            bool extended = (HIWORD(lparam) & KF_EXTENDED) == KF_EXTENDED;

            BKey_Code key_code = B_KEY_UNKNOWN;
            if (vk_code < nb_array_count(b_w32_key_codes)) {
                key_code = b_w32_get_key_code(vk_code, extended);
            }

            // @Cleanup: Query modifiers state.
            if (key_code == B_KEY_ALT)   b_input_state.alt_state   = true;
            if (key_code == B_KEY_CMD)   b_input_state.cmd_state   = true;
            if (key_code == B_KEY_CTRL)  b_input_state.ctrl_state  = true;
            if (key_code == B_KEY_SHIFT) b_input_state.shift_state = true;

            b_w32_send_keyboard_event(key_code, true, was_down, B_KEY_STATE_START|B_KEY_STATE_DOWN);
            return 0;
        } break;

        case WM_SYSKEYUP:
        case WM_KEYUP:
        {
            u32 vk_code = (u32)wparam;
            bool was_down = true; // Always true for WM_KEYUP.
            bool extended = (HIWORD(lparam) & KF_EXTENDED) == KF_EXTENDED;

            BKey_Code key_code = B_KEY_UNKNOWN;
            if (vk_code < nb_array_count(b_w32_key_codes)) {
                key_code = b_w32_get_key_code(vk_code, extended);
            }

            // @Cleanup: Query modifiers state.
            if (key_code == B_KEY_ALT)   b_input_state.alt_state   = false;
            if (key_code == B_KEY_CMD)   b_input_state.cmd_state   = false;
            if (key_code == B_KEY_CTRL)  b_input_state.ctrl_state  = false;
            if (key_code == B_KEY_SHIFT) b_input_state.shift_state = false;

            b_w32_send_keyboard_event(key_code, false, was_down, B_KEY_STATE_END);
            return 0;
        } break;

        case WM_SYSCHAR:
            return 0;

        case WM_CHAR:
            if (IS_HIGH_SURROGATE(wparam)) {
                // Store the first part.
                b_w32_high_surrogate = (u32)wparam;
            } else {
                u32 codepoint = (u32)wparam;
                if (IS_LOW_SURROGATE(codepoint)) {
                    // We have a complete pair, let's combine them.
                    u32 low_surrogate = codepoint;
                    codepoint = (b_w32_high_surrogate - HIGH_SURROGATE_START) << 10;
                    codepoint += (low_surrogate - LOW_SURROGATE_START);
                    codepoint += 0x10000;

                    // print("HS: %u, LS: %u\n", b_w32_high_surrogate, low_surrogate);

                    b_w32_high_surrogate = 0;
                }

                // print("utf32 = %u\n", codepoint);

                // Generate events for printable codepoints.
                if (codepoint > 31 && codepoint != 127) {
                    BEvent event;
                    event.type  = B_EVENT_TEXT_INPUT;
                    event.utf32 = codepoint;

                    b_push_event(event);
                }
            }
            return 0;

        case WM_LBUTTONDOWN:
            // Ignore synthetic mouse events generated from touch screen.
            if (B_IsTouchEvent(GetMessageExtraInfo())) return 0;

            b_w32_send_keyboard_event(B_MOUSE_BUTTON_LEFT, true, false, B_KEY_STATE_DOWN|B_KEY_STATE_START);
            SetCapture(hwnd);
            return 0;

        case WM_LBUTTONUP:
            // Ignore synthetic mouse events generated from touch screen.
            if (B_IsTouchEvent(GetMessageExtraInfo())) return 0;

            b_w32_send_keyboard_event(B_MOUSE_BUTTON_LEFT, false, true, B_KEY_STATE_END);
            ReleaseCapture();
            return 0;

        case WM_RBUTTONDOWN:
            // Ignore synthetic mouse events generated from touch screen.
            if (B_IsTouchEvent(GetMessageExtraInfo())) return 0;

            b_w32_send_keyboard_event(B_MOUSE_BUTTON_RIGHT, true, false, B_KEY_STATE_DOWN|B_KEY_STATE_START);
            SetCapture(hwnd);
            return 0;

        case WM_RBUTTONUP:
            // Ignore synthetic mouse events generated from touch screen.
            if (B_IsTouchEvent(GetMessageExtraInfo())) return 0;

            b_w32_send_keyboard_event(B_MOUSE_BUTTON_RIGHT, false, true, B_KEY_STATE_END);
            ReleaseCapture();
            return 0;

        case WM_MBUTTONDOWN:
            b_w32_send_keyboard_event(B_MOUSE_BUTTON_MIDDLE, true, false, B_KEY_STATE_DOWN|B_KEY_STATE_START);
            SetCapture(hwnd);
            return 0;

        case WM_MBUTTONUP:
            b_w32_send_keyboard_event(B_MOUSE_BUTTON_MIDDLE, false, true, B_KEY_STATE_END);
            ReleaseCapture();
            return 0;

        case WM_XBUTTONDOWN:
            b_w32_send_keyboard_event((wparam < (1 << 17)) ? B_MOUSE_BUTTON_X1 : B_MOUSE_BUTTON_X2, 
                true, false, B_KEY_STATE_DOWN|B_KEY_STATE_START);
            SetCapture(hwnd);
            return TRUE;

        case WM_XBUTTONUP:
            b_w32_send_keyboard_event((wparam < (1 << 17)) ? B_MOUSE_BUTTON_X1 : B_MOUSE_BUTTON_X2, 
                false, true, B_KEY_STATE_END);
            ReleaseCapture();
            return TRUE;

        case WM_MOUSEWHEEL:
        {
            BEvent event;
            event.type = B_EVENT_MOUSE_V_WHEEL;
            b_input_state.typical_wheel_delta = WHEEL_DELTA;
            event.wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
            b_push_event(event);

            b_input_state.mouse_wheel_delta.vertical += event.wheel_delta;
            return 0;
        } break;

        case WM_MOUSEHWHEEL:
        {
            BEvent event;
            event.type = B_EVENT_MOUSE_H_WHEEL;
            b_input_state.typical_wheel_delta = WHEEL_DELTA;
            event.wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
            b_push_event(event);

            b_input_state.mouse_wheel_delta.horizontal += event.wheel_delta;
            return 0;
        } break;

        case WM_MOUSEMOVE:
        {
/*
            // Do we need a mouse move event in practice?

            // Now we use raw input for mouse delta.
            static int w32_last_mouse_x = 0;
            static int w32_last_mouse_y = 0;

            int x = (lparam & 0xFFFF);
            int y = (lparam >> 16) & 0xFFFF;

            b_input_state.mouse_delta_x += x - w32_last_mouse_x;
            b_input_state.mouse_delta_y += y - w32_last_mouse_y;

            w32_last_mouse_x = x;
            w32_last_mouse_y = y;
*/
        } break;

        case WM_TOUCH:
        {
            u32 touch_points = LOWORD(wparam);
            HTOUCHINPUT touch_handle = (HTOUCHINPUT)lparam;

            assert(touch_points <= B_MAX_TOUCH_POINT_INPUT_COUNT);

            bool handled = false;

            if (GetTouchInputInfo(touch_handle, touch_points, b_w32_touch_inputs, size_of(TOUCHINPUT))) {
                // Process touch input.
                u32 count = Min(touch_points, nb_array_count(b_input_state.touch_pointers));
                for (u8 i = 0; i < count; i++) {
                    TOUCHINPUT ti = b_w32_touch_inputs[i];

                    BEvent event = {};
                    event.type = B_EVENT_TOUCH;
                    event.touch_index = i;

                    if (ti.dwFlags & TOUCHEVENTF_MOVE) {
                        event.touch_type = B_TOUCH_MOVED;
                    } else if (ti.dwFlags & TOUCHEVENTF_DOWN) {
                        event.touch_type = B_TOUCH_PRESSED;
                    } else if (ti.dwFlags & TOUCHEVENTF_UP) {
                        event.touch_type = B_TOUCH_RELEASED;
                    }

                    POINT pt;
                    pt.x = TOUCH_COORD_TO_PIXEL(ti.x);
                    pt.y = TOUCH_COORD_TO_PIXEL(ti.y);
                    ScreenToClient(hwnd, &pt);

                    event.x = pt.x;
                    event.y = pt.y;

                    b_push_event(event);

                    b_input_state.touch_pointers[i].type = event.touch_type;
                    b_input_state.touch_pointers[i].x = event.x;
                    b_input_state.touch_pointers[i].y = event.y;
                }

                b_input_state.touch_pointer_count = count;

                handled = true;
            }

            if (handled) {
                // We handled the touch input, return 0.
                CloseTouchInputHandle(touch_handle);
                return 0;
            }

            // We didn't handle the touch input, call DefWindowProc.
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;

/*
        case WM_ERASEBKGND:
            // notify the OS that erasing will be handled by the application to prevent flicker
            return 1;
*/

        case WM_CLOSE:
        case WM_QUIT:
        {
            BEvent event = {0};
            event.type = B_EVENT_QUIT;
            b_push_event(event);
        } break;

        case WM_SETFOCUS:
            b_input_state.application_has_focus = true;
            return 0;

        case WM_KILLFOCUS:
            b_input_state.application_has_focus = false;
            return 0;

        // WM_ENTERSIZEMOVE is sent when the user grabs the resize bars.
        case WM_ENTERSIZEMOVE:
            // Halt frame movement while the app is sizing or moving
            b_input_state.is_app_paused    = true;
            b_input_state.is_user_resizing = true;
            return 0;

        case WM_SIZE: {
            UINT width  = LOWORD(lparam);
            UINT height = HIWORD(lparam);

            BEvent event = {0};
            event.type = B_EVENT_WINDOW_RESIZE;
            event.x = (s32)width;
            event.y = (s32)height;

            if (wparam == SIZE_MINIMIZED) {
                b_input_state.is_app_paused    = true;
                b_input_state.is_window_minimized = true;
                b_input_state.is_window_maximized = false;
            } else if (wparam == SIZE_MAXIMIZED) {
                b_input_state.is_app_paused    = false;
                b_input_state.is_window_minimized = false;
                b_input_state.is_window_maximized = true;

                // Resize.
                b_push_event(event);
            } else if (wparam == SIZE_RESTORED) {
                if (b_input_state.is_window_minimized) {  // Restoring from minimized state?
                    b_input_state.is_app_paused    = false;
                    b_input_state.is_window_minimized = false;

                    // Resize.
                    b_push_event(event);
                } else if (b_input_state.is_window_maximized) {  // Restoring from maximized state?
                    b_input_state.is_app_paused    = false;
                    b_input_state.is_window_maximized = false;

                    // Resize.
                    b_push_event(event);
                } else if (b_input_state.is_user_resizing) {
                    // If user is dragging the resize bars, we do not resize 
                    // the buffers here because as the user continuously 
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is 
                    // done resizing the window and releases the resize bars, which 
                    // sends a WM_EXITSIZEMOVE message.
                } else {  // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
                    // Resize.
                    b_push_event(event);
                }
            }

            return 0;
        } break;

        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
        case WM_EXITSIZEMOVE: {
            b_input_state.is_app_paused    = false;
            b_input_state.is_user_resizing = false;

            // wparam and lparam are not used in WM_EXITSIZEMOVE.
            RECT rect;
            GetClientRect(hwnd, &rect);
            BEvent event = {0};
            event.type = B_EVENT_WINDOW_RESIZE;
            event.x = rect.right  - rect.left;
            event.y = rect.bottom - rect.top;

            // Resize.
            b_push_event(event);
            return 0;
        } break;

        case WM_INPUT:
        {
            LPARAM extra = GetMessageExtraInfo();

            if ((extra & 0x80) == 0x80) // Check the 7th bit (Only works for Vista and up).
            {
                // Filter out touch event.
            } else {
                b_w32_process_raw_input((HRAWINPUT)lparam);
            }

            // We're supposed to always call DefWindowProc:
            // http://the-witness.net/news/2012/10/wm_touch-is-totally-bananas/
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;

        // The WM_MENUCHAR message is sent when a menu is active and the user presses 
        // a key that does not correspond to any mnemonic or accelerator key. 
        case WM_MENUCHAR:
            // Don't beep when we alt-enter.
            return MAKELRESULT(0, MNC_CLOSE);

#if 0
        case WM_DROPFILES: {
            HDROP handle = (HDROP)wparam;

            // If the value of this parameter is 0xFFFFFFFF, DragQueryFile returns a count of the files dropped.
            UINT dropped_files_count = DragQueryFileW(handle, 0xFFFFFFFF, null, 0);
            assert(dropped_files_count > 0);

            if (dropped_files_count) {
                Array<char *> files;
                array_reserve(&files, dropped_files_count);

                for (UINT index = 0; index < dropped_files_count; index++) {
                    // If the third parameter is NULL, DragQueryFile returns the required size, in characters.
                    UINT required_size_wide = DragQueryFileW(handle, index, null, 0) + 2;
                    assert(required_size_wide > 0);

                    WCHAR *file_name_wide = nb_new_array(WCHAR, required_size_wide, nb_temporary_allocator);
                    UINT success = DragQueryFileW(handle, index, file_name_wide, required_size_wide);
                    if (success > 0) {
                        char *file_path = w32_wide_to_utf8(file_name_wide, required_size_wide, {heap_allocator, null});
                        //array_add(&files, file_path);
                    }
                }

                Event event;
                event.type  = EVENT_DRAG_AND_DROP_FILES;
                event.files = files;
                //array_add(&events_this_frame, event);
            }

            DragFinish(handle);

            return 0;
        } break;
#endif

        default: 
            return DefWindowProcW(hwnd, msg, wparam, lparam);
            break;
    }

    return 0;
}

#if 0
static STICKYKEYS b_w32_startup_sticky_keys = {size_of(STICKYKEYS), 0};
static TOGGLEKEYS b_w32_startup_toggle_keys = {size_of(TOGGLEKEYS), 0};
static FILTERKEYS b_w32_startup_filter_keys = {size_of(FILTERKEYS), 0};

static void 
b_w32_allow_accessibility_shortcut_keys(bool allow_keys) {
    if (allow_keys) {
        // Restore StickyKeys/etc to original state and enable Windows key
        STICKYKEYS sk = b_w32_startup_sticky_keys;
        TOGGLEKEYS tk = b_w32_startup_toggle_keys;
        FILTERKEYS fk = b_w32_startup_filter_keys;

        UNUSED(sk);
        UNUSED(tk);
        UNUSED(fk);

        SystemParametersInfo(SPI_SETSTICKYKEYS, size_of(STICKYKEYS), &b_w32_startup_sticky_keys, 0);
        SystemParametersInfo(SPI_SETTOGGLEKEYS, size_of(TOGGLEKEYS), &b_w32_startup_toggle_keys, 0);
        SystemParametersInfo(SPI_SETFILTERKEYS, size_of(FILTERKEYS), &b_w32_startup_filter_keys, 0);
    } else {
        // Disable StickyKeys/etc shortcuts but if the accessibility feature is on, 
        // then leave the settings alone as its probably being usefully used

        STICKYKEYS sk_off = b_w32_startup_sticky_keys;
        if ((sk_off.dwFlags & SKF_STICKYKEYSON) == 0) {
            // Disable the hotkey, confirmation and the sound.
            sk_off.dwFlags &= ~SKF_HOTKEYACTIVE;
            sk_off.dwFlags &= ~SKF_CONFIRMHOTKEY;
            sk_off.dwFlags &= ~SKF_HOTKEYSOUND;

            SystemParametersInfo(SPI_SETSTICKYKEYS, size_of(STICKYKEYS), &sk_off, 0);
        }

        TOGGLEKEYS tk_off = b_w32_startup_toggle_keys;
        if ((tk_off.dwFlags & TKF_TOGGLEKEYSON) == 0) {
            // Disable the hotkey, confirmation and the sound.
            tk_off.dwFlags &= ~TKF_HOTKEYACTIVE;
            tk_off.dwFlags &= ~TKF_CONFIRMHOTKEY;
            tk_off.dwFlags &= ~TKF_HOTKEYSOUND;

            SystemParametersInfo(SPI_SETTOGGLEKEYS, size_of(TOGGLEKEYS), &tk_off, 0);
        }

        FILTERKEYS fk_off = b_w32_startup_filter_keys;
        if ((fk_off.dwFlags & FKF_FILTERKEYSON) == 0) {
            // Disable the hotkey, confirmation and the sound.
            fk_off.dwFlags &= ~FKF_HOTKEYACTIVE;
            fk_off.dwFlags &= ~FKF_CONFIRMHOTKEY;
            fk_off.dwFlags &= ~FKF_HOTKEYSOUND;

            SystemParametersInfo(SPI_SETFILTERKEYS, size_of(FILTERKEYS), &fk_off, 0);
        }
    }
}

static void b_w32_init_input_system(void) {
    if (w32_input_initted) return;

    RAWINPUTDEVICE rid[2];

    rid[0].usUsagePage = 0x01;
    rid[0].usUsage     = 0x02;  // HID mouse usage.
    rid[0].dwFlags     = 0;
    rid[0].hwndTarget  = null;

    // We initialize the keyboard because we need to query the state of
    // the keystrokes that are not handled by the windows proc, 
    // like the WM_KEYDOWN for VK_SNAPSHOT...
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage     = 0x06;  // HID keyboard usage.
    rid[1].dwFlags     = 0;
    rid[1].hwndTarget  = null;

    if (RegisterRawInputDevices(rid, nb_array_count(rid), size_of(RAWINPUTDEVICE)) == FALSE) {
        nb_write_string("Failed to init raw input.\n", true);
    }

    if (!raw_input_buffer.allocated) {
        array_resize(&raw_input_buffer, 8000);
    }

    // Disable stickykeys/togglekeys/filterkeys unless
    // the accessibility feature is on.
    // https://learn.microsoft.com/en-us/windows/win32/dxtecharts/disabling-shortcut-keys-in-games?redirectedfrom=MSDN

    // @Note: We can't pass RIDEV_NOHOTKEYS when initting rawinput since it removes
    // ALT+TAB on Windows 8.


    // Store shortcut keys at startup so we can set them back later..
    SystemParametersInfo(SPI_GETSTICKYKEYS, size_of(STICKYKEYS), &w32_startup_sticky_keys, 0);
    SystemParametersInfo(SPI_GETTOGGLEKEYS, size_of(TOGGLEKEYS), &w32_startup_toggle_keys, 0);
    SystemParametersInfo(SPI_GETFILTERKEYS, size_of(FILTERKEYS), &w32_startup_filter_keys, 0);

    w32_allow_accessibility_shortcut_keys(false);
}
#endif

NB_EXTERN bool bender_init(void) {
    if (b_initted) return true;

    b_w32_instance = GetModuleHandleW(null);

    b_initted = true;
    return b_initted;
}

NB_EXTERN u32 
bender_create_window(const char *title, 
                     s32 width, 
                     s32 height, 
                     s32 window_x, 
                     s32 window_y, 
                     u32 window_parent_index, 
                     u32 window_creation_flags, 
                     const float background_color[3]) {
    u32 result = 0;  // In our API 0 is not valid id.

    if (!b_initted) {
        // If we didn't call b_init at startup, now is the time.
        bender_init();

        // Load the first icon resource.
        HICON icon = LoadIconW(b_w32_instance, MAKEINTRESOURCEW(2));

        // If the icon is null, then use the first one found in the exe.
        if (icon == null) {
            WCHAR exe_path[MAX_PATH];
            GetModuleFileNameW(null, exe_path, MAX_PATH);

            icon = ExtractIconW(b_w32_instance, exe_path, 0); // 0 means first icon.
        }

        // CreateSolidBrush takes a BGR color.
        u32 r = b_float_to_u32_color_channel(background_color[0]);
        u32 g = b_float_to_u32_color_channel(background_color[1]);
        u32 b = b_float_to_u32_color_channel(background_color[2]);

        HBRUSH brush = CreateSolidBrush((b << 16) | (g << 8) | r);

        WNDCLASSEXW wc = {};
        wc.cbSize        = size_of(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.hInstance     = b_w32_instance;
        wc.hIcon         = icon;
        wc.hCursor       = LoadCursorW(null, (PWSTR)IDC_ARROW);
        wc.hbrBackground = brush;
        wc.lpszClassName = BENDER_DEFAULT_WINDOW_CLASS_NAME;

        wc.lpfnWndProc   = b_w32_main_window_callback;

        // Register the window class.
        if (RegisterClassExW(&wc) == 0) {
            nb_write_string("Failed to RegisterClassExW.\n", true);
            return result;
        }
    }

    // If the window size is too big for the user's monitor,
    // we compute the ratio and setup the new window dimensions.
    {
        if (height < 1) height = 1;  // Safe ratio.

        s32 limit = 0;
        HWND desktop = GetDesktopWindow();
        RECT desktop_rect;
        GetWindowRect(desktop, &desktop_rect);

        s32 desktop_height = desktop_rect.bottom - desktop_rect.top;
        limit = (s32)(desktop_height * 0.9f);

        if (height > limit) {
            float ratio = limit / (float)height;
            height = (s32)(height * ratio);
            width  = (s32)(width  * ratio);
        }
    }

    if (window_x == -1) {
        RECT work_area;
        BOOL work_area_success = SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
        if (work_area_success) {
            window_x = work_area.left;
            window_y = work_area.top;
        }
    }


    HWND parent_hwnd = null;
    u32 style = WS_OVERLAPPEDWINDOW;
    u32 ex_style = 0;
    if (window_creation_flags & B_WINDOW_CREATE_DRAG_AND_DROP) {
        ex_style |= WS_EX_ACCEPTFILES;
    }

    if (window_parent_index) {
        Bender_Window_Record *parent_record = b_get_window_record(window_parent_index);
        parent_hwnd = parent_record->handle;
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    }

    if (window_creation_flags & B_WINDOW_CREATE_BORDERLESS) {
        style = WS_POPUP;
    }

    RECT rect = {};
    rect.right  = width;
    rect.bottom = height;

    AdjustWindowRect(&rect, style, 0);

    s32 client_width  = rect.right  - rect.left;
    s32 client_height = rect.bottom - rect.top;

    WCHAR *w32_utf8_to_wide(const char *s, NB_Allocator allocator);

    HWND hwnd = CreateWindowExW(ex_style,
                                BENDER_DEFAULT_WINDOW_CLASS_NAME,
                                w32_utf8_to_wide(title, nb_temporary_allocator),
                                style,
                                window_x, window_y,
                                client_width, client_height,
                                parent_hwnd,
                                null,
                                b_w32_instance,
                                null);
    if (hwnd == null) {
        nb_write_string("Failed to CreateWindowExW.\n", true);
        return result;
    }

    u32 show_command = SW_SHOW;
    if (window_creation_flags & B_WINDOW_CREATE_MAXIMIZED) {
        show_command = SW_MAXIMIZE;
    }

    // Display the window.
    UpdateWindow(hwnd);
    ShowWindow(hwnd, show_command);

#if 0
    if (!w32_input_initted) {
        w32_init_input_system();
        w32_input_initted = true;
    }
#endif

    if (!b_window_record_storage) {
        b_window_record_allocated = 4;
        b_window_record_storage = nb_new_array(Bender_Window_Record, 
                                               b_window_record_allocated, 
                                               NB_GET_ALLOCATOR());
        if (!b_window_record_storage) return result;
    }

    if (b_window_record_count == b_window_record_allocated) {
        u32 old_count = b_window_record_allocated;
        b_window_record_allocated += 4;
        b_window_record_storage = (Bender_Window_Record *)nb_realloc(b_window_record_storage, 
            b_window_record_allocated*size_of(Bender_Window_Record),
            old_count*size_of(Bender_Window_Record),
            NB_GET_ALLOCATOR());
    }

    result = b_window_record_count;
    b_window_record_count += 1; // 0 is an invalid ID in our API.

    Bender_Window_Record *record = b_window_record_storage + result;
    record->handle   = hwnd;
    record->style    = style;
    record->ex_style = ex_style;

    // Setting up touch input for the current window.
    {
        u32 old_mode = nb_logger_push_mode(NB_LOG_NONE);
        const char *old_ident = nb_logger_push_ident("Input");

        int input_caps = GetSystemMetrics(SM_DIGITIZER);
        bool init_touch_input_for_hwnd = false;

        if (input_caps & NID_INTEGRATED_TOUCH) {
            Log("Found an integrated touch digitizer for window: '%s'.", title);
        }
        if (input_caps & NID_EXTERNAL_TOUCH) {
            Log("Found an external touch digitizer for window: '%s'.", title);
        }
        if (input_caps & NID_INTEGRATED_PEN) {
            Log("Found an integrated pen digitizer for window '%s'.", title);
        }
        if (input_caps & NID_EXTERNAL_PEN) {
            Log("Found an external pen digitizer for window: '%s'.", title);
        }
        if (input_caps & NID_MULTI_INPUT) {
            Log("Found an input digitizer with support for multiple inputs for window '%s'.", title);
        }

        if (input_caps & NID_READY) {
            Log("The input digitizer is ready for input.");
            init_touch_input_for_hwnd = true;
        }

        if (init_touch_input_for_hwnd) {
            BOOL success = RegisterTouchWindow(hwnd, TWF_FINETOUCH);//0); //TWF_WANTPALM);
            if (success != 0) {
                Log("Registered '%s' for touch input.", title);
            } else {
                Log("Failed to RegisterTouchWindow '%s' for touch input.", title);
            }
        }

        nb_logger_push_mode(old_mode);
        nb_logger_push_ident(old_ident);
    }

    return result;
}

NB_EXTERN void bender_update_window_events(void) {
#if 0
    for (s64 index = 0; index < events_this_frame.count; index++) {
        Event *it = &events_this_frame[index];
        array_free(&it->files);
    }
    array_reset(&events_this_frame);
#endif

    b_input_state.event_count = 0;

    for (s64 index = 0; index < nb_array_count(b_input_state.button_states); index++) {
        u32 *state = &b_input_state.button_states[index];

        if ((*state & B_KEY_STATE_END) || !b_input_state.application_has_focus) {
            *state &= ~(B_KEY_STATE_DOWN|B_KEY_STATE_START|B_KEY_STATE_END);
        } else {
            *state &= ~B_KEY_STATE_START;
        }
    }

    b_input_state.mouse_wheel_delta.vertical   = 0;
    b_input_state.mouse_wheel_delta.horizontal = 0;
    b_input_state.mouse_delta_x = 0;
    b_input_state.mouse_delta_y = 0;

    b_input_state.touch_pointer_count = 0;

    // Should we do this for all the keystrokes?
    if (b_input_state.alt_state || (b_input_state.button_states[B_KEY_ALT] & B_KEY_STATE_DOWN)) {
        SHORT state = GetAsyncKeyState(VK_MENU);
        if (!(state & 0x8000)) {
            b_input_state.alt_state = false;
            b_input_state.button_states[B_KEY_ALT] |= B_KEY_STATE_END;
        }
    }

    if (b_input_state.cmd_state || (b_input_state.button_states[B_KEY_CMD] & B_KEY_STATE_DOWN)) {
        SHORT state = GetAsyncKeyState(VK_APPS);
        if (!(state & 0x8000)) {
            b_input_state.cmd_state = false;
            b_input_state.button_states[B_KEY_CMD] |= B_KEY_STATE_END;
        }
    }

    if (b_input_state.ctrl_state || (b_input_state.button_states[B_KEY_CTRL] & B_KEY_STATE_DOWN)) {
        SHORT state = GetAsyncKeyState(VK_CONTROL);
        if (!(state & 0x8000)) {
            b_input_state.ctrl_state = false;
            b_input_state.button_states[B_KEY_CTRL] |= B_KEY_STATE_END;
        }
    }

    if (b_input_state.shift_state || (b_input_state.button_states[B_KEY_SHIFT] & B_KEY_STATE_DOWN)) {
        SHORT state = GetAsyncKeyState(VK_SHIFT);
        if (!(state & 0x8000)) {
            b_input_state.shift_state = false;
            b_input_state.button_states[B_KEY_SHIFT] |= B_KEY_STATE_END;
        }
    }

    while (1) {
        MSG msg;

        BOOL result = PeekMessageW(&msg, null, 0, 0, PM_REMOVE);
        if (!result) break;

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

NB_EXTERN bool 
bender_get_next_event(BEvent *event) {
    if (b_input_state.event_count) {
        *event = b_input_state.events_this_frame[b_input_state.event_count-1];
        b_input_state.event_count -= 1;
        return true;
    }

    return false;
}

NB_EXTERN void 
bender_get_window_size(u32 window_id, 
                       s32 *width_return, 
                       s32 *height_return) {
    Bender_Window_Record *record = b_get_window_record(window_id);
    HWND hwnd = record->handle;

    RECT rect;
    GetClientRect(hwnd, &rect);
    *width_return  = rect.right  - rect.left;
    *height_return = rect.bottom - rect.top;
}

NB_EXTERN void 
bender_get_mouse_pointer_position(u32 window_id, 
                                  s32 *x_return, 
                                  s32 *y_return) {
    Bender_Window_Record *record = b_get_window_record(window_id);
    HWND hwnd = record->handle;

    POINT p;
    BOOL success = GetCursorPos(&p);
    if (!success) {
        *x_return = 0;
        *y_return = 0;
    }

    ScreenToClient(hwnd, &p);

    *x_return = p.x;
    *y_return = p.y;
}

#if 0
// @Cutnpaste from get_mouse_pointer_position
NB_EXTERN void 
bender_get_mouse_pointer_position(s32 *x_return, s32 *y_return) {
    UNUSED(x_return);
    UNUSED(y_return);
    POINT p;
    BOOL success = GetCursorPos(&p);
    if (!success) {
        *x_return = 0;
        *y_return = 0;
    }

    HWND hwnd = GetActiveWindow();
    ScreenToClient(hwnd, &p);

    *x_return = p.x;
    *y_return = p.y;
}
#endif

// @Cutnpaste from get_mouse_pointer_position
NB_EXTERN void 
bender_get_mouse_pointer_position_right_handed(u32 window_id, 
                                               s32 *x_return, 
                                               s32 *y_return) {
    Bender_Window_Record *record = b_get_window_record(window_id);
    HWND hwnd = record->handle;

    POINT p;
    BOOL success = GetCursorPos(&p);
    if (!success) {
        *x_return = 0;
        *y_return = 0;
    }

    ScreenToClient(hwnd, &p);

    *x_return = p.x;
    *y_return = p.y;

    RECT screen_rect;
    if (GetClientRect(hwnd, &screen_rect)) {
        s32 height = screen_rect.bottom - screen_rect.top;
        *y_return = height - *y_return;
    }
}

#if 0
// @Cutnpaste from get_mouse_pointer_position_right_handed
NB_EXTERN void bender_get_mouse_pointer_position_right_handed(s32 *x_return, s32 *y_return) {
    POINT p;
    BOOL success = GetCursorPos(&p);
    if (!success) {
        *x_return = 0;
        *y_return = 0;
    }

    HWND hwnd = GetActiveWindow();
    ScreenToClient(hwnd, &p);

    *x_return = p.x;
    *y_return = p.y;

    RECT screen_rect;
    if (GetClientRect(hwnd, &screen_rect)) {
        s32 height = screen_rect.bottom - screen_rect.top;
        *y_return = height - *y_return;
    }
}
#endif

NB_EXTERN void bender_sleep_ms(u32 ms) {
    Sleep((DWORD)ms);
}

#if 0
static void b_w32_set_screen_mode(s32 w, s32 h, bool reset) {
    DEVMODEW screen_settings = {0};
    screen_settings.dmSize       = size_of(screen_settings);
    screen_settings.dmPelsWidth  = w;
    screen_settings.dmPelsHeight = h;
    screen_settings.dmBitsPerPel = 32;
    screen_settings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

    if (reset) {
        ChangeDisplaySettings(null, 0);
    } else {
        ChangeDisplaySettings(&screen_settings, CDS_FULLSCREEN);
    }
}
#endif

NB_EXTERN void 
bender_toggle_fullscreen(u32 window_id, bool want_fullscreen) {
    Bender_Window_Record *record = b_get_window_record(window_id);
    HWND hwnd = record->handle;

    if (want_fullscreen) {
        u32 old_style    = GetWindowLongW(hwnd, GWL_STYLE);
        u32 old_ex_style = GetWindowLongW(hwnd, GWL_EXSTYLE);

        SetWindowLongW(hwnd, GWL_STYLE, (old_style & ~(WS_CAPTION|WS_THICKFRAME)));
        SetWindowLongW(hwnd, GWL_EXSTYLE, 
            (old_ex_style & ~(WS_EX_DLGMODALFRAME|WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE|WS_EX_STATICEDGE)));

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info;
        info.cbSize = size_of(MONITORINFO);
        BOOL success = GetMonitorInfoW(monitor, &info);
        if (success != 0) {
            int x = info.rcMonitor.left;
            int y = info.rcMonitor.top;
            int width  = info.rcMonitor.right  - x;
            int height = info.rcMonitor.bottom - y;

            record->style    = old_style;
            record->ex_style = old_ex_style;
            GetWindowRect(hwnd, &record->rect);

#if 1
            SetWindowPos(hwnd, HWND_TOPMOST, x, y, width, height, 
                         SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
#else
            SetWindowPos(hwnd, HWND_TOPMOST, x, y, width, height, 
                         SWP_NOACTIVATE|SWP_FRAMECHANGED|SWP_NOCOPYBITS|SWP_SHOWWINDOW); 
                         //|SWP_NOREDRAW|SWP_NOOWNERZORDER);
#endif
        }
    } else {
        int x = record->rect.left;
        int y = record->rect.top;
        int width  = record->rect.right  - x;
        int height = record->rect.bottom - y;

        SetWindowLongW(hwnd, GWL_STYLE, record->style);
        SetWindowLongW(hwnd, GWL_EXSTYLE, record->ex_style);

        // @Cleanup: Check if we need to call AdjustWindowRect.

        // HWND_TOP doesn't necessarily remove the effect of HWND_TOPMOST.
#if 1
        SetWindowPos(hwnd, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED);
#else
        SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, width, height, 
                     SWP_FRAMECHANGED|SWP_SHOWWINDOW);
#endif
    }
}

#if 0
static bool 
b_w32_find_minimum_display_mode(DEVMODEW *mode, 
                                s32 min_width, s32 min_height, 
                                u32 min_freq, u32 max_freq) {
    mode->dmSize = size_of(DEVMODEW);

    DWORD current_mode_index = 0;
    while (EnumDisplaySettingsW(w32_target_device_name, current_mode_index, mode) != 0) {
        if (mode->dmPelsWidth  >= (u32)min_width &&
            mode->dmPelsHeight >= (u32)min_height &&
            mode->dmDisplayFrequency >= min_freq &&
            mode->dmDisplayFrequency <= max_freq) {
            return true;
        }

        current_mode_index += 1;
    }

    return false;
}

NB_EXTERN void 
bender_init_display_modes(u32 target_adapter_index) {
    bool target_mode_found = false;

    DISPLAY_DEVICEW adapter = { size_of(DISPLAY_DEVICEW) };
    DWORD current_adapter_index = 0;
    while (EnumDisplayDevicesW(null, current_adapter_index, &adapter, 0) != 0) {
        print("Adapter %u Name:   %s\n", 
              current_adapter_index, 
              w32_wide_to_utf8(adapter.DeviceName, 0, nb_temporary_allocator));
        print("Adapter %u String: %s\n", 
              current_adapter_index, 
              w32_wide_to_utf8(adapter.DeviceString, 0, nb_temporary_allocator));

        if (adapter.StateFlags) {
            print("Adapter %u State: ", current_adapter_index);
            if (adapter.StateFlags & DISPLAY_DEVICE_ACTIVE)
                nb_write_string(" ACTIVE");
            if (adapter.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)
                nb_write_string(" MIRRORING_DRIVER");
            if (adapter.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)
                nb_write_string(" MODESPRUNED");
            if (adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
                nb_write_string(" PRIMARY_DEVICE");
            if (adapter.StateFlags & DISPLAY_DEVICE_REMOVABLE)
                nb_write_string(" REMOVABLE");
            if (adapter.StateFlags & DISPLAY_DEVICE_VGA_COMPATIBLE)
                nb_write_string(" VGA_COMPATIBLE");
            nb_write_string("\n");
        }


        DISPLAY_DEVICEW display_device = { size_of(DISPLAY_DEVICEW) };
        DWORD current_device_index = 0;
        while (EnumDisplayDevicesW(adapter.DeviceName, current_device_index, &display_device, 0) != 0) {
            print("\tDisplay Device %u:\n", current_device_index);
            print("\t\tDeviceName:   %s\n", 
                  w32_wide_to_utf8(display_device.DeviceName, 0, nb_temporary_allocator));
            print("\t\tDeviceString: %s\n", 
                  w32_wide_to_utf8(display_device.DeviceString, 0, nb_temporary_allocator));

            if (display_device.StateFlags) {
                nb_write_string("\t\tStateFlags:  ");
                if (display_device.StateFlags & DISPLAY_DEVICE_ACTIVE)
                    nb_write_string(" ACTIVE");
                if (display_device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)
                    nb_write_string(" MIRRORING_DRIVER");
                if (display_device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)
                    nb_write_string(" MODESPRUNED");
                if (display_device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
                    nb_write_string(" PRIMARY_DEVICE");
                if (display_device.StateFlags & DISPLAY_DEVICE_REMOVABLE)
                    nb_write_string(" REMOVABLE");
                if (display_device.StateFlags & DISPLAY_DEVICE_VGA_COMPATIBLE)
                    nb_write_string(" VGA_COMPATIBLE");
                nb_write_string("\n");
            }

            current_device_index += 1;
        }


        DEVMODEW mode = {};
        mode.dmSize = size_of(DEVMODEW);
        if (EnumDisplaySettingsW(adapter.DeviceName, ENUM_CURRENT_SETTINGS, &mode) != 0) {
            nb_write_string("\tDisplay Mode Current:  ");

            print("%u x %u, %u%s Hz, %u bpp",
                  mode.dmPelsWidth,
                  mode.dmPelsHeight,
                  mode.dmDisplayFrequency,
                  (mode.dmDisplayFrequency <= 1 ? " (Default)" : ""),
                  mode.dmBitsPerPel);

            if (mode.dmDisplayFlags & DM_INTERLACED) {
                nb_write_string(" Interlaced");
            }

            nb_write_string("\n");
        }

        if (EnumDisplaySettingsW(adapter.DeviceName, ENUM_REGISTRY_SETTINGS, &mode) != 0) {
            nb_write_string("\tDisplay Mode Registry: ");

            print("%u x %u, %u%s Hz, %u bpp",
                  mode.dmPelsWidth,
                  mode.dmPelsHeight,
                  mode.dmDisplayFrequency,
                  (mode.dmDisplayFrequency <= 1 ? " (Default)" : ""),
                  mode.dmBitsPerPel);

            if (mode.dmDisplayFlags & DM_INTERLACED) {
                nb_write_string(" Interlaced");
            }

            nb_write_string("\n");
        }


        DWORD current_mode_index = 0;
        while (EnumDisplaySettingsW(adapter.DeviceName, current_mode_index, &mode) != 0) {
            print("\tDisplay Mode %u: ", current_mode_index);

            print("%u x %u, %u%s Hz, %u bpp",
                  mode.dmPelsWidth,
                  mode.dmPelsHeight,
                  mode.dmDisplayFrequency,
                  (mode.dmDisplayFrequency <= 1 ? " (Default)" : ""),
                  mode.dmBitsPerPel);

            if (mode.dmDisplayFlags & DM_INTERLACED) {
                nb_write_string(" Interlaced");
            }

            nb_write_string("\n");

            if (!target_mode_found && (current_adapter_index == target_adapter_index)) {
                wcscpy_s(w32_target_device_name, nb_array_count(w32_target_device_name), adapter.DeviceName);
                w32_target_device_mode = mode;
                target_mode_found = true;
            }

            current_mode_index += 1;
        }


        current_adapter_index += 1;
    }
}
#endif

NB_EXTERN void 
bender_messagebox_info(const char *title, const char *message) {
    WCHAR *wide_title   = w32_utf8_to_wide(title, nb_temporary_allocator);
    WCHAR *wide_message = w32_utf8_to_wide(message, nb_temporary_allocator);

    MessageBoxW(null, wide_message, wide_title, MB_OK|MB_ICONINFORMATION);
}

NB_EXTERN bool 
bender_messagebox_confirm(const char *title, const char *message) {
    WCHAR *wide_title   = w32_utf8_to_wide(title, nb_temporary_allocator);
    WCHAR *wide_message = w32_utf8_to_wide(message, nb_temporary_allocator);

    int result = MessageBoxW(null, wide_message, wide_title, MB_YESNO|MB_ICONINFORMATION);
    return (result == IDYES);
}

NB_EXTERN u32 
bender_messagebox_abort(const char *title, const char *message) {
    WCHAR *wide_title   = w32_utf8_to_wide(title, nb_temporary_allocator);
    WCHAR *wide_message = w32_utf8_to_wide(message, nb_temporary_allocator);

    int result = MessageBoxW(null, wide_message, wide_title, MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_SYSTEMMODAL);
    
    if (result == IDABORT) {
        return B_STATE_ABORT;
    } else if (result == IDRETRY) {
        return B_STATE_RETRY;
    } else {
        return B_STATE_IGNORE;
    }
}

#endif  // OS_WINDOWS
