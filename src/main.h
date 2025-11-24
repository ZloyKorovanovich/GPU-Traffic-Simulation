#ifndef _MAIN_INCLUDED
#define _MAIN_INCLUDED

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#else
#include <alloca.h>
#endif

// =========================================================== TYPES
// =================================================================

typedef long long i64;
typedef long i64x;
typedef int i32;
typedef short i16;
typedef char i8;

typedef unsigned long long u64;
typedef unsigned long u32x;
typedef unsigned u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef float f32;
typedef double f64;

typedef int b32;
#define FALSE 0
#define TRUE 1

#define I64_MAX 9223372036854775807
#define I32_MAX 2147483647
#define U32_MAX 0xffffffff
#define U64_MAX 0xffffffffffffffff

#define I64_MIN (-9223372036854775807 - 1)
#define I32_MIN (-2147483647 - 1)
#define U32_MIN 0
#define U64_MIN 0


typedef u32 Handle;
#define HANDLE_INVALID 0xffffffff

// ======================================================= BASIC OPS
// =================================================================

#define MIN(a, b) ((a < b) ? a : b)
#define MAX(a, b) ((a > b) ? a : b)

#define CLAMP(a, b, t) MAX(MIN(t, b), a)

#define FLAG_IN_MASK(mask, flag) ((flag & mask) == flag)
#define FLAG_NOT_IN_MASK(mask, flag) !(flag & mask)

#define BIT(bit) (1 << bit)

// ======================================================= FUNCTIONS
// =================================================================

#define DYN_ARG u32 argc, u64* argp
#define ARG(n) argp[n]

#define LOOP while(1)

// ========================================================= PACKING
// =================================================================

#define PACK_32_TO_64(a, b) (((u64)(a) << 32) | (u64)(b))
#define UNPACK_64_TO_32_A(a) ((u32)((a) >> 32)) 
#define UNPACK_64_TO_32_B(b) ((u32)((b)))

// =========================================================== CODES
// =================================================================

// IN MEMORY:
// +----------------+----------------+--------------------------------+
// |  MODULE 8bit   | RESERVED 8bit  |           CODE 16bit           |
// +----------------+----------------+--------------------------------+
// IN HEX NUMBER: MMRRCCCC

typedef u32 result;
typedef b32 (*EventCallback) (result event_code);

#define CODE_SUCCESS 0

#define CODE_MASK_MODULE        0xff000000
#define CODE_MASK_RESERVED      0x00ff0000
#define CODE_MASK_CODE          0x0000ffff
#define CODE_MASK_EMPTY         0x00000000

#define CODE_SHIFT_MODULE       24
#define CODE_SHIFT_RESERVED     16
#define CODE_SHIFT_CODE         0

#define CODE_UNPACK_MODULE(code) (code >> CODE_SHIFT_MODULE)
#define CODE_UNPACK_RESERVED(code) ((code & CODE_MASK_RESERVED) >> CODE_SHIFT_RESERVED)
#define CODE_UNPACK_CODE(code) (code & CODE_MASK_CODE)

#define CODE_PACK_MODULE(mask, code) (mask | (code << CODE_SHIFT_MODULE))
#define CODE_PACK_RESERVED(mask, code) (mask | (code << CODE_SHIFT_RESERVED))
#define CODE_PACK_CODE(mask, code) (mask | code)

#define DEBUG_TRACE printf("*** TRACE %s:%u %s\n", __FILE__, __LINE__, __func__);

// ========================================================= ATTRIBS
// =================================================================

#define ATTRIB_PACKED __attribute__((__packed__))
#define EXPECT(exp, val) __builtin_expect(exp, val)
#define EXPECT_T(exp) EXPECT(exp, 1)
#define EXPECT_F(exp) EXPECT(exp, 0)

#define ERROR_CATCH(exp) if(EXPECT_F(exp))

#define ALIGN(ptr, aligment) (ptr + (aligment - ptr % aligment) * (ptr % aligment != 0))

// ========================================================= MODULES
// =================================================================

// 8 bits for this enum values
typedef enum {
    CODE_MODULE_MAIN = 0,
    CODE_MODULE_VULKAN,
} ModuleCodes;

// names id should match ModuleCodes value
#define MODULE_NAMES {"MAIN", "VULKAN"}

// ====================================================== ALLOCATION
// ================================================================= 

#define MIN_STACK 16
#define ZE_ALLOCA(bytes) alloca((MIN_STACK > bytes) ? MIN_STACK : bytes)

#define SET_MEMORY(ptr, value, count, i)                \
for(u32 i = 0; i < count; i++) {                        \
    ptr[i] = value;                                     \
}

#define SAFE_DESTROY(ptr, func) if(ptr) {func; ptr = 0;}

// ========================================================= THREADS
// ================================================================= 

typedef enum {
    COMMAND_CODE_NONE = 0,
    COMMAND_CODE_EXIT,
} CommandCodes;
#define COMMAND_COUNT 2
#define COMMAND_NAMES   {       \
    "none",                     \
    "exit"                      \
}

typedef struct {
    atomic_uint thread_lock;
    u32 command;
    void* data;
} ThreadCommandBuffer;


#endif
