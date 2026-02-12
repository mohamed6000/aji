#ifndef NB_INCLUDE_GENERAL_H
#define NB_INCLUDE_GENERAL_H

/*

    A general layer for C++ projects.

    Use:    #define NB_IMPLEMENTATION
    before: #include "general.h"
    to include the implementation code.


    Control:

    #define NB_ENABLE_ASSERTS 1 (0 by default, 1 for debug build)
    #define NB_ENABLE_DEFERS 1 (0 by default)

    #define NB_INCLUDE_WINDEFS (undefined by default) use it to include 
        custom "windefs.h" file instead of <windows.h>

    #define NB_WINDOWS_CONSOLE_USE_ANSI (undefined by default) uses
        the ansi colors instead of win32 console text attributes 
        for win32 platform.

    #define NB_NO_ASSERT (undefined by default) use it to prevent defining assert(x)


    To define a custom logger:

        void logger_name(Log_Mode mode, const char *ident, const char *message, ...) {...}

    To get printf like compile-time checking, forward declare with:
        void logger_name(Log_Mode mode, const char *ident, const char *message, ...) NB_IS_PRINTF_LIKE(3, 4);

    The Logger_Proc typedef can be helpful in some cases.

*/

#ifndef NB_DEBUG
#define NB_DEBUG 1
#endif

#if NB_DEBUG
#define NB_ENABLE_ASSERTS 1
#endif

#ifndef NB_ENABLE_ASSERTS
#define NB_ENABLE_ASSERTS 0
#endif

#ifndef NB_ENABLE_DEFERS
#define NB_ENABLE_DEFERS 0
#endif

/******** Compiler detection ********/

#if defined(__clang__)
    // Clang compiler.
    #define COMPILER_CLANG 1

    // Platform detection for clang.
    #if defined(_WIN32)
        #define OS_WINDOWS 1
    #elif defined(__gnu_linux__)
        #define OS_LINUX 1
    #elif defined(__APPLE__) && defined(__MACH__)
        #define OS_MAC 1
    #else
        #error Missing OS platform
    #endif

    // Architecture detection for clang.
    #if defined(__amd64__)
        #define ARCH_X64 1
    #elif defined(__i386__)
        #define ARCH_X86 1
    #elif defined(__arm__)
        #define ARCH_ARM 1
    #elif defined(__aarch64__)
        #define ARCH_ARM64 1
    #else 
        #error Unknown architecture
    #endif

#elif defined(_MSC_VER)
    // Microsoft compiler.
    #define COMPILER_CL 1

    // Platform detection for msvc.
    #if defined(_WIN32)
        #define OS_WINDOWS 1
    #else
        #error Missing OS platform
    #endif

    // Architecture detection for msvc.
    #if defined(_M_AMD64)
        #define ARCH_X64 1
    #elif defined(_M_IX86)
        #define ARCH_X86 1
    #elif defined(_M_ARM)
        #define ARCH_ARM 1
    #else
        #error Unknown architecture
    #endif

#elif defined(__GNUC__)
    // GNU C Compiler.
    #define COMPILER_GCC 1

    // Platform detection for gcc.
    #if defined(_WIN32)
        #define OS_WINDOWS 1
    #elif defined(__gnu_linux__)
        #define OS_LINUX 1
    #elif defined(__APPLE__) && defined(__MACH__)
        #define OS_MAC 1
    #else
        #error Missing OS platform
    #endif

    // Architecture detection for gcc.
    #if defined(__amd64__)
        #define ARCH_X64 1
    #elif defined(__i386__)
        #define ARCH_X86 1
    #elif defined(__arm__)
        #define ARCH_ARM 1
    #elif defined(__aarch64__)
        #define ARCH_ARM64 1
    #else
        #error Unknown architecture
    #endif
#endif


// Language detection.
#ifdef __cplusplus
    #define LANGUAGE_CPP 1
#else
    #define LANGUAGE_C 1
#endif


#if !defined(COMPILER_CL)
    #define COMPILER_CL 0
#endif
#if !defined(COMPILER_CLANG)
    #define COMPILER_CLANG 0
#endif
#if !defined(COMPILER_GCC)
    #define COMPILER_GCC 0
#endif

#if !defined(OS_WINDOWS)
    #define OS_WINDOWS 0
#endif
#if !defined(OS_LINUX)
    #define OS_LINUX 0
#endif
#if !defined(OS_MAC)
    #define OS_MAC 0
#endif

#if !defined(ARCH_X64)
    #define ARCH_X64 0
#endif
#if !defined(ARCH_X86)
    #define ARCH_X86 0
#endif
#if !defined(ARCH_ARM)
    #define ARCH_ARM 0
#endif
#if !defined(ARCH_ARM64)
    #define ARCH_ARM64 0
#endif

// Arch cracking.
#if ARCH_X86 || ARCH_X64 || ARCH_ARM || ARCH_ARM64
#define ARCH_LITTLE_ENDIAN 1
#else
#error Unknown architecture endianness
#endif

#if !defined(LANGUAGE_CPP)
    #define LANGUAGE_CPP 0
#endif
#if !defined(LANGUAGE_C)
    #define LANGUAGE_C 0
#endif


#if COMPILER_CL
#define _CRT_SECURE_NO_WARNINGS (1)
#include <intrin.h>
#endif


/******** Primitives ********/
#include <stdint.h>
#include <stddef.h>

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef float  float32;
typedef double float64;

typedef uintptr_t umm;
typedef intptr_t  smm;

typedef u64 b64;  // For consistency.
typedef u32 b32;
typedef u16 b16;  // For consistency.
typedef u8  b8;   // For consistency.


#if LANGUAGE_CPP
#define null 0
#define NB_EXTERN extern "C"
#else
#define null ((void *)0)
#define NB_EXTERN
#endif

#if COMPILER_CL
#define nb_thread_local __declspec(thread)
#elif COMPILER_CLANG || COMPILER_GCC
#define nb_thread_local __thread
#else
#error Undefined nb_thread_local for this compiler
#endif

#if COMPILER_CL
#define NB_INLINE __forceinline
#elif COMPILER_CLANG
#define NB_INLINE __attribute__((always_inline))
#elif COMPILER_GCC
#define NB_INLINE __attribute__((always_inline)) inline
#else
#error Undefined inline for this compiler
#endif

#if COMPILER_CL
#define NB_NO_INLINE __declspec(noinline)
#elif COMPILER_CLANG || COMPILER_GCC
#define NB_NO_INLINE __attribute__((noinline))
#else
#error Undefined no_inline for this compiler
#endif

#if OS_WINDOWS
    #if COMPILER_CL
    #define NB_SHARED_EXPORT __declspec(dllexport)
    #elif COMPILER_CLANG
    #define NB_SHARED_EXPORT __declspec(dllexport)
    #elif COMPILER_GCC
    #define NB_SHARED_EXPORT __declspec(dllexport)
    #else
    #error Undefined NB_SHARED_EXPORT for this compiler
    #endif
#elif OS_LINUX || OS_MAC
    #if COMPILER_GCC || COMPILER_CLANG
    #define NB_SHARED_EXPORT __attribute__((visibility("default")))
    #else
    #error Undefined NB_SHARED_EXPORT for this compiler
    #endif
#else
#error Undefined NB_SHARED_EXPORT for this compiler
#endif

#if LANGUAGE_CPP
#define offset_of(Type, member) offsetof(Type, member)
#else
#define offset_of(Type, member) ((umm)(&((Type *)0)->member))
#endif

#define size_of(x) sizeof(x)
#define array_count(a) (size_of(a) / size_of((a)[0]))

#define align_forward_offset(s, a) (((s) & ((a)-1)) ? ((a) - ((s) & ((a)-1))) : 0)
#define align_forward(s, a) (((s) + ((a)-1)) & ~((a)-1))

#define align_forward_pointer(p, a) ((u8 *)(((umm)(p) + ((a)-1)) & ~((a)-1)))


#if COMPILER_GCC || COMPILER_CLANG
#define NB_IS_PRINTF_LIKE(fmtarg, first_vararg) __attribute__((__format__ (printf, fmtarg, first_vararg)))
#else
#define NB_IS_PRINTF_LIKE(fmtarg, first_vararg)
#endif


// Assuming the input is non-zero.
#define is_power_of_2(x) (((x) & ((x)-1)) == 0)

#define UNUSED(x) (void)(x)
#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))

#define pointer_to_int(ptr) (u64)((uintptr_t)(ptr))
#define pointer_to_type(Type, ptr) (Type)((uintptr_t)(ptr))

#define u32_to_pointer(Type, n) (Type *)((uintptr_t)(n))
#define int_to_pointer(Type, n) (Type *)((uintptr_t)(n))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef TAU
#define TAU 6.283185307179586476925
#endif


#if COMPILER_GCC
    #define nb_debug_break() __builtin_trap()
#elif COMPILER_CL
    #define nb_debug_break() __debugbreak()
#elif COMPILER_CLANG
    #define nb_debug_break() __builtin_trap()
#else
    #if ARCH_X64
        #define nb_debug_break() *((int *)null) = 0
    #else
        #error Unknown architecture for nb_debug_break
    #endif
#endif


NB_EXTERN char *nb_get_stacktrace(void);


#ifndef NB_NO_ASSERT
#if NB_ENABLE_ASSERTS
// https://nullprogram.com/blog/2022/06/26/
#undef assert
#define assert(expression) \
do { \
    if (!(expression)) { \
        nb_set_console_text_color(NB_TEXT_RED, true); \
        nb_write_string("Assertion Failure: " STRINGIFY(expression) " at " __FILE__ ":" STRINGIFY(__LINE__) "\n", true); \
        nb_set_console_text_color(NB_TEXT_LIGHT_GRAY, true); \
        char *stack_trace_details = nb_get_stacktrace();  \
        nb_write_string(stack_trace_details, true);  \
        if (nb_abort_error_message("Assertion Failed", "Assert Failed\n" STRINGIFY(expression) "\nAt: " __FILE__ ":" STRINGIFY(__LINE__) "\n", /*char *details=*/stack_trace_details)) { \
            nb_debug_break(); \
        } \
        nb_set_console_text_color(NB_TEXT_WHITE, true); \
    } \
} while (0)

#else
#undef assert
#define assert(expression)
#endif  // NB_ENABLE_ASSERTS
#endif  // NB_NO_ASSERT


#if NB_ENABLE_DEFERS
#if LANGUAGE_CPP
template<typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda):lambda(lambda){}
    ~ExitScope(void){lambda();}
    ExitScope(const ExitScope &);
private:
    ExitScope &operator =(const ExitScope &);
};

struct ExitScopeHelp {
    template<typename T>
    ExitScope<T> operator+(T t){ return t; }
};

#define defer const auto& CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()
#endif  // LANGUAGE_CPP
#endif  // NB_ENABLE_DEFERS


#define NB_INVALID_DEFAULT_CASE() default: {assert(!"Invalid Default Case");} break


#if LANGUAGE_C
#include <stdbool.h>

#define nb_static_assert(c, m) typedef char CONCAT(static_assertion,__LINE__)[(c)?1:-1]
#else
#define nb_static_assert(c, m) static_assert(c, m)
#endif

nb_static_assert(size_of(s8)  == 1, "size_of(s8)  == 1");
nb_static_assert(size_of(s16) == 2, "size_of(s16) == 2");
nb_static_assert(size_of(s32) == 4, "size_of(s32) == 4");
nb_static_assert(size_of(s64) == 8, "size_of(s64) == 8");

nb_static_assert(size_of(u8)  == 1, "size_of(u8)  == 1");
nb_static_assert(size_of(u16) == 2, "size_of(u16) == 2");
nb_static_assert(size_of(u32) == 4, "size_of(u32) == 4");
nb_static_assert(size_of(u64) == 8, "size_of(u64) == 8");

nb_static_assert(size_of(float32) == 4, "size_of(float32) == 4");
nb_static_assert(size_of(float64) == 8, "size_of(float64) == 8");

nb_static_assert(size_of(umm) == size_of(void *), "size_of(umm) == size_of(void *)");
nb_static_assert(size_of(smm) == size_of(void *), "size_of(smm) == size_of(void *)");

nb_static_assert(size_of(b8)  == 1, "size_of(b8)  == 1");
nb_static_assert(size_of(b16) == 2, "size_of(b16) == 2");
nb_static_assert(size_of(b32) == 4, "size_of(b32) == 4");
nb_static_assert(size_of(b64) == 8, "size_of(b64) == 8");

/******** Min/Max type sizes ********/

#define MIN_S8  0x80 // -128
#define MAX_S8  0x7f // 127

#define MIN_S16  0x8000 // -32768
#define MAX_S16  0x7fff // 32767

#define MIN_S32  0x80000000 // -2147483648
#define MAX_S32  0x7fffffff // 2147483647

#define MIN_S64  ((s64)0x8000000000000000ull) // -2^63
#define MAX_S64  ((s64)0x7fffffffffffffffull) // 2^63 - 1

#define MAX_U8   0xff               // 255
#define MAX_U16  0xffff             // 65535
#define MAX_U32  0xffffffff         // 4294967295
#define MAX_U64  0xffffffffffffffff // 2^64 - 1

#define F32_MIN  1.17549435e-38f
#define F32_MAX  3.40282347e+38f

#define F64_MIN   2.2250738585072014e-308
#define F64_MAX   1.7976931348623157e+308


#define BIT(x) (1 << (x))
#define KB(x) ((u64)(x) << 10ull)
#define MB(x) ((u64)(x) << 20ull)
#define GB(x) ((u64)(x) << 30ull)


// Memory.
#include <string.h>
#define nb_memory_zero(p, s)     memset((p), 0, (s))
#define nb_memory_zero_struct(p) nb_memory_zero((p), size_of(*(p)))
#define nb_memory_zero_array(p)  nb_memory_zero((p), size_of(p))


typedef enum NB_OS_Type {
    NB_OS_NONE,
    NB_OS_WINDOWS,
    NB_OS_LINUX,
    NB_OS_MAC,

    NB_OS_COUNT,
} NB_OS_Type;

typedef enum NB_Arch_Type {
    NB_ARCH_NONE,
    NB_ARCH_X64,
    NB_ARCH_X86,
    NB_ARCH_ARM,
    NB_ARCH_ARM64,
    
    NB_ARCH_COUNT,
} NB_Arch_Type;

// Custom Allocators.

typedef enum NB_Allocator_Mode {
    NB_ALLOCATOR_ALLOCATE,
    NB_ALLOCATOR_RESIZE,
    NB_ALLOCATOR_FREE,
    NB_ALLOCATOR_FREE_ALL,
} NB_Allocator_Mode;

#define NB_ALLOCATOR_PROC(name) void *name(NB_Allocator_Mode mode, s64 size, s64 old_size, void *old_memory, void *allocator_data)

typedef NB_ALLOCATOR_PROC(NB_Allocator_Proc);

typedef struct NB_Allocator {
    NB_Allocator_Proc *proc;
    void *data;
} Allocator;

extern nb_thread_local NB_Allocator nb_current_allocator;

#define NB_SET_ALLOCATOR(a) do { nb_current_allocator = a; } while (0)
#define NB_GET_ALLOCATOR() (nb_current_allocator)

// Heap allocator.
NB_EXTERN void *nb_heap_allocator(NB_Allocator_Mode mode, s64 size, s64 old_size, void *old_memory, void *allocator_data);

#define nb_heap_alloc(s) nb_heap_allocator(NB_ALLOCATOR_ALLOCATE, (s), 0, null, null)
#define nb_heap_realloc(mem, size, old_size) nb_heap_allocator(NB_ALLOCATOR_RESIZE, (size), (old_size), (mem), null)
#define nb_heap_free(mem) nb_heap_allocator(NB_ALLOCATOR_FREE, 0, 0, (mem), null)

#if COMPILER_CL
#define nb_new(Type, ...) (Type *)nb_new_alloc(size_of(Type), __VA_ARGS__)
#else
#define nb_new(Type, ...) (Type *)nb_new_alloc(size_of(Type), ##__VA_ARGS__)
#endif

#ifndef New
#define New nb_new
#endif

#if COMPILER_CL
#define nb_new_array(Type, count, ...) (Type *)nb_new_alloc((count) * size_of(Type), __VA_ARGS__)
#else
#define nb_new_array(Type, count, ...) (Type *)nb_new_alloc((count) * size_of(Type), ##__VA_ARGS__)
#endif

#if COMPILER_CL
#define nb_realloc(m, new_size, old_size, ...) nb_mem_realloc((m), (new_size), (old_size), __VA_ARGS__)
#else
#define nb_realloc(m, new_size, old_size, ...) nb_mem_realloc((m), (new_size), (old_size), ##__VA_ARGS__)
#endif

#if COMPILER_CL
#define nb_free(m, ...) nb_mem_free((m), __VA_ARGS__)
#else
#define nb_free(m, ...) nb_mem_free((m), ##__VA_ARGS__)
#endif



/******** Temporary Storage ********/
#define NB_TS_SIZE_DEFAULT KB(40)

typedef struct NB_Temporary_Storage {
    s64 size;
    u8 *data;

    s64 occupied;
    s64 high_water_mark;

    Allocator allocator;
} NB_Temporary_Storage;

extern nb_thread_local NB_Temporary_Storage nb_temporary_storage;
extern nb_thread_local NB_Allocator nb_temporary_allocator;

NB_EXTERN NB_ALLOCATOR_PROC(nb_temporary_storage_proc);


/******** String ********/

typedef struct String {
    s64 count;
    u8 *data;
} String;

#if LANGUAGE_CPP
#define S(s) String{size_of(s)-1, (u8 *)(s)}
#else
#define S(s) (String){size_of(s)-1, (u8 *)(s)}
#endif

inline String make_string(u8 *data, s64 count) {
    String result;
    result.count = count;
    result.data  = data;
    return result;
}



/******** Custom Loggers ********/

typedef enum NB_Log_Mode {
    NB_LOG_NONE,
    NB_LOG_ERROR,
    NB_LOG_WARNING,
} Log_Mode;

typedef void NB_Logger_Proc(NB_Log_Mode mode, const char *ident, const char *message, ...) NB_IS_PRINTF_LIKE(3, 4);

extern nb_thread_local NB_Logger_Proc *nb_current_logger;

#if COMPILER_CL
#define nb_log(mode, ident, message, ...) nb_current_logger((mode), (ident), (message), __VA_ARGS__)
#else
#define nb_log(mode, ident, message, ...) nb_current_logger((mode), (ident), (message), ##__VA_ARGS__)
#endif

#ifndef Log
#define Log nb_log
#endif

#define NB_SET_LOGGER(l) do { nb_current_logger = l; } while (0)
#define NB_GET_LOGGER() (nb_current_logger)

// Cross-platform write string functions.
void nb_write_string(const char *s, bool to_standard_error = false);
void nb_write_string_count(const char *s, u32 count, bool to_standard_error = false);

void nb_write_new_string(String s, bool to_standard_error = false);

NB_EXTERN bool nb_abort_error_message(const char *title, const char *message, const char *details);

typedef enum NB_System_Console_Text_Color {
    NB_TEXT_BLACK,
    NB_TEXT_DARK_BLUE,
    NB_TEXT_DARK_GREEN,
    NB_TEXT_LIGHT_BLUE,
    NB_TEXT_DARK_RED,
    NB_TEXT_MAGENTA,
    NB_TEXT_ORANGE,
    NB_TEXT_LIGHT_GRAY,
    NB_TEXT_GRAY,
    NB_TEXT_BLUE,
    NB_TEXT_GREEN,
    NB_TEXT_CYAN,
    NB_TEXT_RED,
    NB_TEXT_PURPLE,
    NB_TEXT_YELLOW,
    NB_TEXT_WHITE,

    NB_TEXT_COUNT
} NB_System_Console_Text_Color;

NB_EXTERN void nb_set_console_text_color(NB_System_Console_Text_Color color, bool to_standard_error = false);
NB_EXTERN void nb_set_console_text_color_ansi(NB_System_Console_Text_Color color, bool to_standard_error = false);

// Temporary storage helpers.
inline s64 nb_get_temporary_storage_mark(void) {
    return nb_temporary_storage.occupied;
}

inline void nb_set_temporary_storage_mark(s64 mark) {
    assert(mark >= 0);
    assert(mark <= nb_temporary_storage.size);
    nb_temporary_storage.occupied = mark;
}

inline void nb_reset_temporary_storage(void) {
    nb_set_temporary_storage_mark(0);
    nb_temporary_storage.high_water_mark = 0;
}


/*

mprint():
  
  allocates the string on the fly,
  providing an initial guess for the string is possible,
  requires calling nb_free for the result,
  in case of failure it returns null.

*/

#include <stdarg.h>

#define NB_PRINT_INITIAL_GUESS 256

char *mprint(const char *fmt, ...) NB_IS_PRINTF_LIKE(1, 2);
char *mprint_guess(int initial_guess, const char *fmt, ...) NB_IS_PRINTF_LIKE(2, 3);
NB_EXTERN char *mprint_valist(const char *fmt, va_list arg_list);

NB_EXTERN char *tprint(const char *fmt, ...) NB_IS_PRINTF_LIKE(1, 2);
NB_EXTERN char *tprint_valist(const char *fmt, va_list arg_list);

NB_EXTERN void print(const char *fmt, ...) NB_IS_PRINTF_LIKE(1, 2);

inline void nb_default_logger(NB_Log_Mode mode, const char *ident, const char *message, ...) {
    UNUSED(mode);

    if (ident) {
        nb_write_string("[");
        nb_write_string(ident);
        nb_write_string("] ");
    }

#if 0
    nb_write_string(message);
#else
    s64 mark = nb_get_temporary_storage_mark();
    va_list args;
    va_start(args, message);

    char *s = tprint_valist(message, args);
    va_end(args);

    nb_write_string(s);
    nb_set_temporary_storage_mark(mark);
#endif

    nb_write_string("\n");
}

inline void nb_error_logger(NB_Log_Mode mode, const char *ident, const char *message, ...) {
    UNUSED(mode);

    if (ident) {
        nb_write_string("[", true);
        nb_write_string(ident, true);
        nb_write_string("] ", true);
    }

    nb_write_string(message, true);
    nb_write_string("\n", true);
}



inline u32 nb_safe_truncate_u64(u64 value) {
    assert(value <= MAX_U32);
    u32 result = (u32)value;
    return result;
}



inline void *nb_new_alloc(s64 size, NB_Allocator a = NB_GET_ALLOCATOR()) {
    assert(a.proc != null);
    return a.proc(NB_ALLOCATOR_ALLOCATE, size, 0, null, a.data);
}

inline void *nb_mem_realloc(void *mem, s64 new_size, s64 old_size, NB_Allocator a = NB_GET_ALLOCATOR()) {
    assert(a.proc != null);
    return a.proc(NB_ALLOCATOR_RESIZE, new_size, old_size, mem, a.data);
}

inline void nb_mem_free(void *mem, NB_Allocator a = NB_GET_ALLOCATOR()) {
    assert(a.proc != null);
    a.proc(NB_ALLOCATOR_FREE, 0, 0, mem, a.data);
}


/******** Quick Sort ********/
NB_EXTERN void nb_qsort(void *data, s64 count, s64 stride, s64 (*qsort_compare)(void *, void *));
NB_EXTERN void nb_qsort_it(void *data, s64 count, s64 stride, s64 (*qsort_compare)(void *, void *));



/******** Utility functions ********/

inline u16 nb_swap2(u16 mem) {
    u16 a = mem >> 8;
    u16 b = (mem & 0xFF) << 8;
    
    u16 result = a | b;
    return result;
}

inline u32 nb_swap4(u32 mem) {
    u32 a = (mem >> 24);
    u32 b = (mem >> 16) & 0xFF;
    u32 c = (mem >>  8) & 0xFF;
    u32 d = (mem) & 0xFF;
    
    u32 result = (d << 24) | (c << 16) | (b << 8) | a;
    return result;
}

inline u32 nb_find_least_significant_set_bit(u32 value) {
#if COMPILER_CL
    unsigned long result = 0;
    _BitScanForward(&result, value);
    return (u32)result;
#elif COMPILER_GCC
    return (u32)__builtin_ctz(value);
#else
    for (u32 test = 0; test < 32; ++test) {
        if (value & (1 << test)) {
            return test;
        }
    }

    return 0;
#endif
}

inline void nb_swap_two_memory_blocks(u8 *a_, u8 *b_, s64 count) {
    u8 *a = a_;
    u8 *b = b_;

    while (count--) {
        u8 temp = *a;
        *a++    = *b;
        *b++    = temp;
    }
}

#define nb_get_clamped(v, l, h) (((v) < (l)) ? (l) : (((v) > (h)) ? (h) : (v)))

#if LANGUAGE_CPP
inline void nb_clamp(int *pointer, int low, int high) {
    if (*pointer < low) { *pointer = low; }
    if (*pointer > high) { *pointer = high; }
}

inline void nb_clamp(float *pointer, float low, float high) {
    if (*pointer < low) { *pointer = low; }
    if (*pointer > high) { *pointer = high; }
}
#endif

#ifndef Clamp
#define Clamp nb_clamp
#endif

inline NB_OS_Type nb_get_current_os(void) {
    NB_OS_Type result = NB_OS_NONE;
#if OS_WINDOWS
    result = NB_OS_WINDOWS;
#elif OS_LINUX
    result = NB_OS_LINUX;
#elif OS_MAC
    result = NB_OS_MAC;
#endif

    return result;
}

inline NB_Arch_Type nb_get_current_arch(void) {
    NB_Arch_Type result = NB_ARCH_NONE;
#if ARCH_X64
    result = NB_ARCH_X64;
#elif ARCH_X86
    result = NB_ARCH_X86;
#elif ARCH_ARM
    result = NB_ARCH_ARM;
#elif ARCH_ARM64
    result = NB_ARCH_ARM64;
#endif

    return result;
}

inline const char *nb_os_to_string(NB_OS_Type os) {
    const char *result = null;
    switch (os) {
        case NB_OS_WINDOWS:
            result = "Windows";
            break;

        case NB_OS_LINUX:
            result = "Linux";
            break;

        case NB_OS_MAC:
            result = "Mac";
            break;

        default: break;
    }

    return result;
}

inline const char *nb_arch_to_string(NB_Arch_Type arch) {
    const char *result = null;
    switch (arch) {
        case NB_ARCH_X64:
            result = "X64";
            break;

        case NB_ARCH_X86:
            result = "X86";
            break;

        case NB_ARCH_ARM:
            result = "ARM";
            break;

        case NB_ARCH_ARM64:
            result = "ARM64";
            break;

        default: break;
    }

    return result;
}

inline void nb_panic(void) {
    nb_write_new_string(S("Panic.\n"));
#if NB_DEBUG
    nb_debug_break();
#else
    // abort();
#endif
}

// Ratio helpers.

inline float nb_safe_ratio_0(float n, float d) {
    float result = 0;

    if (d != 0) {
        result = n/d;
    }

    return result;
}

inline float nb_safe_ratio_1(float n, float d) {
    float result = 1;

    if (d != 0) {
        result = n/d;
    }

    return result;
}

inline float nb_safe_ratio_n(float a, float b, float n) {
    float result = n;

    if (b != 0) {
        result = a/b;
    }

    return result;
}

// String helpers.

inline bool nb_strings_are_equal(String a, String b) {
    if (a.count != b.count) return false;

    for (s64 index = 0; index < a.count; ++index) {
        if (a.data[index] != b.data[index]) {
            return false;
        }
    }

    return true;
}

inline bool nb_cstrings_are_equal(char *a, char *b) {
    bool result = (a == b);

    if (a && b) {
        while (*a && *b && (*a == *b)) {
            ++a;
            ++b;
        }

        result = ((*a == 0) && (*b == 0));
    }

    return result;
}

inline bool nb_strings_are_equal_length(s64 length_a, char *a, s64 length_b, char *b) {
    if (length_a != length_b) return false;

    for (s64 index = 0; index < length_a; ++index) {
        if (a[index] != b[index]) return false;
    }

    return true;
}

inline bool nb_strings_are_equal_first_length(s64 length_a, char *a, char *b) {
    char *it = b;
    for (s64 index = 0; index < length_a; ++index, ++it) {
        if ((*it == 0) || (a[index] != *it)) {
            return false;
        }
    }

    bool result = (*it == 0);
    return result;
}

inline bool nb_is_end_of_line(int c) {
    bool result = ((c == '\n') || (c == '\r'));
    return result;
}

inline bool nb_is_white_space(int c) {
    bool result = ((c == ' ') || 
                   (c == '\t') ||
                   (c == '\v') || 
                   (c == '\f') || 
                   (c == '\n') ||
                   (c == '\r'));

    return result;
}

inline bool nb_is_digit(int c) {
    bool result = (c >= '0') && (c <= '9');
    return result;
}

inline s64 nb_string_length(const char *s) {
    if (!s) return 0;

    const char *it = s;
    while (*it++);

    return (s64)(it-1 - s);
}

inline char *nb_eat_spaces(char *s) {
    char *it = s;
    while (*it == ' ') {
        it++;
    }

    return it;
}

inline char *nb_get_extension(char *s) {
    char *result = null;

    char *it = s;
    while (*it) {
        if (*it == '.') result = it;

        it++;
    }

    return result;
}

inline char *nb_find_character_from_right(char *s, u8 c) {
    s64 length = nb_string_length(s);

    for (s64 i = length - 1; i >= 0; --i) {
        if (s[i] == c) {
            return s + i;
        }
    }

    return null;
}

inline char *nb_path_cleanup(char *s) {
    char *it = s;
    while (*it) {
        if (*it == '\\') *it = '/';

        it++;
    }

    return s;
}

inline void nb_advance(String *s, s64 amount) {
    s->data  += amount;
    s->count -= amount;
}

#endif  // NB_INCLUDE_GENERAL_H













/******** General Implementation ********/

#ifdef NB_IMPLEMENTATION

nb_thread_local NB_Logger_Proc *nb_current_logger = nb_default_logger;
nb_thread_local NB_Allocator nb_current_allocator = {nb_heap_allocator, null};

// @Cleanup:
nb_thread_local NB_Temporary_Storage nb_temporary_storage;
nb_thread_local NB_Allocator nb_temporary_allocator = {nb_temporary_storage_proc, null};


#if OS_WINDOWS

#ifdef NB_INCLUDE_WINDEFS
#include "windefs.h"
#else
#define UNICODE
#define _UNICODE

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#endif


#if COMPILER_CL
#pragma comment(lib, "User32.lib")
#endif


void nb_write_string(const char *s, bool to_standard_error) {
    DWORD written = 0;
    HANDLE handle = to_standard_error ? GetStdHandle(STD_ERROR_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);
    s32 status = WriteFile(handle, s, (DWORD)nb_string_length(s), &written, null);
    UNUSED(status);
}

void nb_write_string_count(const char *s, u32 count, bool to_standard_error) {
    DWORD written = 0;
    HANDLE handle = to_standard_error ? GetStdHandle(STD_ERROR_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);
    s32 status = WriteFile(handle, s, count, &written, null);
    UNUSED(status);
}

void nb_write_new_string(String s, bool to_standard_error) {
    DWORD written = 0;
    HANDLE handle = to_standard_error ? GetStdHandle(STD_ERROR_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);
    s32 status = WriteFile(handle, s.data, (DWORD)s.count, &written, null);
    UNUSED(status);
}

static const char *ansi_system_console_text_colors[NB_TEXT_COUNT] = {
    "\x1b[30m",   // NB_TEXT_BLACK
    "\x1b[34m",   // NB_TEXT_DARK_BLUE
    "\x1b[32m",   // NB_TEXT_DARK_GREEN
    "\x1b[36m",   // NB_TEXT_LIGHT_BLUE
    "\x1b[31m",   // NB_TEXT_DARK_RED
    "\x1b[35m",   // NB_TEXT_MAGENTA
    "\x1b[33m",   // NB_TEXT_ORANGE
    "\x1b[37m",   // NB_TEXT_LIGHT_GRAY
    "\x1b[90m",   // NB_TEXT_GRAY
    "\x1b[94m",   // NB_TEXT_BLUE
    "\x1b[92m",   // NB_TEXT_GREEN
    "\x1b[96m",   // NB_TEXT_CYAN
    "\x1b[91m",   // NB_TEXT_RED
    "\x1b[95m",   // NB_TEXT_PURPLE
    "\x1b[93m",   // NB_TEXT_YELLOW
    "\x1b[97m",   // NB_TEXT_WHITE
};

static u8 w32_system_console_text_colors[NB_TEXT_COUNT] = {
    0,   // NB_TEXT_BLACK
    1,   // NB_TEXT_DARK_BLUE
    2,   // NB_TEXT_DARK_GREEN
    3,   // NB_TEXT_LIGHT_BLUE
    4,   // NB_TEXT_DARK_RED
    5,   // NB_TEXT_MAGENTA
    6,   // NB_TEXT_ORANGE
    7,   // NB_TEXT_LIGHT_GRAY
    8,   // NB_TEXT_GRAY
    9,   // NB_TEXT_BLUE
    10,  // NB_TEXT_GREEN
    11,  // NB_TEXT_CYAN
    12,  // NB_TEXT_RED
    13,  // NB_TEXT_PURPLE
    14,  // NB_TEXT_YELLOW
    15,  // NB_TEXT_WHITE
};

NB_EXTERN void nb_set_console_text_color(NB_System_Console_Text_Color color, bool to_standard_error) {
#ifdef NB_WINDOWS_CONSOLE_USE_ANSI
    nb_write_string(ansi_system_console_text_colors[color], to_standard_error);
#else
    HANDLE handle = to_standard_error ? GetStdHandle(STD_ERROR_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(handle, w32_system_console_text_colors[color]);
#endif
}

NB_EXTERN void nb_set_console_text_color_ansi(NB_System_Console_Text_Color color, bool to_standard_error) {
    nb_write_string(ansi_system_console_text_colors[color], to_standard_error);
}

#include <stdio.h>

char *mprint(const char *fmt, ...) {
    char *result = null;
    int size = NB_PRINT_INITIAL_GUESS;

    while (1) {
        result = nb_new_array(char, size);
        if (!result) return null;
        
        va_list args;
        va_start(args, fmt);
        
        int len = _vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            size = len;
            break;
        }

        nb_free(result);
        size *= 2;
    }

    return result;
}

char *mprint_guess(int size, const char *fmt, ...) {
    assert(size > 0);
    
    char *result = null;

    while (1) {
        result = nb_new_array(char, size);
        if (!result) return null;
        
        va_list args;
        va_start(args, fmt);
        
        int len = _vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            size = len;
            break;
        }

        nb_free(result);
        size *= 2;
    }

    return result;
}

NB_EXTERN char *mprint_valist(const char *fmt, va_list arg_list) {
    char *result = null;
    int size = NB_PRINT_INITIAL_GUESS;

    while (1) {
        result = nb_new_array(char, size);
        if (!result) return null;
        
        va_list args;
        args = arg_list;
        
        int len = _vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            size = len;
            break;
        }

        nb_free(result);
        size *= 2;
    }

    return result;
}

NB_EXTERN char *tprint(const char *fmt, ...) {
    char *result = null;

    // Initial guess.
    int size = NB_PRINT_INITIAL_GUESS;
    NB_Allocator allocator = nb_temporary_allocator;
    NB_Temporary_Storage *ts = &nb_temporary_storage;

    while (1) {
        s64 mark = nb_get_temporary_storage_mark();
        result = nb_new_array(char, size, allocator);
        if (!result) return null;
        
        va_list args;
        va_start(args, fmt);
        
        int len = _vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            ts->occupied -= (size - len - 1);
            size = len;
            break;
        }

        nb_set_temporary_storage_mark(mark);
        size *= 2;
    }

    return result;
}

NB_EXTERN char *tprint_valist(const char *fmt, va_list arg_list) {
    char *result = null;
    int size = NB_PRINT_INITIAL_GUESS;
    NB_Allocator allocator = nb_temporary_allocator;
    NB_Temporary_Storage *ts = &nb_temporary_storage;

    while (1) {
        s64 mark = nb_get_temporary_storage_mark();
        result = nb_new_array(char, size, allocator);
        if (!result) return null;

        va_list args;
        args = arg_list;
        
        int len = _vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            ts->occupied -= (size - len - 1);
            size = len;
            break;
        }

        nb_set_temporary_storage_mark(mark);
        size *= 2;
    }

    return result;
}

NB_EXTERN void print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char *s = tprint_valist(fmt, args);
    va_end(args);

    nb_write_string(s);
}

NB_EXTERN bool nb_abort_error_message(const char *title, const char *message, const char *details) {
    char *full_message = tprint("%s%s", message, details);

    int id = MessageBoxA(null, full_message, title, MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_SYSTEMMODAL | MB_DEFBUTTON3);
    if (id == IDABORT) {
        ExitProcess(0);
    }

    return (id == IDRETRY);
}

#if NB_ENABLE_ASSERTS
#include <dbghelp.h>
#if COMPILER_CL
#pragma comment(lib, "Dbghelp.lib")
#endif

#include <inttypes.h>

NB_EXTERN char *nb_get_stacktrace(void) {
    char *result = null;
    HANDLE process = GetCurrentProcess();
 
    static BOOL initted = false;
    if (!initted) {
        initted = SymInitialize(process, null, TRUE);
        SymSetOptions(SYMOPT_LOAD_LINES);
    }

    if (initted == TRUE) {
        // Windows Server 2003 and Windows XP: The sum of the FramesToSkip and FramesToCapture parameters must be less than 63.
        const int MAX_STACK_FRAMES = 63;
        void *stack[MAX_STACK_FRAMES];

        const int MAX_SYMBOL_NAME = 255;
        u8 buf[size_of(SYMBOL_INFO) + (MAX_SYMBOL_NAME+1)];

        SYMBOL_INFO *symbol_info = (SYMBOL_INFO *)buf;
        symbol_info->MaxNameLen = MAX_SYMBOL_NAME;
        symbol_info->SizeOfStruct = size_of(SYMBOL_INFO);

        IMAGEHLP_LINEW64 line64 = {};
        line64.SizeOfStruct = size_of(IMAGEHLP_LINEW64);

        u16 frames = CaptureStackBackTrace(0, MAX_STACK_FRAMES, stack, null);
        if (frames > 0) {
            result = tprint("Caller stack:\n");
            for (u16 index = 0; index < frames; ++index) {
                DWORD64 dw_displacement64;
                BOOL ok = SymFromAddr(process, (DWORD64)(stack[index]), &dw_displacement64, symbol_info);
                if (!ok) continue;

                s64 call_line;
                s64 stack_line;

                DWORD64 call_address = (DWORD64)(stack[index]);  // Caller location.
                DWORD64 stack_address = symbol_info->Address;    // Procedure location.

                DWORD dw_displacement;
                ok = SymGetLineFromAddrW64(process, call_address, &dw_displacement, &line64);
                if (!ok) continue;

                call_line = line64.LineNumber;

                ok = SymGetLineFromAddrW64(process, stack_address, &dw_displacement, &line64);
                if (!ok) continue;

                stack_line = line64.LineNumber;

#if COMPILER_GCC
                char *s = tprint("%s0x%016I64u: %s(%I64d) Line %I64d\n", 
                                 result, 
                                 stack_address, 
                                 symbol_info->Name, 
                                 stack_line, 
                                 call_line);
#else
                char *s = tprint("%s0x%016" PRIXPTR ": %s(%" PRId64 ") Line %" PRId64 "\n", 
                                 result, 
                                 stack_address, 
                                 symbol_info->Name, 
                                 stack_line, 
                                 call_line);
#endif
                result = s;
            }
        }
    } else {
        nb_write_string("[backtrace] Error: Failed to SymInitialize.\n", true);
    }

    return result;
}
#endif  // NB_ENABLE_ASSERTS

WCHAR *w32_utf8_to_wide(const char *s, NB_Allocator allocator) {
    if (!s) return null;
    s64 byte_count = nb_string_length(s);

    int len = MultiByteToWideChar(CP_UTF8, 0, s, (int)byte_count, null, 0);
    if (len <= 0) return null;

    WCHAR *result = nb_new_array(WCHAR, len+1, allocator);
    MultiByteToWideChar(CP_UTF8, 0, s, (int)byte_count, result, len);
    result[len] = 0;

    return result;
}


NB_EXTERN void *
nb_heap_allocator(NB_Allocator_Mode mode, 
                  s64 size, s64 old_size, 
                  void *old_memory, 
                  void *allocator_data) {
    UNUSED(allocator_data);

    switch (mode) {
        case NB_ALLOCATOR_ALLOCATE:
            return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (umm)size);

        case NB_ALLOCATOR_RESIZE: {
            // Allocate, copy, free.

            void *result = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (umm)size);
            if (result == null) return null;

            if (old_memory && (old_size > 0)) {
                memcpy(result, old_memory, (umm)old_size);
                HeapFree(GetProcessHeap(), 0, old_memory);
            }

            return result;
        } break;

        case NB_ALLOCATOR_FREE: {
            HeapFree(GetProcessHeap(), 0, old_memory);
            return null;
        } break;

        case NB_ALLOCATOR_FREE_ALL: {
            // Not supported.
            assert(!"Not supported");
            return null;
        } break;

        default: {
            assert(false);
            return null;
        } break;
    }
}

#endif  // OS_WINDOWS



#if OS_LINUX

#include <unistd.h>
#include <stdlib.h>

void nb_write_string(const char *s, bool to_standard_error) {
    int handle = to_standard_error ? STDERR_FILENO : STDOUT_FILENO;
    ssize_t written = write(handle, s, nb_string_length(s));
    UNUSED(written);
}

void nb_write_string_count(const char *s, u32 count, bool to_standard_error) {
    int handle = to_standard_error ? STDERR_FILENO : STDOUT_FILENO;
    ssize_t written = write(handle, s, count);
    UNUSED(written);
}

void nb_write_new_string(String s, bool to_standard_error) {
    int handle = to_standard_error ? STDERR_FILENO : STDOUT_FILENO;
    ssize_t written = write(handle, s.data, s.count);
    UNUSED(written);
}

static const char *ansi_system_console_text_colors[NB_TEXT_COUNT] = {
    "\x1b[30m",   // NB_TEXT_BLACK
    "\x1b[34m",   // NB_TEXT_DARK_BLUE
    "\x1b[32m",   // NB_TEXT_DARK_GREEN
    "\x1b[36m",   // NB_TEXT_LIGHT_BLUE
    "\x1b[31m",   // NB_TEXT_DARK_RED
    "\x1b[35m",   // NB_TEXT_MAGENTA
    "\x1b[33m",   // NB_TEXT_ORANGE
    "\x1b[37m",   // NB_TEXT_LIGHT_GRAY
    "\x1b[90m",   // NB_TEXT_GRAY
    "\x1b[94m",   // NB_TEXT_BLUE
    "\x1b[92m",   // NB_TEXT_GREEN
    "\x1b[96m",   // NB_TEXT_CYAN
    "\x1b[91m",   // NB_TEXT_RED
    "\x1b[95m",   // NB_TEXT_PURPLE
    "\x1b[93m",   // NB_TEXT_YELLOW
    "\x1b[97m",   // NB_TEXT_WHITE
};

NB_EXTERN void 
nb_set_console_text_color(NB_System_Console_Text_Color color, 
                          bool to_standard_error) {
    // Checking if the handle is connected to a terminal.
    if (isatty(to_standard_error ? STDERR_FILENO : STDOUT_FILENO)) {
        nb_write_string(ansi_system_console_text_colors[color], to_standard_error);
    }
}

NB_EXTERN void 
nb_set_console_text_color_ansi(NB_System_Console_Text_Color color, 
                               bool to_standard_error) {
    nb_write_string(ansi_system_console_text_colors[color], to_standard_error);
}

#include <stdio.h>

char *mprint(const char *fmt, ...) {
    char *result = null;
    int size = NB_PRINT_INITIAL_GUESS;

    while (1) {
        result = nb_new_array(char, size);
        if (!result) return null;
        
        va_list args;
        va_start(args, fmt);
        
        int len = vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            size = len;
            break;
        }

        nb_free(result);
        size *= 2;
    }

    return result;
}

char *mprint_guess(int size, const char *fmt, ...) {
    assert(size > 0);
    
    char *result = null;

    while (1) {
        result = nb_new_array(char, size);
        if (!result) return null;
        
        va_list args;
        va_start(args, fmt);
        
        int len = vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            size = len;
            break;
        }

        nb_free(result);
        size *= 2;
    }

    return result;
}

NB_EXTERN char *mprint_valist(const char *fmt, va_list arg_list) {
    char *result = null;
    int size = NB_PRINT_INITIAL_GUESS;

    while (1) {
        result = nb_new_array(char, size);
        if (!result) return null;
        
        va_list args;
        // args = arg_list;
        va_copy(args, arg_list);  // @Cleanup
        
        int len = vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            size = len;
            break;
        }

        nb_free(result);
        size *= 2;
    }

    return result;
}

NB_EXTERN char *tprint(const char *fmt, ...) {
    char *result = null;

    // Initial guess.
    int size = NB_PRINT_INITIAL_GUESS;
    NB_Allocator allocator = nb_temporary_allocator;
    NB_Temporary_Storage *ts = &nb_temporary_storage;

    while (1) {
        s64 mark = nb_get_temporary_storage_mark();
        result = nb_new_array(char, size, allocator);
        if (!result) return null;
        
        va_list args;
        va_start(args, fmt);
        
        int len = vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            ts->occupied -= (size - len - 1);
            size = len;
            break;
        }

        nb_set_temporary_storage_mark(mark);
        size *= 2;
    }

    return result;
}

NB_EXTERN char *tprint_valist(const char *fmt, va_list arg_list) {
    char *result = null;
    int size = NB_PRINT_INITIAL_GUESS;
    NB_Allocator allocator = nb_temporary_allocator;
    NB_Temporary_Storage *ts = &nb_temporary_storage;

    while (1) {
        s64 mark = nb_get_temporary_storage_mark();
        result = nb_new_array(char, size, allocator);
        if (!result) return null;

        va_list args;
        // args = arg_list;
        va_copy(args, arg_list);  // @Cleanup
        
        int len = vsnprintf(result, size, fmt, args);
        va_end(args);

        if ((len >= 0) && (size >= len+1)) {
            ts->occupied -= (size - len - 1);
            size = len;
            break;
        }

        nb_set_temporary_storage_mark(mark);
        size *= 2;
    }

    return result;
}

NB_EXTERN void print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char *s = tprint_valist(fmt, args);
    va_end(args);

    nb_write_string(s);
}

NB_EXTERN bool 
nb_abort_error_message(const char *title, 
                       const char *message, 
                       const char *details) {
    UNUSED(title);
    UNUSED(message);
    UNUSED(details);

    fflush(stderr);
    return true;
}

#if NB_ENABLE_ASSERTS

#include <execinfo.h>
#include <inttypes.h>

NB_EXTERN char *nb_get_stacktrace(void) {
    char *result = null;

    // @Note: to print the stack, compile with -rdynamic.
    const int MAX_STACK_FRAMES = 64;
    void *stack[MAX_STACK_FRAMES];

    int frames = backtrace(stack, MAX_STACK_FRAMES);
    if (frames > 0) {
        print("Caller stack:\n");

        char **symbols = backtrace_symbols(stack, frames);
        if (symbols) {
            for (int index = 0; index < frames; index++) {
                void *stack_address = stack[index];

                // @Todo: line numbers...
                s64 stack_line = 0;
                s64 call_line  = 0;

                // @Todo: Actually allocate and append to the result!!

#if COMPILER_GCC
                print("0x%016zu: %s(%ld) Line %ld\n", 
                      (size_t)stack_address, 
                      symbols[index], 
                      stack_line, call_line);
#else
                print("0x%016" PRIXPTR ": %s(%" PRId64 ") Line %" PRId64 "\n", 
                      (size_t)stack_address, 
                      symbols[index], 
                      stack_line, call_line);
#endif
            }

            free(symbols);
        }
    }

    return result;
}

#endif  // NB_ENABLE_ASSERTS

NB_EXTERN void *
nb_heap_allocator(NB_Allocator_Mode mode, 
                  s64 size, s64 old_size, 
                  void *old_memory, 
                  void *allocator_data) {
    UNUSED(allocator_data);

    switch (mode) {
        case NB_ALLOCATOR_ALLOCATE:
            // @Todo: mmap?
            return malloc((umm)size);

        case NB_ALLOCATOR_RESIZE: {
            // Allocate, copy, free.

            void *result = malloc((umm)size);
            if (result == null) return null;

            if (old_memory && (old_size > 0)) {
                memcpy(result, old_memory, (umm)old_size);
                free(old_memory);
            }

            return result;
        } break;

        case NB_ALLOCATOR_FREE: {
            free(old_memory);
            return null;
        } break;

        case NB_ALLOCATOR_FREE_ALL: {
            // Not supported.
            assert(!"Not supported");
            return null;
        } break;

        default: {
            assert(false);
            return null;
        } break;
    }
    return null;
}

#endif  // OS_LINUX



NB_EXTERN NB_ALLOCATOR_PROC(nb_temporary_storage_proc) {
    UNUSED(allocator_data);

    NB_Temporary_Storage *ts = &nb_temporary_storage;

    if (!ts->allocator.proc) {
        ts->allocator.proc = nb_heap_allocator;
        ts->allocator.data = null;
    }

    s64 nbytes = size;
    s64 alignment = 8;

    s64 extra = (alignment - (nbytes % alignment)) % alignment;
    nbytes += extra;

    switch (mode) {
        case NB_ALLOCATOR_ALLOCATE: {
            if (!ts->data) {
                if (!ts->size) ts->size = NB_TS_SIZE_DEFAULT;

                ts->data = (u8 *)ts->allocator.proc(NB_ALLOCATOR_ALLOCATE, 
                                                    ts->size, 0, 
                                                    null, 
                                                    ts->allocator.data);
                if (!ts->data) return null;
            }

#if NB_DEBUG
            if (nbytes > (ts->size - ts->occupied)) {
                ts->high_water_mark += nbytes;

#if OS_WINDOWS && COMPILER_GCC
                Log(NB_LOG_WARNING, 
                    "Temporary_Storage", "Attempting to allocate from the heap, highest water mark: %I64d\n", 
                    ts->high_water_mark);
#else
                Log(NB_LOG_WARNING, 
                    "Temporary_Storage", 
                    "Attempting to allocate from the heap, highest water mark: %" PRId64 "\n", 
                    ts->high_water_mark);
#endif
                return nb_heap_allocator(NB_ALLOCATOR_ALLOCATE, nbytes, 0, null, null);
            }
#else
            // @Cleanup:
            assert(nbytes <= (ts->size - ts->occupied));
#endif

            void *result = ts->data + ts->occupied;
            ts->occupied += nbytes;
            return result;
        } break;

        case NB_ALLOCATOR_RESIZE: {
            // Bump allocators do not resize so we allocate
            // a new chunk of memory using the new size.


            // @Cutnpaste: from ALLOCATOR_ALLOCATE.
#if NB_DEBUG
            if (nbytes > (ts->size - ts->occupied)) {
                ts->high_water_mark += nbytes;
#if OS_WINDOWS && COMPILER_GCC
                Log(NB_LOG_WARNING, 
                    "Temporary_Storage", 
                    "Attempting to allocate from the heap, highest water mark: %I64d\n", 
                    ts->high_water_mark);
#else
                Log(NB_LOG_WARNING, 
                    "Temporary_Storage", 
                    "Attempting to allocate from the heap, highest water mark: %" PRId64 "\n", 
                    ts->high_water_mark);
#endif
                return nb_heap_allocator(NB_ALLOCATOR_ALLOCATE, nbytes, 0, null, null);
            }
#else
            // @Cleanup:
            assert(nbytes <= (ts->size - ts->occupied));
#endif

            void *result = ts->data + ts->occupied;
            ts->occupied += nbytes;

            if (old_memory && (old_size > 0)) {
                memcpy(result, old_memory, (umm)Min(old_size, nbytes));
            }

            return result;
        } break;

        case NB_ALLOCATOR_FREE:
            // Not supported.
            assert(false);
            return null;

        case NB_ALLOCATOR_FREE_ALL: {
            ts->allocator.proc(NB_ALLOCATOR_FREE, 0, 0, ts->data, ts->allocator.data);
            ts->data = null;
            ts->size = 0;
            ts->occupied = 0;
            ts->high_water_mark = 0;
            return null;
        } break;

        default:
            assert(false);
            return null;
    }
}



static s64 
nb_get_partition_index_for_qsort(u8 *data, 
                                 s64 low, s64 high, 
                                 s64 stride, 
                                 s64 (*qsort_compare)(void *, void *)) {
    u8 *pivot_address = data + high * stride;

    u8 *start = data + low  * stride;
    u8 *end   = data + high * stride;

    s64 i = low - 1;

    for (u8 *it = start; it < end; it += stride) {
        if (qsort_compare(it, pivot_address) < 0) {
            i += 1;
            nb_swap_two_memory_blocks(data + i * stride, it, stride);
        }
    }

    nb_swap_two_memory_blocks(data + (i+1) * stride, pivot_address, stride);
    return i + 1;
}

void nb_qsort(void *data, s64 count, 
              s64 stride, 
              s64 (*qsort_compare)(void *, void *)) {
    if (count < 2) return;

    u8 *start = (u8 *)data;
    u8 *pivot_address = start + (count / 2) * stride;

    s64 i = 0;
    s64 j = count-1;

    while (1) {
        while (qsort_compare(start + i*stride, pivot_address) < 0) { i += 1; }
        while (qsort_compare(pivot_address, start + j*stride) < 0) { j -= 1; }

        if (i >= j) break;

        nb_swap_two_memory_blocks(start + i*stride, start + j*stride, stride);

        i += 1;
        j -= 1;
    }

    nb_qsort(start, i, stride, qsort_compare);
    nb_qsort(start + i*stride, count-i, stride, qsort_compare);
}

void nb_qsort_it(void *data, s64 count, 
                 s64 stride, 
                 s64 (*qsort_compare)(void *, void *)) {
    if (count < 2) return;

    s64 *qsort_stack = nb_new_array(s64, count * 2, nb_temporary_allocator);

    // Push.
    qsort_stack[0] = 0;
    qsort_stack[1] = count-1;

    int top = 1;
    while (top >= 0) {
        // Pop.
        s64 high = qsort_stack[top--];
        s64 low  = qsort_stack[top--];

        s64 pivot_index = nb_get_partition_index_for_qsort((u8 *)data, low, high, stride, qsort_compare);

        // Insert array intervals using the pivot index.

        if ((pivot_index - 1) > low) {
            qsort_stack[++top] = low;
            qsort_stack[++top] = pivot_index - 1;
        }

        if ((pivot_index + 1) < high) {
            qsort_stack[++top] = pivot_index + 1;
            qsort_stack[++top] = high;
        }
    }
}

#endif  // NB_IMPLEMENTATION
