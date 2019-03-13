#include <math.h>
#include <string.h>

#include "decompiled.h"
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x) __builtin_expect(!!(x), 1)

#define TRAP(x) (wasm_rt_trap(WASM_RT_TRAP_##x), 0)

#define FUNC_PROLOGUE                                            \
  if (++wasm_rt_call_stack_depth > WASM_RT_MAX_CALL_STACK_DEPTH) \
    TRAP(EXHAUSTION)

#define FUNC_EPILOGUE --wasm_rt_call_stack_depth

#define UNREACHABLE TRAP(UNREACHABLE)

#define CALL_INDIRECT(table, t, ft, x, ...)          \
  (LIKELY((x) < table.size && table.data[x].func &&  \
          table.data[x].func_type == func_types[ft]) \
       ? ((t)table.data[x].func)(__VA_ARGS__)        \
       : TRAP(CALL_INDIRECT))

#define MEMCHECK(mem, a, t)  \
  if (UNLIKELY((a) + sizeof(t) > mem->size)) TRAP(OOB)

#define DEFINE_LOAD(name, t1, t2, t3)              \
  static inline t3 name(wasm_rt_memory_t* mem, u64 addr) {   \
    MEMCHECK(mem, addr, t1);                       \
    t1 result;                                     \
    memcpy(&result, &mem->data[addr], sizeof(t1)); \
    return (t3)(t2)result;                         \
  }

#define DEFINE_STORE(name, t1, t2)                           \
  static inline void name(wasm_rt_memory_t* mem, u64 addr, t2 value) { \
    MEMCHECK(mem, addr, t1);                                 \
    t1 wrapped = (t1)value;                                  \
    memcpy(&mem->data[addr], &wrapped, sizeof(t1));          \
  }

DEFINE_LOAD(i32_load, u32, u32, u32);
DEFINE_LOAD(i64_load, u64, u64, u64);
DEFINE_LOAD(f32_load, f32, f32, f32);
DEFINE_LOAD(f64_load, f64, f64, f64);
DEFINE_LOAD(i32_load8_s, s8, s32, u32);
DEFINE_LOAD(i64_load8_s, s8, s64, u64);
DEFINE_LOAD(i32_load8_u, u8, u32, u32);
DEFINE_LOAD(i64_load8_u, u8, u64, u64);
DEFINE_LOAD(i32_load16_s, s16, s32, u32);
DEFINE_LOAD(i64_load16_s, s16, s64, u64);
DEFINE_LOAD(i32_load16_u, u16, u32, u32);
DEFINE_LOAD(i64_load16_u, u16, u64, u64);
DEFINE_LOAD(i64_load32_s, s32, s64, u64);
DEFINE_LOAD(i64_load32_u, u32, u64, u64);
DEFINE_STORE(i32_store, u32, u32);
DEFINE_STORE(i64_store, u64, u64);
DEFINE_STORE(f32_store, f32, f32);
DEFINE_STORE(f64_store, f64, f64);
DEFINE_STORE(i32_store8, u8, u32);
DEFINE_STORE(i32_store16, u16, u32);
DEFINE_STORE(i64_store8, u8, u64);
DEFINE_STORE(i64_store16, u16, u64);
DEFINE_STORE(i64_store32, u32, u64);

#define I32_CLZ(x) ((x) ? __builtin_clz(x) : 32)
#define I64_CLZ(x) ((x) ? __builtin_clzll(x) : 64)
#define I32_CTZ(x) ((x) ? __builtin_ctz(x) : 32)
#define I64_CTZ(x) ((x) ? __builtin_ctzll(x) : 64)
#define I32_POPCNT(x) (__builtin_popcount(x))
#define I64_POPCNT(x) (__builtin_popcountll(x))

#define DIV_S(ut, min, x, y)                                 \
   ((UNLIKELY((y) == 0)) ?                TRAP(DIV_BY_ZERO)  \
  : (UNLIKELY((x) == min && (y) == -1)) ? TRAP(INT_OVERFLOW) \
  : (ut)((x) / (y)))

#define REM_S(ut, min, x, y)                                \
   ((UNLIKELY((y) == 0)) ?                TRAP(DIV_BY_ZERO) \
  : (UNLIKELY((x) == min && (y) == -1)) ? 0                 \
  : (ut)((x) % (y)))

#define I32_DIV_S(x, y) DIV_S(u32, INT32_MIN, (s32)x, (s32)y)
#define I64_DIV_S(x, y) DIV_S(u64, INT64_MIN, (s64)x, (s64)y)
#define I32_REM_S(x, y) REM_S(u32, INT32_MIN, (s32)x, (s32)y)
#define I64_REM_S(x, y) REM_S(u64, INT64_MIN, (s64)x, (s64)y)

#define DIVREM_U(op, x, y) \
  ((UNLIKELY((y) == 0)) ? TRAP(DIV_BY_ZERO) : ((x) op (y)))

#define DIV_U(x, y) DIVREM_U(/, x, y)
#define REM_U(x, y) DIVREM_U(%, x, y)

#define ROTL(x, y, mask) \
  (((x) << ((y) & (mask))) | ((x) >> (((mask) - (y) + 1) & (mask))))
#define ROTR(x, y, mask) \
  (((x) >> ((y) & (mask))) | ((x) << (((mask) - (y) + 1) & (mask))))

#define I32_ROTL(x, y) ROTL(x, y, 31)
#define I64_ROTL(x, y) ROTL(x, y, 63)
#define I32_ROTR(x, y) ROTR(x, y, 31)
#define I64_ROTR(x, y) ROTR(x, y, 63)

#define FMIN(x, y)                                          \
   ((UNLIKELY((x) != (x))) ? NAN                            \
  : (UNLIKELY((y) != (y))) ? NAN                            \
  : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? x : y) \
  : (x < y) ? x : y)

#define FMAX(x, y)                                          \
   ((UNLIKELY((x) != (x))) ? NAN                            \
  : (UNLIKELY((y) != (y))) ? NAN                            \
  : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? y : x) \
  : (x > y) ? x : y)

#define TRUNC_S(ut, st, ft, min, max, maxop, x)                             \
   ((UNLIKELY((x) != (x))) ? TRAP(INVALID_CONVERSION)                       \
  : (UNLIKELY((x) < (ft)(min) || (x) maxop (ft)(max))) ? TRAP(INT_OVERFLOW) \
  : (ut)(st)(x))

#define I32_TRUNC_S_F32(x) TRUNC_S(u32, s32, f32, INT32_MIN, INT32_MAX, >=, x)
#define I64_TRUNC_S_F32(x) TRUNC_S(u64, s64, f32, INT64_MIN, INT64_MAX, >=, x)
#define I32_TRUNC_S_F64(x) TRUNC_S(u32, s32, f64, INT32_MIN, INT32_MAX, >,  x)
#define I64_TRUNC_S_F64(x) TRUNC_S(u64, s64, f64, INT64_MIN, INT64_MAX, >=, x)

#define TRUNC_U(ut, ft, max, maxop, x)                                    \
   ((UNLIKELY((x) != (x))) ? TRAP(INVALID_CONVERSION)                     \
  : (UNLIKELY((x) <= (ft)-1 || (x) maxop (ft)(max))) ? TRAP(INT_OVERFLOW) \
  : (ut)(x))

#define I32_TRUNC_U_F32(x) TRUNC_U(u32, f32, UINT32_MAX, >=, x)
#define I64_TRUNC_U_F32(x) TRUNC_U(u64, f32, UINT64_MAX, >=, x)
#define I32_TRUNC_U_F64(x) TRUNC_U(u32, f64, UINT32_MAX, >,  x)
#define I64_TRUNC_U_F64(x) TRUNC_U(u64, f64, UINT64_MAX, >=, x)

#define DEFINE_REINTERPRET(name, t1, t2)  \
  static inline t2 name(t1 x) {           \
    t2 result;                            \
    memcpy(&result, &x, sizeof(result));  \
    return result;                        \
  }

DEFINE_REINTERPRET(f32_reinterpret_i32, u32, f32)
DEFINE_REINTERPRET(i32_reinterpret_f32, f32, u32)
DEFINE_REINTERPRET(f64_reinterpret_i64, u64, f64)
DEFINE_REINTERPRET(i64_reinterpret_f64, f64, u64)


static u32 func_types[26];

static void init_func_types(void) {
  func_types[0] = wasm_rt_register_func_type(0, 0);
  func_types[1] = wasm_rt_register_func_type(3, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[2] = wasm_rt_register_func_type(2, 0, WASM_RT_I32, WASM_RT_I32);
  func_types[3] = wasm_rt_register_func_type(0, 1, WASM_RT_I32);
  func_types[4] = wasm_rt_register_func_type(2, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[5] = wasm_rt_register_func_type(1, 0, WASM_RT_I32);
  func_types[6] = wasm_rt_register_func_type(1, 0, WASM_RT_I64);
  func_types[7] = wasm_rt_register_func_type(0, 1, WASM_RT_I64);
  func_types[8] = wasm_rt_register_func_type(4, 1, WASM_RT_I64, WASM_RT_I64, WASM_RT_I64, WASM_RT_I64, WASM_RT_I32);
  func_types[9] = wasm_rt_register_func_type(5, 0, WASM_RT_I32, WASM_RT_I64, WASM_RT_I64, WASM_RT_I64, WASM_RT_I64);
  func_types[10] = wasm_rt_register_func_type(2, 1, WASM_RT_I64, WASM_RT_I64, WASM_RT_I32);
  func_types[11] = wasm_rt_register_func_type(2, 0, WASM_RT_I32, WASM_RT_F64);
  func_types[12] = wasm_rt_register_func_type(2, 0, WASM_RT_I32, WASM_RT_F32);
  func_types[13] = wasm_rt_register_func_type(2, 1, WASM_RT_I64, WASM_RT_I64, WASM_RT_F64);
  func_types[14] = wasm_rt_register_func_type(2, 1, WASM_RT_I64, WASM_RT_I64, WASM_RT_F32);
  func_types[15] = wasm_rt_register_func_type(3, 0, WASM_RT_I64, WASM_RT_I64, WASM_RT_I64);
  func_types[16] = wasm_rt_register_func_type(5, 0, WASM_RT_I32, WASM_RT_I64, WASM_RT_I64, WASM_RT_I32, WASM_RT_I32);
  func_types[17] = wasm_rt_register_func_type(4, 0, WASM_RT_I32, WASM_RT_I32, WASM_RT_I64, WASM_RT_I32);
  func_types[18] = wasm_rt_register_func_type(3, 0, WASM_RT_I32, WASM_RT_I64, WASM_RT_I32);
  func_types[19] = wasm_rt_register_func_type(1, 1, WASM_RT_I32, WASM_RT_I32);
  func_types[20] = wasm_rt_register_func_type(5, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[21] = wasm_rt_register_func_type(8, 0, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[22] = wasm_rt_register_func_type(3, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I64);
  func_types[23] = wasm_rt_register_func_type(3, 0, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[24] = wasm_rt_register_func_type(2, 0, WASM_RT_I32, WASM_RT_I64);
  func_types[25] = wasm_rt_register_func_type(4, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I64, WASM_RT_I64);
}

static void f34(void);
static void apply(u64, u64, u64);
static u32 f36(u32, u32);
static void f37(u32);
static void f38(u32, u64, u64, u32, u32);
static u32 f39(u32, u32);
static void f40(u32, u32, u64, u32);
static void f41(u32, u64, u32);
static void f42(u32, u32);
static void f43(u32, u32);
static u32 f44(u32, u32);
static u32 f45(u32, u32);
static u32 f46(u32, u32);
static u32 f47(u32, u32);
static u32 f48(u32, u32);
static u32 _Znwj(u32);
static u32 _Znaj(u32);
static void _ZdlPv(u32);
static void _ZdaPv(u32);
static u32 _ZnwjSt11align_val_t(u32, u32);
static u32 _ZnajSt11align_val_t(u32, u32);
static void _ZdlPvSt11align_val_t(u32, u32);
static void _ZdaPvSt11align_val_t(u32, u32);
static void f57(u32);
static u32 f58(u32, u32);
static u32 f59(u32, u32, u32, u32, u32);
static void f60(u32, u32, u32, u32, u32, u32, u32, u32);
static void f61(u32, u32);
static void f62(u32);
static void f63(u32);
static u64 f64_0(u32, u32, u32);
static void f65(u32, u32, u32);
static void f66(void);
static void f67(void);
static void f68(u32);
static u32 f69(void);
static void f70(u32);
static u32 f71(u32);
static u32 f72(u32);
static void f73(u32, u64);
static u32 f74(u32);
static u64 f75(u32, u32, u32, u64);
static u64 f76(u32, u32, u32);
static u32 f77(u32, u32, u32);
static u32 f78(u32, u32, u32);
static u32 f79(u32);
static u32 f80(u32, u32, u32);
static u32 f81(u32, u32);
static u32 f82(u32);
static u32 f83(u32, u32);
static u32 f84(u32);
static void f85(u32);

static u32 g0;
static u32 __heap_base;
static u32 __data_end;

static void init_globals(void) {
  g0 = 8192u;
  __heap_base = 17425u;
  __data_end = 17425u;
}

static wasm_rt_memory_t memory;

static wasm_rt_table_t T0;

static void f34(void) {
  FUNC_PROLOGUE;
  FUNC_EPILOGUE;
}

static void apply(u64 p0, u64 p1, u64 p2) {
  u32 l3 = 0, l4 = 0, l5 = 0, l6 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  u64 j0, j1, j2;
  i0 = g0;
  i1 = 160u;
  i0 -= i1;
  l3 = i0;
  g0 = i0;
  f34();
  i0 = l3;
  i1 = 8192u;
  i32_store((&memory), (u64)(i0 + 152), i1);
  i0 = l3;
  i1 = 8192u;
  i1 = f79(i1);
  i32_store((&memory), (u64)(i0 + 156), i1);
  i0 = l3;
  i1 = l3;
  j1 = i64_load((&memory), (u64)(i1 + 152));
  i64_store((&memory), (u64)(i0 + 24), j1);
  i0 = l3;
  i1 = 64u;
  i0 += i1;
  i1 = l3;
  i2 = 24u;
  i1 += i2;
  i0 = f36(i0, i1);
  j0 = p1;
  j1 = 6138663591592764928ull;
  i0 = j0 != j1;
  if (i0) {goto B0;}
  i0 = l3;
  i1 = 8213u;
  i32_store((&memory), (u64)(i0 + 144), i1);
  i0 = l3;
  i1 = 8213u;
  i1 = f79(i1);
  i32_store((&memory), (u64)(i0 + 148), i1);
  i0 = l3;
  i1 = l3;
  j1 = i64_load((&memory), (u64)(i1 + 144));
  i64_store((&memory), (u64)(i0 + 16), j1);
  i0 = l3;
  i1 = 112u;
  i0 += i1;
  i1 = l3;
  i2 = 16u;
  i1 += i2;
  i0 = f36(i0, i1);
  j0 = p2;
  j1 = 14829575313431724032ull;
  i0 = j0 != j1;
  if (i0) {goto B0;}
  i0 = l3;
  i1 = 112u;
  i0 += i1;
  i1 = 24u;
  i0 += i1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  j1 = 6138663591592764928ull;
  i64_store((&memory), (u64)(i0 + 120), j1);
  i0 = l3;
  j1 = p0;
  i64_store((&memory), (u64)(i0 + 112), j1);
  i0 = l3;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 128), j1);
  i0 = l3;
  i1 = 64u;
  i0 += i1;
  f37(i0);
  i0 = l3;
  i1 = 48u;
  i0 += i1;
  i1 = 8u;
  i0 += i1;
  l4 = i0;
  i1 = l3;
  i2 = 64u;
  i1 += i2;
  i2 = 24u;
  i1 += i2;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0), j1);
  i0 = l3;
  i1 = l3;
  j1 = i64_load((&memory), (u64)(i1 + 80));
  i64_store((&memory), (u64)(i0 + 48), j1);
  i0 = l3;
  j0 = i64_load((&memory), (u64)(i0 + 72));
  p1 = j0;
  i0 = l3;
  j0 = i64_load((&memory), (u64)(i0 + 64));
  p2 = j0;
  i0 = l3;
  i1 = 32u;
  i0 += i1;
  i1 = l3;
  i2 = 96u;
  i1 += i2;
  l5 = i1;
  i0 = f58(i0, i1);
  l6 = i0;
  i0 = l3;
  i1 = 8u;
  i0 += i1;
  i1 = l4;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0), j1);
  i0 = l3;
  i1 = l3;
  j1 = i64_load((&memory), (u64)(i1 + 48));
  i64_store((&memory), (u64)(i0), j1);
  i0 = l3;
  i1 = 112u;
  i0 += i1;
  j1 = p2;
  j2 = p1;
  i3 = l3;
  i4 = l6;
  f38(i0, j1, j2, i3, i4);
  i0 = l6;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B1;}
  i0 = l6;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  _ZdlPv(i0);
  B1:;
  i0 = l5;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B0;}
  i0 = l3;
  i1 = 104u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  _ZdlPv(i0);
  B0:;
  i0 = 0u;
  f70(i0);
  i0 = l3;
  i1 = 160u;
  i0 += i1;
  g0 = i0;
  FUNC_EPILOGUE;
}

static u32 f36(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l5 = 0, l6 = 0, l7 = 0;
  u64 l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j0, j1, j2, j3;
  i0 = p0;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0), j1);
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l2 = i0;
  i1 = 14u;
  i0 = i0 < i1;
  if (i0) {goto B4;}
  i0 = 0u;
  i1 = 8308u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = 12u;
  l3 = i0;
  goto B3;
  B4:;
  i0 = l2;
  i0 = !(i0);
  if (i0) {goto B0;}
  i0 = l2;
  i1 = 12u;
  i2 = l2;
  i3 = 12u;
  i2 = i2 < i3;
  i0 = i2 ? i0 : i1;
  l3 = i0;
  i0 = !(i0);
  if (i0) {goto B2;}
  B3:;
  i0 = p0;
  j0 = i64_load((&memory), (u64)(i0));
  l4 = j0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  l5 = i0;
  i0 = 0u;
  l6 = i0;
  L5: 
    i0 = p0;
    j1 = l4;
    j2 = 5ull;
    j1 <<= (j2 & 63);
    l4 = j1;
    i64_store((&memory), (u64)(i0), j1);
    i0 = l5;
    i1 = l6;
    i0 += i1;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l7 = i0;
    i1 = 46u;
    i0 = i0 != i1;
    if (i0) {goto B7;}
    i0 = 0u;
    l7 = i0;
    goto B6;
    B7:;
    i0 = l7;
    i1 = 4294967247u;
    i0 += i1;
    i1 = 255u;
    i0 &= i1;
    i1 = 4u;
    i0 = i0 > i1;
    if (i0) {goto B8;}
    i0 = l7;
    i1 = 4294967248u;
    i0 += i1;
    l7 = i0;
    goto B6;
    B8:;
    i0 = l7;
    i1 = 4294967199u;
    i0 += i1;
    i1 = 255u;
    i0 &= i1;
    i1 = 25u;
    i0 = i0 > i1;
    if (i0) {goto B9;}
    i0 = l7;
    i1 = 4294967205u;
    i0 += i1;
    l7 = i0;
    goto B6;
    B9:;
    i0 = 0u;
    l7 = i0;
    i0 = 0u;
    i1 = 8413u;
    (*Z_envZ_eosio_assertZ_vii)(i0, i1);
    i0 = p0;
    j0 = i64_load((&memory), (u64)(i0));
    l4 = j0;
    B6:;
    i0 = p0;
    j1 = l4;
    i2 = l7;
    j2 = (u64)(i2);
    j3 = 255ull;
    j2 &= j3;
    j1 |= j2;
    l4 = j1;
    i64_store((&memory), (u64)(i0), j1);
    i0 = l6;
    i1 = 1u;
    i0 += i1;
    l6 = i0;
    i1 = l3;
    i0 = i0 < i1;
    if (i0) {goto L5;}
    goto B1;
  B2:;
  i0 = p0;
  j0 = i64_load((&memory), (u64)(i0));
  l4 = j0;
  i0 = 0u;
  l3 = i0;
  B1:;
  i0 = p0;
  j1 = l4;
  i2 = 12u;
  i3 = l3;
  i2 -= i3;
  i3 = 5u;
  i2 *= i3;
  i3 = 4u;
  i2 += i3;
  j2 = (u64)(i2);
  j1 <<= (j2 & 63);
  i64_store((&memory), (u64)(i0), j1);
  i0 = l2;
  i1 = 13u;
  i0 = i0 != i1;
  if (i0) {goto B0;}
  j0 = 0ull;
  l4 = j0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  i0 = i32_load8_u((&memory), (u64)(i0 + 12));
  l6 = i0;
  i1 = 46u;
  i0 = i0 == i1;
  if (i0) {goto B10;}
  i0 = l6;
  i1 = 4294967247u;
  i0 += i1;
  i1 = 255u;
  i0 &= i1;
  i1 = 4u;
  i0 = i0 > i1;
  if (i0) {goto B11;}
  i0 = l6;
  i1 = 4294967248u;
  i0 += i1;
  j0 = (u64)(i0);
  j1 = 255ull;
  j0 &= j1;
  l4 = j0;
  goto B10;
  B11:;
  i0 = l6;
  i1 = 4294967199u;
  i0 += i1;
  i1 = 255u;
  i0 &= i1;
  i1 = 26u;
  i0 = i0 >= i1;
  if (i0) {goto B12;}
  i0 = l6;
  i1 = 4294967205u;
  i0 += i1;
  l6 = i0;
  j0 = (u64)(i0);
  j1 = 255ull;
  j0 &= j1;
  l4 = j0;
  i0 = l6;
  i1 = 255u;
  i0 &= i1;
  i1 = 16u;
  i0 = i0 < i1;
  if (i0) {goto B10;}
  i0 = 0u;
  i1 = 8346u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  goto B10;
  B12:;
  i0 = 0u;
  i1 = 8413u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  B10:;
  i0 = p0;
  i1 = p0;
  j1 = i64_load((&memory), (u64)(i1));
  j2 = l4;
  j1 |= j2;
  i64_store((&memory), (u64)(i0), j1);
  B0:;
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static void f37(u32 p0) {
  u32 l1 = 0, l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  u64 j1;
  i0 = g0;
  i1 = 32u;
  i0 -= i1;
  l1 = i0;
  l2 = i0;
  i0 = l1;
  g0 = i0;
  i0 = (*Z_envZ_action_data_sizeZ_iv)();
  l3 = i0;
  i1 = 513u;
  i0 = i0 < i1;
  if (i0) {goto B1;}
  i0 = l3;
  i0 = f82(i0);
  l1 = i0;
  goto B0;
  B1:;
  i0 = l1;
  i1 = l3;
  i2 = 15u;
  i1 += i2;
  i2 = 4294967280u;
  i1 &= i2;
  i0 -= i1;
  l1 = i0;
  g0 = i0;
  B0:;
  i0 = l1;
  i1 = l3;
  i0 = (*Z_envZ_read_action_dataZ_iii)(i0, i1);
  i0 = p0;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 8), j1);
  i0 = p0;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0), j1);
  i0 = p0;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 16), j1);
  i0 = p0;
  i1 = 24u;
  i0 += i1;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0), j1);
  i0 = p0;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 32), j1);
  i0 = p0;
  i1 = 40u;
  i0 += i1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = l1;
  i32_store((&memory), (u64)(i0 + 12), i1);
  i0 = l2;
  i1 = l1;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = l2;
  i1 = l1;
  i2 = l3;
  i1 += i2;
  i32_store((&memory), (u64)(i0 + 16), i1);
  i0 = l3;
  i1 = 7u;
  i0 = i0 > i1;
  if (i0) {goto B2;}
  i0 = 0u;
  i1 = 8700u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  B2:;
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  l4 = i0;
  i0 = p0;
  i1 = l1;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = l2;
  i1 = l1;
  i2 = 8u;
  i1 += i2;
  l5 = i1;
  i32_store((&memory), (u64)(i0 + 12), i1);
  i0 = l3;
  i1 = 4294967288u;
  i0 &= i1;
  l3 = i0;
  i1 = 8u;
  i0 = i0 != i1;
  if (i0) {goto B3;}
  i0 = 0u;
  i1 = 8700u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  B3:;
  i0 = p0;
  i1 = 16u;
  i0 += i1;
  l6 = i0;
  i0 = l4;
  i1 = l5;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = l2;
  i1 = l1;
  i2 = 16u;
  i1 += i2;
  l4 = i1;
  i32_store((&memory), (u64)(i0 + 12), i1);
  i0 = l3;
  i1 = 16u;
  i0 = i0 != i1;
  if (i0) {goto B4;}
  i0 = 0u;
  i1 = 8700u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  B4:;
  i0 = p0;
  i1 = 32u;
  i0 += i1;
  l5 = i0;
  i0 = l6;
  i1 = l4;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = l2;
  i1 = l1;
  i2 = 24u;
  i1 += i2;
  l4 = i1;
  i32_store((&memory), (u64)(i0 + 12), i1);
  i0 = l2;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 24), j1);
  i0 = l3;
  i1 = 24u;
  i0 = i0 != i1;
  if (i0) {goto B5;}
  i0 = 0u;
  i1 = 8700u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  B5:;
  i0 = l2;
  i1 = 24u;
  i0 += i1;
  i1 = l4;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = p0;
  i1 = 24u;
  i0 += i1;
  i1 = l2;
  j1 = i64_load((&memory), (u64)(i1 + 24));
  i64_store((&memory), (u64)(i0), j1);
  i0 = l2;
  i1 = l1;
  i2 = 32u;
  i1 += i2;
  i32_store((&memory), (u64)(i0 + 12), i1);
  i0 = l2;
  i1 = 8u;
  i0 += i1;
  i1 = l5;
  i0 = f39(i0, i1);
  i0 = l2;
  i1 = 32u;
  i0 += i1;
  g0 = i0;
  FUNC_EPILOGUE;
}

static void f38(u32 p0, u64 p1, u64 p2, u32 p3, u32 p4) {
  u32 l5 = 0, l7 = 0, l8 = 0;
  u64 l6 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j0, j1, j2;
  i0 = g0;
  i1 = 48u;
  i0 -= i1;
  l5 = i0;
  g0 = i0;
  i0 = p0;
  j0 = i64_load((&memory), (u64)(i0));
  l6 = j0;
  j1 = p1;
  i0 = j0 == j1;
  if (i0) {goto B1;}
  j0 = l6;
  j1 = p2;
  i0 = j0 != j1;
  if (i0) {goto B0;}
  i0 = 8492u;
  (*Z_envZ_printsZ_vi)(i0);
  j0 = p1;
  (*Z_envZ_printnZ_vj)(j0);
  i0 = 8500u;
  (*Z_envZ_printsZ_vi)(i0);
  j0 = p2;
  (*Z_envZ_printnZ_vj)(j0);
  i0 = 8507u;
  (*Z_envZ_printsZ_vi)(i0);
  i0 = p3;
  j0 = i64_load((&memory), (u64)(i0));
  (*Z_envZ_printiZ_vj)(j0);
  i0 = 8521u;
  (*Z_envZ_printsZ_vi)(i0);
  i0 = p4;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  i1 = p4;
  i2 = 1u;
  i1 += i2;
  i2 = p4;
  i2 = i32_load8_u((&memory), (u64)(i2));
  l7 = i2;
  i3 = 1u;
  i2 &= i3;
  l8 = i2;
  i0 = i2 ? i0 : i1;
  i1 = p4;
  i1 = i32_load((&memory), (u64)(i1 + 4));
  i2 = l7;
  i3 = 1u;
  i2 >>= (i3 & 31);
  i3 = l8;
  i1 = i3 ? i1 : i2;
  (*Z_envZ_prints_lZ_vii)(i0, i1);
  i0 = l5;
  i1 = 32u;
  i0 += i1;
  i1 = 8u;
  i0 += i1;
  l7 = i0;
  i1 = p3;
  i2 = 8u;
  i1 += i2;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0), j1);
  i0 = l5;
  i1 = p3;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0 + 32), j1);
  i0 = l5;
  i1 = 16u;
  i0 += i1;
  i1 = p4;
  i0 = f58(i0, i1);
  p4 = i0;
  i0 = l5;
  i1 = 8u;
  i0 += i1;
  i1 = l7;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0), j1);
  i0 = l5;
  i1 = l5;
  j1 = i64_load((&memory), (u64)(i1 + 32));
  i64_store((&memory), (u64)(i0), j1);
  i0 = p0;
  i1 = l5;
  j2 = p1;
  i3 = p4;
  f40(i0, i1, j2, i3);
  i0 = p4;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B1;}
  i0 = p4;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  _ZdlPv(i0);
  B1:;
  i0 = l5;
  i1 = 48u;
  i0 += i1;
  g0 = i0;
  goto Bfunc;
  B0:;
  i0 = 8465u;
  (*Z_envZ_printsZ_vi)(i0);
  i0 = l5;
  i1 = 48u;
  i0 += i1;
  g0 = i0;
  Bfunc:;
  FUNC_EPILOGUE;
}

static u32 f39(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  u64 j1;
  i0 = g0;
  i1 = 32u;
  i0 -= i1;
  l2 = i0;
  g0 = i0;
  i0 = l2;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 24), i1);
  i0 = l2;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 16), j1);
  i0 = p0;
  i1 = l2;
  i2 = 16u;
  i1 += i2;
  i0 = f48(i0, i1);
  i0 = l2;
  i0 = i32_load((&memory), (u64)(i0 + 20));
  i1 = l2;
  i1 = i32_load((&memory), (u64)(i1 + 16));
  l3 = i1;
  i0 -= i1;
  l4 = i0;
  i0 = !(i0);
  if (i0) {goto B7;}
  i0 = l2;
  i1 = 8u;
  i0 += i1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0), j1);
  i0 = l4;
  i1 = 4294967280u;
  i0 = i0 >= i1;
  if (i0) {goto B2;}
  i0 = l4;
  i1 = 10u;
  i0 = i0 > i1;
  if (i0) {goto B6;}
  i0 = l2;
  i1 = l4;
  i2 = 1u;
  i1 <<= (i2 & 31);
  i32_store8((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = 1u;
  i0 |= i1;
  l5 = i0;
  goto B5;
  B7:;
  i0 = p1;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B4;}
  i0 = p1;
  i1 = 0u;
  i32_store16((&memory), (u64)(i0), i1);
  i0 = p1;
  i1 = 8u;
  i0 += i1;
  l3 = i0;
  goto B3;
  B6:;
  i0 = l4;
  i1 = 16u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  l6 = i0;
  i0 = _Znwj(i0);
  l5 = i0;
  i0 = l2;
  i1 = l6;
  i2 = 1u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = l5;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = l2;
  i1 = l4;
  i32_store((&memory), (u64)(i0 + 4), i1);
  B5:;
  i0 = l4;
  l7 = i0;
  i0 = l5;
  l6 = i0;
  L8: 
    i0 = l6;
    i1 = l3;
    i1 = i32_load8_u((&memory), (u64)(i1));
    i32_store8((&memory), (u64)(i0), i1);
    i0 = l6;
    i1 = 1u;
    i0 += i1;
    l6 = i0;
    i0 = l3;
    i1 = 1u;
    i0 += i1;
    l3 = i0;
    i0 = l7;
    i1 = 4294967295u;
    i0 += i1;
    l7 = i0;
    if (i0) {goto L8;}
  i0 = l5;
  i1 = l4;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p1;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B10;}
  i0 = p1;
  i1 = 0u;
  i32_store16((&memory), (u64)(i0), i1);
  goto B9;
  B10:;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 4), i1);
  B9:;
  i0 = p1;
  i1 = 0u;
  f61(i0, i1);
  i0 = p1;
  i1 = 8u;
  i0 += i1;
  i1 = l2;
  i2 = 8u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  i32_store((&memory), (u64)(i0), i1);
  i0 = p1;
  i1 = l2;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0), j1);
  i0 = l2;
  i0 = i32_load((&memory), (u64)(i0 + 16));
  l3 = i0;
  i0 = !(i0);
  if (i0) {goto B0;}
  goto B1;
  B4:;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 4), i1);
  i0 = p1;
  i1 = 8u;
  i0 += i1;
  l3 = i0;
  B3:;
  i0 = p1;
  i1 = 0u;
  f61(i0, i1);
  i0 = l3;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p1;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0), j1);
  i0 = l2;
  i0 = i32_load((&memory), (u64)(i0 + 16));
  l3 = i0;
  if (i0) {goto B1;}
  goto B0;
  B2:;
  i0 = l2;
  f57(i0);
  UNREACHABLE;
  B1:;
  i0 = l2;
  i1 = l3;
  i32_store((&memory), (u64)(i0 + 20), i1);
  i0 = l3;
  _ZdlPv(i0);
  B0:;
  i0 = l2;
  i1 = 32u;
  i0 += i1;
  g0 = i0;
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static void f40(u32 p0, u32 p1, u64 p2, u32 p3) {
  u32 l4 = 0, l5 = 0, l6 = 0, l7 = 0, l8 = 0, l9 = 0, l10 = 0, l11 = 0, 
      l12 = 0;
  u64 l13 = 0, l14 = 0, l15 = 0, l16 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  u64 j0, j1, j2, j3;
  i0 = g0;
  i1 = 64u;
  i0 -= i1;
  l4 = i0;
  g0 = i0;
  i0 = l4;
  i1 = 56u;
  i0 += i1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l4;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 48), j1);
  i0 = 8531u;
  i0 = f79(i0);
  l5 = i0;
  i1 = 4294967280u;
  i0 = i0 >= i1;
  if (i0) {goto B0;}
  i0 = l5;
  i1 = 11u;
  i0 = i0 >= i1;
  if (i0) {goto B3;}
  i0 = l4;
  i1 = l5;
  i2 = 1u;
  i1 <<= (i2 & 31);
  i32_store8((&memory), (u64)(i0 + 48), i1);
  i0 = l4;
  i1 = 48u;
  i0 += i1;
  i1 = 1u;
  i0 |= i1;
  l6 = i0;
  i0 = l5;
  if (i0) {goto B2;}
  goto B1;
  B3:;
  i0 = l5;
  i1 = 16u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  l7 = i0;
  i0 = _Znwj(i0);
  l6 = i0;
  i0 = l4;
  i1 = l7;
  i2 = 1u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0 + 48), i1);
  i0 = l4;
  i1 = l6;
  i32_store((&memory), (u64)(i0 + 56), i1);
  i0 = l4;
  i1 = l5;
  i32_store((&memory), (u64)(i0 + 52), i1);
  B2:;
  i0 = l6;
  i1 = 8531u;
  i2 = l5;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  B1:;
  i0 = l6;
  i1 = l5;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p3;
  i0 = i32_load8_u((&memory), (u64)(i0));
  l5 = i0;
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B5;}
  i0 = l5;
  i1 = 1u;
  i0 >>= (i1 & 31);
  l5 = i0;
  i0 = p3;
  i1 = 1u;
  i0 += i1;
  l8 = i0;
  goto B4;
  B5:;
  i0 = p3;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l5 = i0;
  i0 = p3;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l8 = i0;
  B4:;
  i0 = 1u;
  l7 = i0;
  i0 = 0u;
  l9 = i0;
  i0 = l4;
  i0 = i32_load((&memory), (u64)(i0 + 52));
  i1 = l4;
  i1 = i32_load8_u((&memory), (u64)(i1 + 48));
  l6 = i1;
  i2 = 1u;
  i1 >>= (i2 & 31);
  i2 = l6;
  i3 = 1u;
  i2 &= i3;
  l10 = i2;
  i0 = i2 ? i0 : i1;
  l6 = i0;
  i0 = !(i0);
  if (i0) {goto B6;}
  i0 = l8;
  i1 = l5;
  i0 += i1;
  l11 = i0;
  i0 = l5;
  i1 = l6;
  i0 = (u32)((s32)i0 < (s32)i1);
  if (i0) {goto B8;}
  i0 = l4;
  i0 = i32_load((&memory), (u64)(i0 + 56));
  i1 = l4;
  i2 = 48u;
  i1 += i2;
  i2 = 1u;
  i1 |= i2;
  i2 = l10;
  i0 = i2 ? i0 : i1;
  l12 = i0;
  i0 = i32_load8_u((&memory), (u64)(i0));
  l10 = i0;
  i0 = l8;
  l9 = i0;
  L9: 
    i0 = l5;
    i1 = l6;
    i0 -= i1;
    i1 = 1u;
    i0 += i1;
    l5 = i0;
    i0 = !(i0);
    if (i0) {goto B8;}
    i0 = l9;
    i1 = l10;
    i2 = l5;
    i0 = f77(i0, i1, i2);
    l5 = i0;
    i0 = !(i0);
    if (i0) {goto B8;}
    i0 = l5;
    i1 = l12;
    i2 = l6;
    i0 = f78(i0, i1, i2);
    i0 = !(i0);
    if (i0) {goto B7;}
    i0 = l11;
    i1 = l5;
    i2 = 1u;
    i1 += i2;
    l9 = i1;
    i0 -= i1;
    l5 = i0;
    i1 = l6;
    i0 = (u32)((s32)i0 >= (s32)i1);
    if (i0) {goto L9;}
  B8:;
  i0 = l11;
  l5 = i0;
  B7:;
  i0 = 4294967295u;
  i1 = l5;
  i2 = l8;
  i1 -= i2;
  i2 = l5;
  i3 = l11;
  i2 = i2 == i3;
  i0 = i2 ? i0 : i1;
  l9 = i0;
  B6:;
  i0 = l4;
  i1 = 32u;
  i0 += i1;
  i1 = p3;
  i2 = 0u;
  i3 = l9;
  i4 = p3;
  i0 = f59(i0, i1, i2, i3, i4);
  l9 = i0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l11 = i0;
  i0 = l4;
  i0 = i32_load8_u((&memory), (u64)(i0 + 32));
  l5 = i0;
  i0 = l9;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l6 = i0;
  i0 = 8533u;
  i0 = f79(i0);
  p3 = i0;
  i0 = l6;
  i1 = l5;
  i2 = 1u;
  i1 >>= (i2 & 31);
  i2 = l5;
  i3 = 1u;
  i2 &= i3;
  l5 = i2;
  i0 = i2 ? i0 : i1;
  l6 = i0;
  i0 = !(i0);
  if (i0) {goto B10;}
  i0 = l11;
  i1 = l9;
  i2 = 1u;
  i1 += i2;
  i2 = l5;
  i0 = i2 ? i0 : i1;
  l11 = i0;
  l5 = i0;
  i0 = p3;
  i0 = !(i0);
  if (i0) {goto B11;}
  i0 = l11;
  l5 = i0;
  L12: 
    i0 = 8533u;
    i1 = l5;
    i1 = i32_load8_u((&memory), (u64)(i1));
    i2 = p3;
    i0 = f77(i0, i1, i2);
    i0 = !(i0);
    if (i0) {goto B11;}
    i0 = 1u;
    l7 = i0;
    i0 = l5;
    i1 = 1u;
    i0 += i1;
    l5 = i0;
    i0 = l6;
    i1 = 4294967295u;
    i0 += i1;
    l6 = i0;
    if (i0) {goto L12;}
    goto B10;
  B11:;
  i0 = l5;
  i1 = l11;
  i0 -= i1;
  i1 = 4294967295u;
  i0 = i0 == i1;
  l7 = i0;
  B10:;
  i0 = l7;
  i1 = 8544u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = 0u;
  l5 = i0;
  i0 = l9;
  i1 = 0u;
  i2 = 10u;
  j0 = f64_0(i0, i1, i2);
  l13 = j0;
  j1 = 18446744073709551613ull;
  j0 += j1;
  j1 = 94ull;
  i0 = j0 < j1;
  i1 = 8572u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = 8603u;
  (*Z_envZ_printsZ_vi)(i0);
  j0 = l13;
  (*Z_envZ_printuiZ_vj)(j0);
  j0 = (*Z_envZ_current_timeZ_jv)();
  l14 = j0;
  i0 = (*Z_envZ_tapos_block_prefixZ_iv)();
  l6 = i0;
  i0 = 8619u;
  (*Z_envZ_printsZ_vi)(i0);
  i0 = l6;
  j1 = l14;
  j2 = 1000000ull;
  j1 = DIV_U(j1, j2);
  i1 = (u32)(j1);
  i0 += i1;
  i1 = 179424691u;
  i0 *= i1;
  i1 = 100u;
  i0 = REM_U(i0, i1);
  i1 = 1u;
  i0 += i1;
  j0 = (u64)(i0);
  l14 = j0;
  (*Z_envZ_printuiZ_vj)(j0);
  j0 = l13;
  j1 = l14;
  i0 = j0 <= j1;
  if (i0) {goto B17;}
  i0 = p1;
  j0 = i64_load((&memory), (u64)(i0 + 8));
  l15 = j0;
  j1 = 8ull;
  j0 >>= (j1 & 63);
  l14 = j0;
  i0 = p1;
  j0 = i64_load((&memory), (u64)(i0));
  j1 = 990000ull;
  j2 = l13;
  j3 = 18446744073709551615ull;
  j2 += j3;
  j1 = DIV_U(j1, j2);
  j0 *= j1;
  j1 = 10000ull;
  j0 = DIV_U(j0, j1);
  l16 = j0;
  L18: 
    j0 = l14;
    i0 = (u32)(j0);
    i1 = 24u;
    i0 <<= (i1 & 31);
    i1 = 3221225471u;
    i0 += i1;
    i1 = 452984830u;
    i0 = i0 > i1;
    if (i0) {goto B16;}
    j0 = l14;
    j1 = 8ull;
    j0 >>= (j1 & 63);
    l13 = j0;
    j0 = l14;
    j1 = 65280ull;
    j0 &= j1;
    j1 = 0ull;
    i0 = j0 == j1;
    if (i0) {goto B19;}
    j0 = l13;
    l14 = j0;
    i0 = l5;
    l6 = i0;
    i1 = 1u;
    i0 += i1;
    l5 = i0;
    i0 = l6;
    i1 = 6u;
    i0 = (u32)((s32)i0 < (s32)i1);
    if (i0) {goto L18;}
    goto B15;
    B19:;
    j0 = l13;
    l14 = j0;
    L20: 
      j0 = l14;
      j1 = 65280ull;
      j0 &= j1;
      j1 = 0ull;
      i0 = j0 != j1;
      if (i0) {goto B16;}
      j0 = l14;
      j1 = 8ull;
      j0 >>= (j1 & 63);
      l14 = j0;
      i0 = l5;
      i1 = 6u;
      i0 = (u32)((s32)i0 < (s32)i1);
      l6 = i0;
      i0 = l5;
      i1 = 1u;
      i0 += i1;
      l7 = i0;
      l5 = i0;
      i0 = l6;
      if (i0) {goto L20;}
    i0 = l7;
    i1 = 1u;
    i0 += i1;
    l5 = i0;
    i0 = l7;
    i1 = 6u;
    i0 = (u32)((s32)i0 < (s32)i1);
    if (i0) {goto L18;}
    goto B15;
  B17:;
  i0 = 8638u;
  (*Z_envZ_printsZ_vi)(i0);
  i0 = 1u;
  l5 = i0;
  i0 = l4;
  i0 = i32_load8_u((&memory), (u64)(i0 + 32));
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B14;}
  goto B13;
  B16:;
  i0 = 0u;
  i1 = 8645u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  B15:;
  i0 = 8629u;
  (*Z_envZ_printsZ_vi)(i0);
  j0 = l16;
  (*Z_envZ_printiZ_vj)(j0);
  i0 = l4;
  i1 = 8u;
  i0 += i1;
  j1 = l15;
  i64_store((&memory), (u64)(i0), j1);
  i0 = l4;
  j1 = l15;
  i64_store((&memory), (u64)(i0 + 24), j1);
  i0 = l4;
  j1 = l16;
  i64_store((&memory), (u64)(i0), j1);
  i0 = l4;
  j1 = l16;
  i64_store((&memory), (u64)(i0 + 16), j1);
  i0 = p0;
  j1 = p2;
  i2 = l4;
  f41(i0, j1, i2);
  i0 = 1u;
  l5 = i0;
  i0 = l4;
  i0 = i32_load8_u((&memory), (u64)(i0 + 32));
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B13;}
  B14:;
  i0 = l9;
  i1 = 8u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  _ZdlPv(i0);
  B13:;
  i0 = l4;
  i0 = i32_load8_u((&memory), (u64)(i0 + 48));
  i1 = l5;
  i0 &= i1;
  if (i0) {goto B21;}
  i0 = l4;
  i1 = 64u;
  i0 += i1;
  g0 = i0;
  goto Bfunc;
  B21:;
  i0 = l4;
  i1 = 56u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  _ZdlPv(i0);
  i0 = l4;
  i1 = 64u;
  i0 += i1;
  g0 = i0;
  goto Bfunc;
  B0:;
  i0 = l4;
  i1 = 48u;
  i0 += i1;
  f57(i0);
  UNREACHABLE;
  Bfunc:;
  FUNC_EPILOGUE;
}

static void f41(u32 p0, u64 p1, u32 p2) {
  u32 l3 = 0, l6 = 0, l7 = 0, l9 = 0, l10 = 0;
  u64 l4 = 0, l5 = 0, l8 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j0, j1;
  i0 = g0;
  i1 = 224u;
  i0 -= i1;
  l3 = i0;
  g0 = i0;
  i0 = p0;
  j0 = i64_load((&memory), (u64)(i0));
  l4 = j0;
  i0 = l3;
  i1 = 8665u;
  i32_store((&memory), (u64)(i0 + 128), i1);
  i0 = l3;
  i1 = 8665u;
  i1 = f79(i1);
  i32_store((&memory), (u64)(i0 + 132), i1);
  i0 = l3;
  i1 = l3;
  j1 = i64_load((&memory), (u64)(i1 + 128));
  i64_store((&memory), (u64)(i0 + 24), j1);
  i0 = l3;
  i1 = 136u;
  i0 += i1;
  i1 = l3;
  i2 = 24u;
  i1 += i2;
  i0 = f36(i0, i1);
  j0 = i64_load((&memory), (u64)(i0));
  l5 = j0;
  i0 = l3;
  i1 = 8192u;
  i32_store((&memory), (u64)(i0 + 112), i1);
  i0 = l3;
  i1 = 8192u;
  i1 = f79(i1);
  i32_store((&memory), (u64)(i0 + 116), i1);
  i0 = l3;
  i1 = l3;
  j1 = i64_load((&memory), (u64)(i1 + 112));
  i64_store((&memory), (u64)(i0 + 16), j1);
  i0 = l3;
  i1 = 120u;
  i0 += i1;
  i1 = l3;
  i2 = 16u;
  i1 += i2;
  i0 = f36(i0, i1);
  l6 = i0;
  i0 = l3;
  i1 = 8213u;
  i32_store((&memory), (u64)(i0 + 96), i1);
  i0 = l3;
  i1 = 8213u;
  i1 = f79(i1);
  i32_store((&memory), (u64)(i0 + 100), i1);
  i0 = l3;
  i1 = l3;
  j1 = i64_load((&memory), (u64)(i1 + 96));
  i64_store((&memory), (u64)(i0 + 8), j1);
  i0 = l3;
  i1 = 104u;
  i0 += i1;
  i1 = l3;
  i2 = 8u;
  i1 += i2;
  i0 = f36(i0, i1);
  l7 = i0;
  i0 = p0;
  j0 = i64_load((&memory), (u64)(i0));
  l8 = j0;
  i0 = l3;
  i1 = 40u;
  i0 += i1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 32), j1);
  i0 = 8672u;
  i0 = f79(i0);
  p0 = i0;
  i1 = 4294967280u;
  i0 = i0 >= i1;
  if (i0) {goto B1;}
  i0 = p0;
  i1 = 11u;
  i0 = i0 >= i1;
  if (i0) {goto B4;}
  i0 = l3;
  i1 = p0;
  i2 = 1u;
  i1 <<= (i2 & 31);
  i32_store8((&memory), (u64)(i0 + 32), i1);
  i0 = l3;
  i1 = 32u;
  i0 += i1;
  i1 = 1u;
  i0 |= i1;
  l9 = i0;
  i0 = p0;
  if (i0) {goto B3;}
  goto B2;
  B4:;
  i0 = p0;
  i1 = 16u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  l10 = i0;
  i0 = _Znwj(i0);
  l9 = i0;
  i0 = l3;
  i1 = l10;
  i2 = 1u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0 + 32), i1);
  i0 = l3;
  i1 = l9;
  i32_store((&memory), (u64)(i0 + 40), i1);
  i0 = l3;
  i1 = p0;
  i32_store((&memory), (u64)(i0 + 36), i1);
  B3:;
  i0 = l9;
  i1 = 8672u;
  i2 = p0;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  B2:;
  i0 = l9;
  i1 = p0;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = 48u;
  i0 += i1;
  i1 = 24u;
  i0 += i1;
  i1 = p2;
  i2 = 8u;
  i1 += i2;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0), j1);
  i0 = l3;
  i1 = 88u;
  i0 += i1;
  i1 = l3;
  i2 = 32u;
  i1 += i2;
  i2 = 8u;
  i1 += i2;
  p0 = i1;
  i1 = i32_load((&memory), (u64)(i1));
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  j1 = p1;
  i64_store((&memory), (u64)(i0 + 56), j1);
  i0 = l3;
  j1 = l8;
  i64_store((&memory), (u64)(i0 + 48), j1);
  i0 = l3;
  i1 = p2;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0 + 64), j1);
  i0 = l3;
  i1 = l3;
  j1 = i64_load((&memory), (u64)(i1 + 32));
  i64_store((&memory), (u64)(i0 + 80), j1);
  i0 = l3;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 32), j1);
  i0 = l3;
  i1 = l6;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0 + 144), j1);
  i0 = l3;
  i1 = l7;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0 + 152), j1);
  i0 = 16u;
  i0 = _Znwj(i0);
  p0 = i0;
  j1 = l4;
  i64_store((&memory), (u64)(i0), j1);
  i0 = p0;
  j1 = l5;
  i64_store((&memory), (u64)(i0 + 8), j1);
  i0 = l3;
  i1 = 144u;
  i0 += i1;
  i1 = 36u;
  i0 += i1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = 144u;
  i0 += i1;
  i1 = 24u;
  i0 += i1;
  i1 = p0;
  i2 = 16u;
  i1 += i2;
  l9 = i1;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = 164u;
  i0 += i1;
  i1 = l9;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = p0;
  i32_store((&memory), (u64)(i0 + 160), i1);
  i0 = l3;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 172), j1);
  i0 = l3;
  i1 = 48u;
  i0 += i1;
  i1 = 36u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = l3;
  i1 = i32_load8_u((&memory), (u64)(i1 + 80));
  p0 = i1;
  i2 = 1u;
  i1 >>= (i2 & 31);
  i2 = p0;
  i3 = 1u;
  i2 &= i3;
  i0 = i2 ? i0 : i1;
  l9 = i0;
  i1 = 32u;
  i0 += i1;
  p0 = i0;
  i0 = l9;
  j0 = (u64)(i0);
  p1 = j0;
  i0 = l3;
  i1 = 172u;
  i0 += i1;
  l9 = i0;
  L5: 
    i0 = p0;
    i1 = 1u;
    i0 += i1;
    p0 = i0;
    j0 = p1;
    j1 = 7ull;
    j0 >>= (j1 & 63);
    p1 = j0;
    j1 = 0ull;
    i0 = j0 != j1;
    if (i0) {goto L5;}
  i0 = p0;
  i0 = !(i0);
  if (i0) {goto B7;}
  i0 = l9;
  i1 = p0;
  f42(i0, i1);
  i0 = l3;
  i1 = 176u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l9 = i0;
  i0 = l3;
  i1 = 172u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  p0 = i0;
  goto B6;
  B7:;
  i0 = 0u;
  l9 = i0;
  i0 = 0u;
  p0 = i0;
  B6:;
  i0 = l3;
  i1 = p0;
  i32_store((&memory), (u64)(i0 + 212), i1);
  i0 = l3;
  i1 = p0;
  i32_store((&memory), (u64)(i0 + 208), i1);
  i0 = l3;
  i1 = l9;
  i32_store((&memory), (u64)(i0 + 216), i1);
  i0 = l3;
  i1 = l3;
  i2 = 208u;
  i1 += i2;
  i32_store((&memory), (u64)(i0 + 184), i1);
  i0 = l3;
  i1 = l3;
  i2 = 48u;
  i1 += i2;
  i32_store((&memory), (u64)(i0 + 192), i1);
  i0 = l3;
  i1 = 192u;
  i0 += i1;
  i1 = l3;
  i2 = 184u;
  i1 += i2;
  f43(i0, i1);
  i0 = l3;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 200), i1);
  i0 = l3;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 192), j1);
  i0 = 16u;
  p0 = i0;
  i0 = l3;
  i1 = 164u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l9 = i0;
  i1 = l3;
  i2 = 144u;
  i1 += i2;
  i2 = 16u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  p2 = i1;
  i0 -= i1;
  l6 = i0;
  i1 = 4u;
  i0 = (u32)((s32)i0 >> (i1 & 31));
  j0 = (u64)(i0);
  p1 = j0;
  L8: 
    i0 = p0;
    i1 = 1u;
    i0 += i1;
    p0 = i0;
    j0 = p1;
    j1 = 7ull;
    j0 >>= (j1 & 63);
    p1 = j0;
    j1 = 0ull;
    i0 = j0 != j1;
    if (i0) {goto L8;}
  i0 = p2;
  i1 = l9;
  i0 = i0 == i1;
  if (i0) {goto B9;}
  i0 = l6;
  i1 = 4294967280u;
  i0 &= i1;
  i1 = p0;
  i0 += i1;
  p0 = i0;
  B9:;
  i0 = p0;
  i1 = l3;
  i2 = 176u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  l9 = i1;
  i0 += i1;
  i1 = l3;
  i2 = 172u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  p2 = i1;
  i0 -= i1;
  p0 = i0;
  i0 = l9;
  i1 = p2;
  i0 -= i1;
  j0 = (u64)(i0);
  p1 = j0;
  L10: 
    i0 = p0;
    i1 = 1u;
    i0 += i1;
    p0 = i0;
    j0 = p1;
    j1 = 7ull;
    j0 >>= (j1 & 63);
    p1 = j0;
    j1 = 0ull;
    i0 = j0 != j1;
    if (i0) {goto L10;}
  i0 = p0;
  i0 = !(i0);
  if (i0) {goto B12;}
  i0 = l3;
  i1 = 192u;
  i0 += i1;
  i1 = p0;
  f42(i0, i1);
  i0 = l3;
  i0 = i32_load((&memory), (u64)(i0 + 196));
  l9 = i0;
  i0 = l3;
  i0 = i32_load((&memory), (u64)(i0 + 192));
  p0 = i0;
  goto B11;
  B12:;
  i0 = 0u;
  l9 = i0;
  i0 = 0u;
  p0 = i0;
  B11:;
  i0 = l3;
  i1 = p0;
  i32_store((&memory), (u64)(i0 + 212), i1);
  i0 = l3;
  i1 = p0;
  i32_store((&memory), (u64)(i0 + 208), i1);
  i0 = l3;
  i1 = l9;
  i32_store((&memory), (u64)(i0 + 216), i1);
  i0 = l3;
  i1 = 208u;
  i0 += i1;
  i1 = l3;
  i2 = 144u;
  i1 += i2;
  i0 = f44(i0, i1);
  i0 = l3;
  i0 = i32_load((&memory), (u64)(i0 + 192));
  p0 = i0;
  i1 = l3;
  i1 = i32_load((&memory), (u64)(i1 + 196));
  i2 = p0;
  i1 -= i2;
  (*Z_envZ_send_inlineZ_vii)(i0, i1);
  i0 = l3;
  i0 = i32_load((&memory), (u64)(i0 + 192));
  p0 = i0;
  i0 = !(i0);
  if (i0) {goto B13;}
  i0 = l3;
  i1 = p0;
  i32_store((&memory), (u64)(i0 + 196), i1);
  i0 = p0;
  _ZdlPv(i0);
  B13:;
  i0 = l3;
  i0 = i32_load((&memory), (u64)(i0 + 172));
  p0 = i0;
  i0 = !(i0);
  if (i0) {goto B14;}
  i0 = l3;
  i1 = 176u;
  i0 += i1;
  i1 = p0;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  _ZdlPv(i0);
  B14:;
  i0 = l3;
  i0 = i32_load((&memory), (u64)(i0 + 160));
  p0 = i0;
  i0 = !(i0);
  if (i0) {goto B15;}
  i0 = l3;
  i1 = 164u;
  i0 += i1;
  i1 = p0;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  _ZdlPv(i0);
  B15:;
  i0 = l3;
  i1 = 80u;
  i0 += i1;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B17;}
  i0 = l3;
  i0 = i32_load8_u((&memory), (u64)(i0 + 32));
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B16;}
  goto B0;
  B17:;
  i0 = l3;
  i1 = 88u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  _ZdlPv(i0);
  i0 = l3;
  i0 = i32_load8_u((&memory), (u64)(i0 + 32));
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B0;}
  B16:;
  i0 = l3;
  i1 = 40u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  _ZdlPv(i0);
  i0 = l3;
  i1 = 224u;
  i0 += i1;
  g0 = i0;
  goto Bfunc;
  B1:;
  i0 = l3;
  i1 = 32u;
  i0 += i1;
  f57(i0);
  UNREACHABLE;
  B0:;
  i0 = l3;
  i1 = 224u;
  i0 += i1;
  g0 = i0;
  Bfunc:;
  FUNC_EPILOGUE;
}

static void f42(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l2 = i0;
  i1 = p0;
  i1 = i32_load((&memory), (u64)(i1 + 4));
  l3 = i1;
  i0 -= i1;
  i1 = p1;
  i0 = i0 >= i1;
  if (i0) {goto B4;}
  i0 = l3;
  i1 = p0;
  i1 = i32_load((&memory), (u64)(i1));
  l4 = i1;
  i0 -= i1;
  l5 = i0;
  i1 = p1;
  i0 += i1;
  l6 = i0;
  i1 = 4294967295u;
  i0 = (u32)((s32)i0 <= (s32)i1);
  if (i0) {goto B2;}
  i0 = 2147483647u;
  l7 = i0;
  i0 = l2;
  i1 = l4;
  i0 -= i1;
  l2 = i0;
  i1 = 1073741822u;
  i0 = i0 > i1;
  if (i0) {goto B5;}
  i0 = l6;
  i1 = l2;
  i2 = 1u;
  i1 <<= (i2 & 31);
  l2 = i1;
  i2 = l2;
  i3 = l6;
  i2 = i2 < i3;
  i0 = i2 ? i0 : i1;
  l7 = i0;
  i0 = !(i0);
  if (i0) {goto B3;}
  B5:;
  i0 = l7;
  i0 = _Znwj(i0);
  l2 = i0;
  goto B1;
  B4:;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  p0 = i0;
  L6: 
    i0 = l3;
    i1 = 0u;
    i32_store8((&memory), (u64)(i0), i1);
    i0 = p0;
    i1 = p0;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 1u;
    i1 += i2;
    l3 = i1;
    i32_store((&memory), (u64)(i0), i1);
    i0 = p1;
    i1 = 4294967295u;
    i0 += i1;
    p1 = i0;
    if (i0) {goto L6;}
    goto B0;
  B3:;
  i0 = 0u;
  l7 = i0;
  i0 = 0u;
  l2 = i0;
  goto B1;
  B2:;
  i0 = p0;
  f68(i0);
  UNREACHABLE;
  B1:;
  i0 = l2;
  i1 = l7;
  i0 += i1;
  l7 = i0;
  i0 = l3;
  i1 = p1;
  i0 += i1;
  i1 = l4;
  i0 -= i1;
  l4 = i0;
  i0 = l2;
  i1 = l5;
  i0 += i1;
  l5 = i0;
  l3 = i0;
  L7: 
    i0 = l3;
    i1 = 0u;
    i32_store8((&memory), (u64)(i0), i1);
    i0 = l3;
    i1 = 1u;
    i0 += i1;
    l3 = i0;
    i0 = p1;
    i1 = 4294967295u;
    i0 += i1;
    p1 = i0;
    if (i0) {goto L7;}
  i0 = l2;
  i1 = l4;
  i0 += i1;
  l4 = i0;
  i0 = l5;
  i1 = p0;
  i2 = 4u;
  i1 += i2;
  l6 = i1;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = p0;
  i2 = i32_load((&memory), (u64)(i2));
  p1 = i2;
  i1 -= i2;
  l3 = i1;
  i0 -= i1;
  l2 = i0;
  i0 = l3;
  i1 = 1u;
  i0 = (u32)((s32)i0 < (s32)i1);
  if (i0) {goto B8;}
  i0 = l2;
  i1 = p1;
  i2 = l3;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  p1 = i0;
  B8:;
  i0 = p0;
  i1 = l2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l6;
  i1 = l4;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  i1 = l7;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p1;
  i0 = !(i0);
  if (i0) {goto B0;}
  i0 = p1;
  _ZdlPv(i0);
  goto Bfunc;
  B0:;
  Bfunc:;
  FUNC_EPILOGUE;
}

static void f43(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0, l5 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  u64 j1;
  i0 = g0;
  i1 = 16u;
  i0 -= i1;
  l2 = i0;
  g0 = i0;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  l3 = i0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  l4 = i0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1 + 4));
  l5 = i1;
  i0 -= i1;
  i1 = 7u;
  i0 = (u32)((s32)i0 > (s32)i1);
  if (i0) {goto B0;}
  i0 = 0u;
  i1 = 8694u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = l4;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l5 = i0;
  B0:;
  i0 = l5;
  i1 = l3;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = l4;
  i1 = 4u;
  i0 += i1;
  l4 = i0;
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 8u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  l5 = i0;
  i1 = 8u;
  i0 += i1;
  l3 = i0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  l4 = i0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1 + 4));
  p0 = i1;
  i0 -= i1;
  i1 = 7u;
  i0 = (u32)((s32)i0 > (s32)i1);
  if (i0) {goto B1;}
  i0 = 0u;
  i1 = 8694u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = l4;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  p0 = i0;
  B1:;
  i0 = p0;
  i1 = l3;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = l4;
  i1 = 4u;
  i0 += i1;
  l4 = i0;
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 8u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l5;
  i1 = 16u;
  i0 += i1;
  l3 = i0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  l4 = i0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1 + 4));
  p0 = i1;
  i0 -= i1;
  i1 = 7u;
  i0 = (u32)((s32)i0 > (s32)i1);
  if (i0) {goto B2;}
  i0 = 0u;
  i1 = 8694u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = l4;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  p0 = i0;
  B2:;
  i0 = p0;
  i1 = l3;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = l4;
  i1 = 4u;
  i0 += i1;
  p0 = i0;
  i1 = p0;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 8u;
  i1 += i2;
  l3 = i1;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = l5;
  i2 = 24u;
  i1 += i2;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0 + 8), j1);
  i0 = l4;
  i1 = 8u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = l3;
  i0 -= i1;
  i1 = 7u;
  i0 = (u32)((s32)i0 > (s32)i1);
  if (i0) {goto B3;}
  i0 = 0u;
  i1 = 8694u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  l3 = i0;
  B3:;
  i0 = l3;
  i1 = l2;
  i2 = 8u;
  i1 += i2;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = p0;
  i1 = p0;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 8u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = l5;
  i2 = 32u;
  i1 += i2;
  i0 = f45(i0, i1);
  i0 = l2;
  i1 = 16u;
  i0 += i1;
  g0 = i0;
  FUNC_EPILOGUE;
}

static u32 f44(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  i1 = p0;
  i1 = i32_load((&memory), (u64)(i1 + 4));
  l2 = i1;
  i0 -= i1;
  i1 = 7u;
  i0 = (u32)((s32)i0 > (s32)i1);
  if (i0) {goto B0;}
  i0 = 0u;
  i1 = 8694u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l2 = i0;
  B0:;
  i0 = l2;
  i1 = p1;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l2 = i0;
  i1 = l2;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 8u;
  i1 += i2;
  l3 = i1;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p1;
  i1 = 8u;
  i0 += i1;
  l4 = i0;
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = l3;
  i0 -= i1;
  i1 = 7u;
  i0 = (u32)((s32)i0 > (s32)i1);
  if (i0) {goto B1;}
  i0 = 0u;
  i1 = 8694u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = l2;
  i0 = i32_load((&memory), (u64)(i0));
  l3 = i0;
  B1:;
  i0 = l3;
  i1 = l4;
  i2 = 8u;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = l2;
  i1 = l2;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 8u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = p1;
  i2 = 16u;
  i1 += i2;
  i0 = f46(i0, i1);
  i1 = p1;
  i2 = 28u;
  i1 += i2;
  i0 = f47(i0, i1);
  FUNC_EPILOGUE;
  return i0;
}

static u32 f45(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l5 = 0, l6 = 0, l7 = 0, l8 = 0;
  u64 l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j0, j1, j2;
  i0 = g0;
  i1 = 16u;
  i0 -= i1;
  l2 = i0;
  g0 = i0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  i1 = p1;
  i1 = i32_load8_u((&memory), (u64)(i1));
  l3 = i1;
  i2 = 1u;
  i1 >>= (i2 & 31);
  i2 = l3;
  i3 = 1u;
  i2 &= i3;
  i0 = i2 ? i0 : i1;
  j0 = (u64)(i0);
  l4 = j0;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l3 = i0;
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  l5 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l6 = i0;
  L0: 
    j0 = l4;
    i0 = (u32)(j0);
    l7 = i0;
    i0 = l2;
    j1 = l4;
    j2 = 7ull;
    j1 >>= (j2 & 63);
    l4 = j1;
    j2 = 0ull;
    i1 = j1 != j2;
    l8 = i1;
    i2 = 7u;
    i1 <<= (i2 & 31);
    i2 = l7;
    i3 = 127u;
    i2 &= i3;
    i1 |= i2;
    i32_store8((&memory), (u64)(i0 + 15), i1);
    i0 = l5;
    i0 = i32_load((&memory), (u64)(i0));
    i1 = l3;
    i0 -= i1;
    i1 = 0u;
    i0 = (u32)((s32)i0 > (s32)i1);
    if (i0) {goto B1;}
    i0 = 0u;
    i1 = 8694u;
    (*Z_envZ_eosio_assertZ_vii)(i0, i1);
    i0 = l6;
    i0 = i32_load((&memory), (u64)(i0));
    l3 = i0;
    B1:;
    i0 = l3;
    i1 = l2;
    i2 = 15u;
    i1 += i2;
    i2 = 1u;
    i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
    i0 = l6;
    i1 = l6;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 1u;
    i1 += i2;
    l3 = i1;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l8;
    if (i0) {goto L0;}
  i0 = p1;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = p1;
  i1 = i32_load8_u((&memory), (u64)(i1));
  l6 = i1;
  i2 = 1u;
  i1 >>= (i2 & 31);
  i2 = l6;
  i3 = 1u;
  i2 &= i3;
  l7 = i2;
  i0 = i2 ? i0 : i1;
  l6 = i0;
  i0 = !(i0);
  if (i0) {goto B2;}
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  i1 = p1;
  i2 = 1u;
  i1 += i2;
  i2 = l7;
  i0 = i2 ? i0 : i1;
  l7 = i0;
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = l3;
  i0 -= i1;
  i1 = l6;
  i0 = (u32)((s32)i0 >= (s32)i1);
  if (i0) {goto B3;}
  i0 = 0u;
  i1 = 8694u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l3 = i0;
  B3:;
  i0 = l3;
  i1 = l7;
  i2 = l6;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l3 = i0;
  i1 = l3;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = l6;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  B2:;
  i0 = l2;
  i1 = 16u;
  i0 += i1;
  g0 = i0;
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f46(u32 p0, u32 p1) {
  u32 l2 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0;
  u64 l3 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j0, j1, j2;
  i0 = g0;
  i1 = 16u;
  i0 -= i1;
  l2 = i0;
  g0 = i0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  i1 = p1;
  i1 = i32_load((&memory), (u64)(i1));
  i0 -= i1;
  i1 = 4u;
  i0 = (u32)((s32)i0 >> (i1 & 31));
  j0 = (u64)(i0);
  l3 = j0;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l4 = i0;
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  l5 = i0;
  L0: 
    j0 = l3;
    i0 = (u32)(j0);
    l6 = i0;
    i0 = l2;
    j1 = l3;
    j2 = 7ull;
    j1 >>= (j2 & 63);
    l3 = j1;
    j2 = 0ull;
    i1 = j1 != j2;
    l7 = i1;
    i2 = 7u;
    i1 <<= (i2 & 31);
    i2 = l6;
    i3 = 127u;
    i2 &= i3;
    i1 |= i2;
    i32_store8((&memory), (u64)(i0 + 15), i1);
    i0 = l5;
    i0 = i32_load((&memory), (u64)(i0));
    i1 = l4;
    i0 -= i1;
    i1 = 0u;
    i0 = (u32)((s32)i0 > (s32)i1);
    if (i0) {goto B1;}
    i0 = 0u;
    i1 = 8694u;
    (*Z_envZ_eosio_assertZ_vii)(i0, i1);
    i0 = p0;
    i1 = 4u;
    i0 += i1;
    i0 = i32_load((&memory), (u64)(i0));
    l4 = i0;
    B1:;
    i0 = l4;
    i1 = l2;
    i2 = 15u;
    i1 += i2;
    i2 = 1u;
    i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
    i0 = p0;
    i1 = 4u;
    i0 += i1;
    l4 = i0;
    i1 = l4;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 1u;
    i1 += i2;
    l4 = i1;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l7;
    if (i0) {goto L0;}
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  l7 = i0;
  i1 = p1;
  i2 = 4u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  p1 = i1;
  i0 = i0 == i1;
  if (i0) {goto B2;}
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  l5 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l6 = i0;
  L3: 
    i0 = l5;
    i0 = i32_load((&memory), (u64)(i0));
    i1 = l4;
    i0 -= i1;
    i1 = 7u;
    i0 = (u32)((s32)i0 > (s32)i1);
    if (i0) {goto B4;}
    i0 = 0u;
    i1 = 8694u;
    (*Z_envZ_eosio_assertZ_vii)(i0, i1);
    i0 = l6;
    i0 = i32_load((&memory), (u64)(i0));
    l4 = i0;
    B4:;
    i0 = l4;
    i1 = l7;
    i2 = 8u;
    i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
    i0 = l6;
    i1 = l6;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 8u;
    i1 += i2;
    l4 = i1;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l5;
    i0 = i32_load((&memory), (u64)(i0));
    i1 = l4;
    i0 -= i1;
    i1 = 7u;
    i0 = (u32)((s32)i0 > (s32)i1);
    if (i0) {goto B5;}
    i0 = 0u;
    i1 = 8694u;
    (*Z_envZ_eosio_assertZ_vii)(i0, i1);
    i0 = l6;
    i0 = i32_load((&memory), (u64)(i0));
    l4 = i0;
    B5:;
    i0 = l4;
    i1 = l7;
    i2 = 8u;
    i1 += i2;
    i2 = 8u;
    i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
    i0 = l6;
    i1 = l6;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 8u;
    i1 += i2;
    l4 = i1;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l7;
    i1 = 16u;
    i0 += i1;
    l7 = i0;
    i1 = p1;
    i0 = i0 != i1;
    if (i0) {goto L3;}
  B2:;
  i0 = l2;
  i1 = 16u;
  i0 += i1;
  g0 = i0;
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f47(u32 p0, u32 p1) {
  u32 l2 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0, l8 = 0;
  u64 l3 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j0, j1, j2;
  i0 = g0;
  i1 = 16u;
  i0 -= i1;
  l2 = i0;
  g0 = i0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  i1 = p1;
  i1 = i32_load((&memory), (u64)(i1));
  i0 -= i1;
  j0 = (u64)(i0);
  l3 = j0;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l4 = i0;
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  l5 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l6 = i0;
  L0: 
    j0 = l3;
    i0 = (u32)(j0);
    l7 = i0;
    i0 = l2;
    j1 = l3;
    j2 = 7ull;
    j1 >>= (j2 & 63);
    l3 = j1;
    j2 = 0ull;
    i1 = j1 != j2;
    l8 = i1;
    i2 = 7u;
    i1 <<= (i2 & 31);
    i2 = l7;
    i3 = 127u;
    i2 &= i3;
    i1 |= i2;
    i32_store8((&memory), (u64)(i0 + 15), i1);
    i0 = l5;
    i0 = i32_load((&memory), (u64)(i0));
    i1 = l4;
    i0 -= i1;
    i1 = 0u;
    i0 = (u32)((s32)i0 > (s32)i1);
    if (i0) {goto B1;}
    i0 = 0u;
    i1 = 8694u;
    (*Z_envZ_eosio_assertZ_vii)(i0, i1);
    i0 = l6;
    i0 = i32_load((&memory), (u64)(i0));
    l4 = i0;
    B1:;
    i0 = l4;
    i1 = l2;
    i2 = 15u;
    i1 += i2;
    i2 = 1u;
    i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
    i0 = l6;
    i1 = l6;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 1u;
    i1 += i2;
    l4 = i1;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l8;
    if (i0) {goto L0;}
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = l4;
  i0 -= i1;
  i1 = p1;
  i2 = 4u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = p1;
  i2 = i32_load((&memory), (u64)(i2));
  l7 = i2;
  i1 -= i2;
  l6 = i1;
  i0 = (u32)((s32)i0 >= (s32)i1);
  if (i0) {goto B2;}
  i0 = 0u;
  i1 = 8694u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l4 = i0;
  B2:;
  i0 = l4;
  i1 = l7;
  i2 = l6;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l4 = i0;
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = l6;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = 16u;
  i0 += i1;
  g0 = i0;
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f48(u32 p0, u32 p1) {
  u32 l2 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0, l8 = 0;
  u64 l3 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j0, j1;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l2 = i0;
  j0 = 0ull;
  l3 = j0;
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  l4 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l5 = i0;
  i0 = 0u;
  l6 = i0;
  L0: 
    i0 = l2;
    i1 = l4;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 < i1;
    if (i0) {goto B1;}
    i0 = 0u;
    i1 = 8705u;
    (*Z_envZ_eosio_assertZ_vii)(i0, i1);
    i0 = l5;
    i0 = i32_load((&memory), (u64)(i0));
    l2 = i0;
    B1:;
    i0 = l2;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l7 = i0;
    i0 = l5;
    i1 = l2;
    i2 = 1u;
    i1 += i2;
    l8 = i1;
    i32_store((&memory), (u64)(i0), i1);
    j0 = l3;
    i1 = l7;
    i2 = 127u;
    i1 &= i2;
    i2 = l6;
    i3 = 255u;
    i2 &= i3;
    l2 = i2;
    i1 <<= (i2 & 31);
    j1 = (u64)(i1);
    j0 |= j1;
    l3 = j0;
    i0 = l2;
    i1 = 7u;
    i0 += i1;
    l6 = i0;
    i0 = l8;
    l2 = i0;
    i0 = l7;
    i1 = 128u;
    i0 &= i1;
    if (i0) {goto L0;}
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l7 = i0;
  i1 = p1;
  i1 = i32_load((&memory), (u64)(i1));
  l2 = i1;
  i0 -= i1;
  l5 = i0;
  j1 = l3;
  i1 = (u32)(j1);
  l6 = i1;
  i0 = i0 >= i1;
  if (i0) {goto B3;}
  i0 = p1;
  i1 = l6;
  i2 = l5;
  i1 -= i2;
  f42(i0, i1);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l8 = i0;
  i0 = p1;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l7 = i0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  l2 = i0;
  goto B2;
  B3:;
  i0 = l5;
  i1 = l6;
  i0 = i0 <= i1;
  if (i0) {goto B2;}
  i0 = p1;
  i1 = 4u;
  i0 += i1;
  i1 = l2;
  i2 = l6;
  i1 += i2;
  l7 = i1;
  i32_store((&memory), (u64)(i0), i1);
  B2:;
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = l8;
  i0 -= i1;
  i1 = l7;
  i2 = l2;
  i1 -= i2;
  l7 = i1;
  i0 = i0 >= i1;
  if (i0) {goto B4;}
  i0 = 0u;
  i1 = 8700u;
  (*Z_envZ_eosio_assertZ_vii)(i0, i1);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l8 = i0;
  B4:;
  i0 = l2;
  i1 = l8;
  i2 = l7;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l2 = i0;
  i1 = l2;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = l7;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static u32 _Znwj(u32 p0) {
  u32 l1 = 0, l2 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p0;
  i1 = 1u;
  i2 = p0;
  i0 = i2 ? i0 : i1;
  l1 = i0;
  i0 = f82(i0);
  p0 = i0;
  if (i0) {goto B0;}
  L1: 
    i0 = 0u;
    p0 = i0;
    i0 = 0u;
    i0 = i32_load((&memory), (u64)(i0 + 8728));
    l2 = i0;
    i0 = !(i0);
    if (i0) {goto B0;}
    i0 = l2;
    CALL_INDIRECT(T0, void (*)(void), 0, i0);
    i0 = l1;
    i0 = f82(i0);
    p0 = i0;
    i0 = !(i0);
    if (i0) {goto L1;}
  B0:;
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static u32 _Znaj(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = p0;
  i0 = _Znwj(i0);
  FUNC_EPILOGUE;
  return i0;
}

static void _ZdlPv(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = p0;
  i0 = !(i0);
  if (i0) {goto B0;}
  i0 = p0;
  f85(i0);
  B0:;
  FUNC_EPILOGUE;
}

static void _ZdaPv(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = p0;
  _ZdlPv(i0);
  FUNC_EPILOGUE;
}

static u32 _ZnwjSt11align_val_t(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  i0 = g0;
  i1 = 16u;
  i0 -= i1;
  l2 = i0;
  g0 = i0;
  i0 = l2;
  i1 = 12u;
  i0 += i1;
  i1 = p1;
  i2 = 4u;
  i3 = p1;
  i4 = 4u;
  i3 = i3 > i4;
  i1 = i3 ? i1 : i2;
  p1 = i1;
  i2 = p0;
  i3 = 1u;
  i4 = p0;
  i2 = i4 ? i2 : i3;
  l3 = i2;
  i0 = f80(i0, i1, i2);
  i0 = !(i0);
  if (i0) {goto B0;}
  L2: 
    i0 = 0u;
    i0 = i32_load((&memory), (u64)(i0 + 8728));
    p0 = i0;
    i0 = !(i0);
    if (i0) {goto B1;}
    i0 = p0;
    CALL_INDIRECT(T0, void (*)(void), 0, i0);
    i0 = l2;
    i1 = 12u;
    i0 += i1;
    i1 = p1;
    i2 = l3;
    i0 = f80(i0, i1, i2);
    if (i0) {goto L2;}
    goto B0;
  B1:;
  i0 = l2;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 12), i1);
  B0:;
  i0 = l2;
  i0 = i32_load((&memory), (u64)(i0 + 12));
  p0 = i0;
  i0 = l2;
  i1 = 16u;
  i0 += i1;
  g0 = i0;
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static u32 _ZnajSt11align_val_t(u32 p0, u32 p1) {
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = p0;
  i1 = p1;
  i0 = _ZnwjSt11align_val_t(i0, i1);
  FUNC_EPILOGUE;
  return i0;
}

static void _ZdlPvSt11align_val_t(u32 p0, u32 p1) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = p0;
  i0 = !(i0);
  if (i0) {goto B0;}
  i0 = p0;
  f85(i0);
  B0:;
  FUNC_EPILOGUE;
}

static void _ZdaPvSt11align_val_t(u32 p0, u32 p1) {
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = p0;
  i1 = p1;
  _ZdlPvSt11align_val_t(i0, i1);
  FUNC_EPILOGUE;
}

static void f57(u32 p0) {
  FUNC_PROLOGUE;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  FUNC_EPILOGUE;
}

static u32 f58(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  u64 j1;
  i0 = p0;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0), j1);
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  l2 = i0;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p1;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B0;}
  i0 = p0;
  i1 = p1;
  j1 = i64_load((&memory), (u64)(i1));
  i64_store((&memory), (u64)(i0), j1);
  i0 = l2;
  i1 = p1;
  i2 = 8u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  goto Bfunc;
  B0:;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l2 = i0;
  i1 = 4294967280u;
  i0 = i0 >= i1;
  if (i0) {goto B1;}
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l3 = i0;
  i0 = l2;
  i1 = 11u;
  i0 = i0 >= i1;
  if (i0) {goto B3;}
  i0 = p0;
  i1 = l2;
  i2 = 1u;
  i1 <<= (i2 & 31);
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = 1u;
  i0 += i1;
  p1 = i0;
  i0 = l2;
  if (i0) {goto B2;}
  i0 = p1;
  i1 = l2;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p0;
  goto Bfunc;
  B3:;
  i0 = l2;
  i1 = 16u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  l4 = i0;
  i0 = _Znwj(i0);
  p1 = i0;
  i0 = p0;
  i1 = l4;
  i2 = 1u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = p1;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = p0;
  i1 = l2;
  i32_store((&memory), (u64)(i0 + 4), i1);
  B2:;
  i0 = p1;
  i1 = l3;
  i2 = l2;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = p1;
  i1 = l2;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p0;
  goto Bfunc;
  B1:;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f59(u32 p0, u32 p1, u32 p2, u32 p3, u32 p4) {
  u32 l5 = 0, l6 = 0, l7 = 0, l8 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j1;
  i0 = p0;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0), j1);
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  i1 = p1;
  i1 = i32_load8_u((&memory), (u64)(i1));
  l5 = i1;
  i2 = 1u;
  i1 >>= (i2 & 31);
  i2 = l5;
  i3 = 1u;
  i2 &= i3;
  l6 = i2;
  i0 = i2 ? i0 : i1;
  l5 = i0;
  i1 = p2;
  i0 = i0 < i1;
  if (i0) {goto B0;}
  i0 = l5;
  i1 = p2;
  i0 -= i1;
  l5 = i0;
  i1 = p3;
  i2 = l5;
  i3 = p3;
  i2 = i2 < i3;
  i0 = i2 ? i0 : i1;
  p3 = i0;
  i1 = 4294967280u;
  i0 = i0 >= i1;
  if (i0) {goto B0;}
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l7 = i0;
  i0 = p3;
  i1 = 11u;
  i0 = i0 >= i1;
  if (i0) {goto B2;}
  i0 = p0;
  i1 = p3;
  i2 = 1u;
  i1 <<= (i2 & 31);
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = 1u;
  i0 += i1;
  l5 = i0;
  i0 = p3;
  if (i0) {goto B1;}
  i0 = l5;
  i1 = p3;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p0;
  goto Bfunc;
  B2:;
  i0 = p3;
  i1 = 16u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  l8 = i0;
  i0 = _Znwj(i0);
  l5 = i0;
  i0 = p0;
  i1 = l8;
  i2 = 1u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = l5;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = p0;
  i1 = p3;
  i32_store((&memory), (u64)(i0 + 4), i1);
  B1:;
  i0 = l5;
  i1 = l7;
  i2 = p1;
  i3 = 1u;
  i2 += i3;
  i3 = l6;
  i1 = i3 ? i1 : i2;
  i2 = p2;
  i1 += i2;
  i2 = p3;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = l5;
  i1 = p3;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p0;
  goto Bfunc;
  B0:;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static void f60(u32 p0, u32 p1, u32 p2, u32 p3, u32 p4, u32 p5, u32 p6, u32 p7) {
  u32 l8 = 0, l9 = 0, l10 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = 4294967278u;
  i1 = p1;
  i0 -= i1;
  i1 = p2;
  i0 = i0 < i1;
  if (i0) {goto B0;}
  i0 = p0;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B3;}
  i0 = p0;
  i1 = 1u;
  i0 += i1;
  l8 = i0;
  i0 = 4294967279u;
  l9 = i0;
  i0 = p1;
  i1 = 2147483622u;
  i0 = i0 <= i1;
  if (i0) {goto B2;}
  goto B1;
  B3:;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l8 = i0;
  i0 = 4294967279u;
  l9 = i0;
  i0 = p1;
  i1 = 2147483622u;
  i0 = i0 > i1;
  if (i0) {goto B1;}
  B2:;
  i0 = 11u;
  l9 = i0;
  i0 = p1;
  i1 = 1u;
  i0 <<= (i1 & 31);
  l10 = i0;
  i1 = p2;
  i2 = p1;
  i1 += i2;
  p2 = i1;
  i2 = p2;
  i3 = l10;
  i2 = i2 < i3;
  i0 = i2 ? i0 : i1;
  p2 = i0;
  i1 = 11u;
  i0 = i0 < i1;
  if (i0) {goto B1;}
  i0 = p2;
  i1 = 16u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  l9 = i0;
  B1:;
  i0 = l9;
  i0 = _Znwj(i0);
  p2 = i0;
  i0 = p4;
  i0 = !(i0);
  if (i0) {goto B4;}
  i0 = p2;
  i1 = l8;
  i2 = p4;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  B4:;
  i0 = p6;
  i0 = !(i0);
  if (i0) {goto B5;}
  i0 = p2;
  i1 = p4;
  i0 += i1;
  i1 = p7;
  i2 = p6;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  B5:;
  i0 = p3;
  i1 = p5;
  i0 -= i1;
  p3 = i0;
  i1 = p4;
  i0 -= i1;
  p7 = i0;
  i0 = !(i0);
  if (i0) {goto B6;}
  i0 = p2;
  i1 = p4;
  i0 += i1;
  i1 = p6;
  i0 += i1;
  i1 = l8;
  i2 = p4;
  i1 += i2;
  i2 = p5;
  i1 += i2;
  i2 = p7;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  B6:;
  i0 = p1;
  i1 = 10u;
  i0 = i0 == i1;
  if (i0) {goto B7;}
  i0 = l8;
  _ZdlPv(i0);
  B7:;
  i0 = p0;
  i1 = p2;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = p0;
  i1 = l9;
  i2 = 1u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = p3;
  i2 = p6;
  i1 += i2;
  p4 = i1;
  i32_store((&memory), (u64)(i0 + 4), i1);
  i0 = p2;
  i1 = p4;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  goto Bfunc;
  B0:;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  Bfunc:;
  FUNC_EPILOGUE;
}

static void f61(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0, l8 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = p1;
  i1 = 4294967280u;
  i0 = i0 >= i1;
  if (i0) {goto B3;}
  i0 = p0;
  i0 = i32_load8_u((&memory), (u64)(i0));
  l2 = i0;
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B5;}
  i0 = l2;
  i1 = 1u;
  i0 >>= (i1 & 31);
  l3 = i0;
  i0 = 10u;
  l4 = i0;
  goto B4;
  B5:;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  l2 = i0;
  i1 = 4294967294u;
  i0 &= i1;
  i1 = 4294967295u;
  i0 += i1;
  l4 = i0;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l3 = i0;
  B4:;
  i0 = 10u;
  l5 = i0;
  i0 = l3;
  i1 = p1;
  i2 = l3;
  i3 = p1;
  i2 = i2 > i3;
  i0 = i2 ? i0 : i1;
  p1 = i0;
  i1 = 11u;
  i0 = i0 < i1;
  if (i0) {goto B6;}
  i0 = p1;
  i1 = 16u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  i1 = 4294967295u;
  i0 += i1;
  l5 = i0;
  B6:;
  i0 = l5;
  i1 = l4;
  i0 = i0 == i1;
  if (i0) {goto B9;}
  i0 = l5;
  i1 = 10u;
  i0 = i0 != i1;
  if (i0) {goto B10;}
  i0 = 1u;
  l6 = i0;
  i0 = p0;
  i1 = 1u;
  i0 += i1;
  p1 = i0;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l4 = i0;
  i0 = 0u;
  l7 = i0;
  i0 = 1u;
  l8 = i0;
  i0 = l2;
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B7;}
  goto B2;
  B10:;
  i0 = l5;
  i1 = 1u;
  i0 += i1;
  i0 = _Znwj(i0);
  p1 = i0;
  i0 = l5;
  i1 = l4;
  i0 = i0 > i1;
  if (i0) {goto B8;}
  i0 = p1;
  if (i0) {goto B8;}
  B9:;
  goto Bfunc;
  B8:;
  i0 = p0;
  i0 = i32_load8_u((&memory), (u64)(i0));
  l2 = i0;
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B11;}
  i0 = 1u;
  l7 = i0;
  i0 = p0;
  i1 = 1u;
  i0 += i1;
  l4 = i0;
  i0 = 0u;
  l6 = i0;
  i0 = 1u;
  l8 = i0;
  i0 = l2;
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B2;}
  goto B7;
  B11:;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l4 = i0;
  i0 = 1u;
  l6 = i0;
  i0 = 1u;
  l7 = i0;
  i0 = 1u;
  l8 = i0;
  i0 = l2;
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B2;}
  B7:;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  i1 = 1u;
  i0 += i1;
  l2 = i0;
  i0 = !(i0);
  if (i0) {goto B0;}
  goto B1;
  B3:;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  B2:;
  i0 = l2;
  i1 = 254u;
  i0 &= i1;
  i1 = l8;
  i0 >>= (i1 & 31);
  i1 = 1u;
  i0 += i1;
  l2 = i0;
  i0 = !(i0);
  if (i0) {goto B0;}
  B1:;
  i0 = p1;
  i1 = l4;
  i2 = l2;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  B0:;
  i0 = l6;
  i0 = !(i0);
  if (i0) {goto B12;}
  i0 = l4;
  _ZdlPv(i0);
  B12:;
  i0 = l7;
  i0 = !(i0);
  if (i0) {goto B13;}
  i0 = p0;
  i1 = l3;
  i32_store((&memory), (u64)(i0 + 4), i1);
  i0 = p0;
  i1 = p1;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = p0;
  i1 = l5;
  i2 = 1u;
  i1 += i2;
  i2 = 1u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  goto Bfunc;
  B13:;
  i0 = p0;
  i1 = l3;
  i2 = 1u;
  i1 <<= (i2 & 31);
  i32_store8((&memory), (u64)(i0), i1);
  Bfunc:;
  FUNC_EPILOGUE;
}

static void f62(u32 p0) {
  u32 l1 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = g0;
  i1 = 16u;
  i0 -= i1;
  l1 = i0;
  g0 = i0;
  i0 = l1;
  i1 = p0;
  i2 = 17144u;
  f65(i0, i1, i2);
  f66();
  UNREACHABLE;
  FUNC_EPILOGUE;
}

static void f63(u32 p0) {
  u32 l1 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = g0;
  i1 = 16u;
  i0 -= i1;
  l1 = i0;
  g0 = i0;
  i0 = l1;
  i1 = p0;
  i2 = 8709u;
  f65(i0, i1, i2);
  f67();
  UNREACHABLE;
  FUNC_EPILOGUE;
}

static u64 f64_0(u32 p0, u32 p1, u32 p2) {
  u32 l3 = 0, l4 = 0, l5 = 0, l6 = 0;
  u64 l7 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j0, j1;
  i0 = g0;
  i1 = 16u;
  i0 -= i1;
  l3 = i0;
  g0 = i0;
  i0 = l3;
  i1 = 8u;
  i0 += i1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0), j1);
  i0 = 8485u;
  i0 = f79(i0);
  l4 = i0;
  i1 = 4294967280u;
  i0 = i0 >= i1;
  if (i0) {goto B2;}
  i0 = l4;
  i1 = 11u;
  i0 = i0 >= i1;
  if (i0) {goto B5;}
  i0 = l3;
  i1 = l4;
  i2 = 1u;
  i1 <<= (i2 & 31);
  i32_store8((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = 1u;
  i0 |= i1;
  l5 = i0;
  i0 = l4;
  if (i0) {goto B4;}
  goto B3;
  B5:;
  i0 = l4;
  i1 = 16u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  l6 = i0;
  i0 = _Znwj(i0);
  l5 = i0;
  i0 = l3;
  i1 = l6;
  i2 = 1u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = l5;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = l3;
  i1 = l4;
  i32_store((&memory), (u64)(i0 + 4), i1);
  B4:;
  i0 = l5;
  i1 = 8485u;
  i2 = l4;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  B3:;
  i0 = l5;
  i1 = l4;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 12), i1);
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l4 = i0;
  i0 = p0;
  i0 = i32_load8_u((&memory), (u64)(i0));
  l5 = i0;
  i0 = f69();
  i0 = i32_load((&memory), (u64)(i0));
  l6 = i0;
  i0 = f69();
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l4;
  i1 = p0;
  i2 = 1u;
  i1 += i2;
  i2 = l5;
  i3 = 1u;
  i2 &= i3;
  i0 = i2 ? i0 : i1;
  l4 = i0;
  i1 = l3;
  i2 = 12u;
  i1 += i2;
  i2 = p2;
  j0 = f76(i0, i1, i2);
  l7 = j0;
  i0 = f69();
  p0 = i0;
  i0 = i32_load((&memory), (u64)(i0));
  l5 = i0;
  i0 = p0;
  i1 = l6;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l5;
  i1 = 34u;
  i0 = i0 == i1;
  if (i0) {goto B1;}
  i0 = l3;
  i0 = i32_load((&memory), (u64)(i0 + 12));
  p0 = i0;
  i1 = l4;
  i0 = i0 == i1;
  if (i0) {goto B0;}
  i0 = p1;
  i0 = !(i0);
  if (i0) {goto B6;}
  i0 = p1;
  i1 = p0;
  i2 = l4;
  i1 -= i2;
  i32_store((&memory), (u64)(i0), i1);
  B6:;
  i0 = l3;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B7;}
  i0 = l3;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  _ZdlPv(i0);
  B7:;
  i0 = l3;
  i1 = 16u;
  i0 += i1;
  g0 = i0;
  j0 = l7;
  goto Bfunc;
  B2:;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  B1:;
  i0 = l3;
  f62(i0);
  UNREACHABLE;
  B0:;
  i0 = l3;
  f63(i0);
  UNREACHABLE;
  Bfunc:;
  FUNC_EPILOGUE;
  return j0;
}

static void f65(u32 p0, u32 p1, u32 p2) {
  u32 l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0, l8 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4, i5, i6, i7;
  u64 j1;
  i0 = p0;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = p0;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0), j1);
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  i1 = p1;
  i1 = i32_load8_u((&memory), (u64)(i1));
  l3 = i1;
  i2 = 1u;
  i1 >>= (i2 & 31);
  i2 = l3;
  i3 = 1u;
  i2 &= i3;
  i0 = i2 ? i0 : i1;
  l3 = i0;
  i1 = p2;
  i1 = f79(i1);
  l4 = i1;
  i0 += i1;
  l5 = i0;
  i1 = 4294967280u;
  i0 = i0 >= i1;
  if (i0) {goto B3;}
  i0 = p1;
  i0 = i32_load8_u((&memory), (u64)(i0));
  l6 = i0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l7 = i0;
  i0 = l5;
  i1 = 10u;
  i0 = i0 > i1;
  if (i0) {goto B6;}
  i0 = p0;
  i1 = l3;
  i2 = 1u;
  i1 <<= (i2 & 31);
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = 1u;
  i0 += i1;
  l5 = i0;
  i0 = l3;
  if (i0) {goto B5;}
  goto B4;
  B6:;
  i0 = l5;
  i1 = 16u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  l8 = i0;
  i0 = _Znwj(i0);
  l5 = i0;
  i0 = p0;
  i1 = l8;
  i2 = 1u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  i1 = l5;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  i1 = l3;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  i0 = !(i0);
  if (i0) {goto B4;}
  B5:;
  i0 = l5;
  i1 = l7;
  i2 = p1;
  i3 = 1u;
  i2 += i3;
  i3 = l6;
  i4 = 1u;
  i3 &= i4;
  i1 = i3 ? i1 : i2;
  i2 = l3;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  B4:;
  i0 = l5;
  i1 = l3;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  i0 = p0;
  i0 = i32_load8_u((&memory), (u64)(i0));
  p1 = i0;
  i1 = 1u;
  i0 &= i1;
  l5 = i0;
  if (i0) {goto B8;}
  i0 = 10u;
  l3 = i0;
  i0 = 10u;
  i1 = p1;
  i2 = 1u;
  i1 >>= (i2 & 31);
  p1 = i1;
  i0 -= i1;
  i1 = l4;
  i0 = i0 < i1;
  if (i0) {goto B7;}
  goto B2;
  B8:;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = 4294967294u;
  i0 &= i1;
  i1 = 4294967295u;
  i0 += i1;
  l3 = i0;
  i1 = p0;
  i2 = 4u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  p1 = i1;
  i0 -= i1;
  i1 = l4;
  i0 = i0 >= i1;
  if (i0) {goto B2;}
  B7:;
  i0 = p0;
  i1 = l3;
  i2 = p1;
  i3 = l4;
  i2 += i3;
  i3 = l3;
  i2 -= i3;
  i3 = p1;
  i4 = p1;
  i5 = 0u;
  i6 = l4;
  i7 = p2;
  f60(i0, i1, i2, i3, i4, i5, i6, i7);
  goto B1;
  B3:;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  B2:;
  i0 = l4;
  i0 = !(i0);
  if (i0) {goto B1;}
  i0 = p0;
  i1 = 8u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = p0;
  i2 = 1u;
  i1 += i2;
  i2 = l5;
  i0 = i2 ? i0 : i1;
  l3 = i0;
  i1 = p1;
  i0 += i1;
  i1 = p2;
  i2 = l4;
  i0 = (*Z_envZ_memcpyZ_iiii)(i0, i1, i2);
  i0 = p1;
  i1 = l4;
  i0 += i1;
  p1 = i0;
  i0 = p0;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  if (i0) {goto B0;}
  i0 = p0;
  i1 = p1;
  i2 = 1u;
  i1 <<= (i2 & 31);
  i32_store8((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = p1;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  goto Bfunc;
  B1:;
  goto Bfunc;
  B0:;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  i1 = p1;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = p1;
  i0 += i1;
  i1 = 0u;
  i32_store8((&memory), (u64)(i0), i1);
  Bfunc:;
  FUNC_EPILOGUE;
}

static void f66(void) {
  FUNC_PROLOGUE;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  FUNC_EPILOGUE;
}

static void f67(void) {
  FUNC_PROLOGUE;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  FUNC_EPILOGUE;
}

static void f68(u32 p0) {
  FUNC_PROLOGUE;
  (*Z_envZ_abortZ_vv)();
  UNREACHABLE;
  FUNC_EPILOGUE;
}

static u32 f69(void) {
  FUNC_PROLOGUE;
  u32 i0;
  i0 = 8732u;
  FUNC_EPILOGUE;
  return i0;
}

static void f70(u32 p0) {
  FUNC_PROLOGUE;
  FUNC_EPILOGUE;
}

static u32 f71(u32 p0) {
  u32 l1 = 0, l2 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  u64 j1;
  i0 = p0;
  i1 = p0;
  i1 = i32_load8_u((&memory), (u64)(i1 + 74));
  l1 = i1;
  i2 = 4294967295u;
  i1 += i2;
  i2 = l1;
  i1 |= i2;
  i32_store8((&memory), (u64)(i0 + 74), i1);
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 20));
  i1 = p0;
  i1 = i32_load((&memory), (u64)(i1 + 28));
  i0 = i0 <= i1;
  if (i0) {goto B0;}
  i0 = p0;
  i1 = 0u;
  i2 = 0u;
  i3 = p0;
  i3 = i32_load((&memory), (u64)(i3 + 36));
  i0 = CALL_INDIRECT(T0, u32 (*)(u32, u32, u32), 1, i3, i0, i1, i2);
  B0:;
  i0 = p0;
  j1 = 0ull;
  i64_store((&memory), (u64)(i0 + 16), j1);
  i0 = p0;
  i1 = 28u;
  i0 += i1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  l1 = i0;
  i1 = 4u;
  i0 &= i1;
  if (i0) {goto B1;}
  i0 = p0;
  i1 = p0;
  i1 = i32_load((&memory), (u64)(i1 + 44));
  i2 = p0;
  i2 = i32_load((&memory), (u64)(i2 + 48));
  i1 += i2;
  l2 = i1;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = p0;
  i1 = l2;
  i32_store((&memory), (u64)(i0 + 4), i1);
  i0 = l1;
  i1 = 27u;
  i0 <<= (i1 & 31);
  i1 = 31u;
  i0 = (u32)((s32)i0 >> (i1 & 31));
  goto Bfunc;
  B1:;
  i0 = p0;
  i1 = l1;
  i2 = 32u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = 4294967295u;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f72(u32 p0) {
  u32 l1 = 0, l2 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = g0;
  i1 = 16u;
  i0 -= i1;
  l1 = i0;
  g0 = i0;
  i0 = 4294967295u;
  l2 = i0;
  i0 = p0;
  i0 = f71(i0);
  if (i0) {goto B0;}
  i0 = p0;
  i1 = l1;
  i2 = 15u;
  i1 += i2;
  i2 = 1u;
  i3 = p0;
  i3 = i32_load((&memory), (u64)(i3 + 32));
  i0 = CALL_INDIRECT(T0, u32 (*)(u32, u32, u32), 1, i3, i0, i1, i2);
  i1 = 1u;
  i0 = i0 != i1;
  if (i0) {goto B0;}
  i0 = l1;
  i0 = i32_load8_u((&memory), (u64)(i0 + 15));
  l2 = i0;
  B0:;
  i0 = l1;
  i1 = 16u;
  i0 += i1;
  g0 = i0;
  i0 = l2;
  FUNC_EPILOGUE;
  return i0;
}

static void f73(u32 p0, u64 p1) {
  u32 l2 = 0, l3 = 0;
  u64 l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  u64 j0, j1, j2;
  i0 = p0;
  j1 = p1;
  i64_store((&memory), (u64)(i0 + 112), j1);
  i0 = p0;
  i1 = p0;
  i1 = i32_load((&memory), (u64)(i1 + 8));
  l2 = i1;
  i2 = p0;
  i2 = i32_load((&memory), (u64)(i2 + 4));
  l3 = i2;
  i1 -= i2;
  j1 = (u64)(s64)(s32)(i1);
  l4 = j1;
  i64_store((&memory), (u64)(i0 + 120), j1);
  j0 = p1;
  i0 = !(j0);
  if (i0) {goto B0;}
  j0 = l4;
  j1 = p1;
  i0 = (u64)((s64)j0 <= (s64)j1);
  if (i0) {goto B0;}
  i0 = p0;
  i1 = l3;
  j2 = p1;
  i2 = (u32)(j2);
  i1 += i2;
  i32_store((&memory), (u64)(i0 + 104), i1);
  goto Bfunc;
  B0:;
  i0 = p0;
  i1 = l2;
  i32_store((&memory), (u64)(i0 + 104), i1);
  Bfunc:;
  FUNC_EPILOGUE;
}

static u32 f74(u32 p0) {
  u32 l2 = 0, l3 = 0, l4 = 0;
  u64 l1 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  u64 j0, j1, j2;
  i0 = p0;
  j0 = i64_load((&memory), (u64)(i0 + 112));
  l1 = j0;
  i0 = !(j0);
  if (i0) {goto B5;}
  i0 = p0;
  j0 = i64_load((&memory), (u64)(i0 + 120));
  j1 = l1;
  i0 = (u64)((s64)j0 >= (s64)j1);
  if (i0) {goto B4;}
  B5:;
  i0 = p0;
  i0 = f72(i0);
  l2 = i0;
  i1 = 4294967295u;
  i0 = (u32)((s32)i0 <= (s32)i1);
  if (i0) {goto B4;}
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8));
  l3 = i0;
  i0 = p0;
  i1 = 112u;
  i0 += i1;
  j0 = i64_load((&memory), (u64)(i0));
  l1 = j0;
  j1 = 0ull;
  i0 = j0 == j1;
  if (i0) {goto B6;}
  j0 = l1;
  i1 = p0;
  j1 = i64_load((&memory), (u64)(i1 + 120));
  j0 -= j1;
  l1 = j0;
  i1 = l3;
  i2 = p0;
  i2 = i32_load((&memory), (u64)(i2 + 4));
  l4 = i2;
  i1 -= i2;
  j1 = (u64)(s64)(s32)(i1);
  i0 = (u64)((s64)j0 <= (s64)j1);
  if (i0) {goto B3;}
  B6:;
  i0 = p0;
  i1 = l3;
  i32_store((&memory), (u64)(i0 + 104), i1);
  i0 = l3;
  i0 = !(i0);
  if (i0) {goto B2;}
  goto B1;
  B4:;
  i0 = p0;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 104), i1);
  i0 = 4294967295u;
  goto Bfunc;
  B3:;
  i0 = p0;
  i1 = l4;
  j2 = l1;
  i2 = (u32)(j2);
  i1 += i2;
  i2 = 4294967295u;
  i1 += i2;
  i32_store((&memory), (u64)(i0 + 104), i1);
  i0 = l3;
  if (i0) {goto B1;}
  B2:;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l3 = i0;
  goto B0;
  B1:;
  i0 = p0;
  i1 = p0;
  j1 = i64_load((&memory), (u64)(i1 + 120));
  i2 = l3;
  i3 = 1u;
  i2 += i3;
  i3 = p0;
  i4 = 4u;
  i3 += i4;
  i3 = i32_load((&memory), (u64)(i3));
  l3 = i3;
  i2 -= i3;
  j2 = (u64)(s64)(s32)(i2);
  j1 += j2;
  i64_store((&memory), (u64)(i0 + 120), j1);
  B0:;
  i0 = l2;
  i1 = l3;
  i2 = 4294967295u;
  i1 += i2;
  p0 = i1;
  i1 = i32_load8_u((&memory), (u64)(i1));
  i0 = i0 == i1;
  if (i0) {goto B7;}
  i0 = p0;
  i1 = l2;
  i32_store8((&memory), (u64)(i0), i1);
  B7:;
  i0 = l2;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u64 f75(u32 p0, u32 p1, u32 p2, u64 p3) {
  u32 l4 = 0, l5 = 0, l6 = 0, l7 = 0, l9 = 0, l14 = 0;
  u64 l8 = 0, l10 = 0, l11 = 0, l12 = 0, l13 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  u64 j0, j1, j2, j3;
  i0 = p1;
  i1 = 36u;
  i0 = i0 > i1;
  if (i0) {goto B16;}
  i0 = p1;
  i1 = 1u;
  i0 = i0 == i1;
  if (i0) {goto B16;}
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  l4 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l5 = i0;
  L17: 
    i0 = l5;
    i0 = i32_load((&memory), (u64)(i0));
    l6 = i0;
    i1 = l4;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 < i1;
    if (i0) {goto B19;}
    i0 = p0;
    i0 = f74(i0);
    l6 = i0;
    i1 = 4294967287u;
    i0 += i1;
    i1 = 5u;
    i0 = i0 >= i1;
    if (i0) {goto B18;}
    goto L17;
    B19:;
    i0 = l5;
    i1 = l6;
    i2 = 1u;
    i1 += i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l6;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l6 = i0;
    i1 = 4294967287u;
    i0 += i1;
    i1 = 5u;
    i0 = i0 < i1;
    if (i0) {goto L17;}
    B18:;
    i0 = l6;
    i1 = 32u;
    i0 = i0 == i1;
    if (i0) {goto L17;}
  i0 = l6;
  i1 = 45u;
  i0 = i0 == i1;
  l5 = i0;
  if (i0) {goto B20;}
  i0 = l6;
  i1 = 43u;
  i0 = i0 != i1;
  if (i0) {goto B15;}
  B20:;
  i0 = 4294967295u;
  i1 = 0u;
  i2 = l5;
  i0 = i2 ? i0 : i1;
  l7 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l5 = i0;
  i0 = i32_load((&memory), (u64)(i0));
  l6 = i0;
  i1 = p0;
  i2 = 104u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  i0 = i0 >= i1;
  if (i0) {goto B14;}
  i0 = l5;
  i1 = l6;
  i2 = 1u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l6;
  i0 = i32_load8_u((&memory), (u64)(i0));
  l6 = i0;
  i0 = p1;
  i1 = 16u;
  i0 |= i1;
  i1 = 16u;
  i0 = i0 != i1;
  if (i0) {goto B12;}
  goto B13;
  B16:;
  i0 = f69();
  i1 = 22u;
  i32_store((&memory), (u64)(i0), i1);
  j0 = 0ull;
  goto Bfunc;
  B15:;
  i0 = 0u;
  l7 = i0;
  i0 = p1;
  i1 = 16u;
  i0 |= i1;
  i1 = 16u;
  i0 = i0 == i1;
  if (i0) {goto B13;}
  goto B12;
  B14:;
  i0 = p0;
  i0 = f74(i0);
  l6 = i0;
  i0 = p1;
  i1 = 16u;
  i0 |= i1;
  i1 = 16u;
  i0 = i0 != i1;
  if (i0) {goto B12;}
  B13:;
  i0 = l6;
  i1 = 48u;
  i0 = i0 != i1;
  if (i0) {goto B12;}
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l5 = i0;
  i0 = i32_load((&memory), (u64)(i0));
  l6 = i0;
  i1 = p0;
  i2 = 104u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  i0 = i0 >= i1;
  if (i0) {goto B11;}
  i0 = l5;
  i1 = l6;
  i2 = 1u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l6;
  i0 = i32_load8_u((&memory), (u64)(i0));
  l6 = i0;
  goto B10;
  B12:;
  i0 = p1;
  i1 = 10u;
  i2 = p1;
  i0 = i2 ? i0 : i1;
  p1 = i0;
  i1 = l6;
  i2 = 17169u;
  i1 += i2;
  i1 = i32_load8_u((&memory), (u64)(i1));
  i0 = i0 > i1;
  if (i0) {goto B9;}
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i0 = !(i0);
  if (i0) {goto B21;}
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l6 = i0;
  i1 = l6;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 4294967295u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  B21:;
  i0 = p0;
  j1 = 0ull;
  f73(i0, j1);
  i0 = f69();
  i1 = 22u;
  i32_store((&memory), (u64)(i0), i1);
  j0 = 0ull;
  goto Bfunc;
  B11:;
  i0 = p0;
  i0 = f74(i0);
  l6 = i0;
  B10:;
  i0 = l6;
  i1 = 32u;
  i0 |= i1;
  i1 = 120u;
  i0 = i0 != i1;
  if (i0) {goto B22;}
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l5 = i0;
  i0 = i32_load((&memory), (u64)(i0));
  l6 = i0;
  i1 = p0;
  i2 = 104u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  i0 = i0 >= i1;
  if (i0) {goto B8;}
  i0 = l5;
  i1 = l6;
  i2 = 1u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l6;
  i0 = i32_load8_u((&memory), (u64)(i0));
  l6 = i0;
  goto B7;
  B22:;
  i0 = p1;
  i0 = !(i0);
  if (i0) {goto B6;}
  B9:;
  i0 = p1;
  i1 = 10u;
  i0 = i0 != i1;
  if (i0) {goto B5;}
  j0 = 0ull;
  l8 = j0;
  i0 = l6;
  i1 = 4294967248u;
  i0 += i1;
  l4 = i0;
  i1 = 9u;
  i0 = i0 > i1;
  if (i0) {goto B1;}
  i0 = 0u;
  l5 = i0;
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  l9 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  p2 = i0;
  L24: 
    i0 = l5;
    i1 = 10u;
    i0 *= i1;
    l6 = i0;
    i0 = p2;
    i0 = i32_load((&memory), (u64)(i0));
    p1 = i0;
    i1 = l9;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 >= i1;
    if (i0) {goto B26;}
    i0 = p2;
    i1 = p1;
    i2 = 1u;
    i1 += i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l6;
    i1 = l4;
    i0 += i1;
    l5 = i0;
    i0 = p1;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l6 = i0;
    i1 = 4294967248u;
    i0 += i1;
    l4 = i0;
    i1 = 9u;
    i0 = i0 <= i1;
    if (i0) {goto B25;}
    goto B23;
    B26:;
    i0 = l6;
    i1 = l4;
    i0 += i1;
    l5 = i0;
    i0 = p0;
    i0 = f74(i0);
    l6 = i0;
    i1 = 4294967248u;
    i0 += i1;
    l4 = i0;
    i1 = 9u;
    i0 = i0 > i1;
    if (i0) {goto B23;}
    B25:;
    i0 = l5;
    i1 = 429496729u;
    i0 = i0 < i1;
    if (i0) {goto L24;}
  B23:;
  i0 = l5;
  j0 = (u64)(i0);
  l8 = j0;
  i0 = l4;
  i1 = 9u;
  i0 = i0 > i1;
  if (i0) {goto B1;}
  i0 = 10u;
  p1 = i0;
  j0 = l8;
  j1 = 10ull;
  j0 *= j1;
  l10 = j0;
  i1 = l4;
  j1 = (u64)(s64)(s32)(i1);
  l11 = j1;
  j2 = 18446744073709551615ull;
  j1 ^= j2;
  i0 = j0 > j1;
  if (i0) {goto B2;}
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  p2 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l4 = i0;
  L27: 
    i0 = l4;
    i0 = i32_load((&memory), (u64)(i0));
    l6 = i0;
    i1 = p2;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 >= i1;
    if (i0) {goto B29;}
    i0 = l4;
    i1 = l6;
    i2 = 1u;
    i1 += i2;
    i32_store((&memory), (u64)(i0), i1);
    j0 = l10;
    j1 = l11;
    j0 += j1;
    l8 = j0;
    i0 = l6;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l6 = i0;
    i1 = 4294967248u;
    i0 += i1;
    l5 = i0;
    i1 = 9u;
    i0 = i0 <= i1;
    if (i0) {goto B28;}
    goto B3;
    B29:;
    j0 = l10;
    j1 = l11;
    j0 += j1;
    l8 = j0;
    i0 = p0;
    i0 = f74(i0);
    l6 = i0;
    i1 = 4294967248u;
    i0 += i1;
    l5 = i0;
    i1 = 9u;
    i0 = i0 > i1;
    if (i0) {goto B3;}
    B28:;
    j0 = l8;
    j1 = 1844674407370955162ull;
    i0 = j0 >= j1;
    if (i0) {goto B3;}
    j0 = l8;
    j1 = 10ull;
    j0 *= j1;
    l10 = j0;
    i1 = l5;
    j1 = (u64)(s64)(s32)(i1);
    l11 = j1;
    j2 = 18446744073709551615ull;
    j1 ^= j2;
    i0 = j0 <= j1;
    if (i0) {goto L27;}
    goto B2;
  B8:;
  i0 = p0;
  i0 = f74(i0);
  l6 = i0;
  B7:;
  i0 = 16u;
  p1 = i0;
  i0 = l6;
  i1 = 17169u;
  i0 += i1;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = 16u;
  i0 = i0 < i1;
  if (i0) {goto B5;}
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l6 = i0;
  i0 = !(i0);
  if (i0) {goto B30;}
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l5 = i0;
  i1 = l5;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 4294967295u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  B30:;
  i0 = p2;
  i0 = !(i0);
  if (i0) {goto B4;}
  j0 = 0ull;
  l8 = j0;
  i0 = l6;
  i0 = !(i0);
  if (i0) {goto B0;}
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l6 = i0;
  i1 = l6;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 4294967295u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  j0 = 0ull;
  goto Bfunc;
  B6:;
  i0 = 8u;
  p1 = i0;
  B5:;
  i0 = p1;
  i1 = 4294967295u;
  i0 += i1;
  i1 = p1;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B31;}
  j0 = 0ull;
  l8 = j0;
  i0 = p1;
  i1 = l6;
  i2 = 17169u;
  i1 += i2;
  i1 = i32_load8_u((&memory), (u64)(i1));
  l5 = i1;
  i0 = i0 <= i1;
  if (i0) {goto B32;}
  i0 = 0u;
  l4 = i0;
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  l9 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  p2 = i0;
  L34: 
    i0 = l5;
    i1 = l4;
    i2 = p1;
    i1 *= i2;
    i0 += i1;
    l4 = i0;
    i0 = p2;
    i0 = i32_load((&memory), (u64)(i0));
    l6 = i0;
    i1 = l9;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 >= i1;
    if (i0) {goto B36;}
    i0 = p2;
    i1 = l6;
    i2 = 1u;
    i1 += i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l6;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l6 = i0;
    i1 = 17169u;
    i0 += i1;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l5 = i0;
    i0 = l4;
    i1 = 119304646u;
    i0 = i0 <= i1;
    if (i0) {goto B35;}
    goto B33;
    B36:;
    i0 = p0;
    i0 = f74(i0);
    l6 = i0;
    i1 = 17169u;
    i0 += i1;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l5 = i0;
    i0 = l4;
    i1 = 119304646u;
    i0 = i0 > i1;
    if (i0) {goto B33;}
    B35:;
    i0 = p1;
    i1 = l5;
    i0 = i0 > i1;
    if (i0) {goto L34;}
  B33:;
  i0 = l4;
  j0 = (u64)(i0);
  l8 = j0;
  B32:;
  i0 = p1;
  i1 = l5;
  i0 = i0 <= i1;
  if (i0) {goto B2;}
  j0 = l8;
  j1 = 18446744073709551615ull;
  i2 = p1;
  j2 = (u64)(i2);
  l12 = j2;
  j1 = DIV_U(j1, j2);
  l13 = j1;
  i0 = j0 > j1;
  if (i0) {goto B2;}
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  p2 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l4 = i0;
  L37: 
    j0 = l8;
    j1 = l12;
    j0 *= j1;
    l10 = j0;
    i1 = l5;
    j1 = (u64)(i1);
    j2 = 255ull;
    j1 &= j2;
    l11 = j1;
    j2 = 18446744073709551615ull;
    j1 ^= j2;
    i0 = j0 > j1;
    if (i0) {goto B2;}
    i0 = l4;
    i0 = i32_load((&memory), (u64)(i0));
    l6 = i0;
    i1 = p2;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 >= i1;
    if (i0) {goto B39;}
    i0 = l4;
    i1 = l6;
    i2 = 1u;
    i1 += i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l6;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l6 = i0;
    goto B38;
    B39:;
    i0 = p0;
    i0 = f74(i0);
    l6 = i0;
    B38:;
    j0 = l10;
    j1 = l11;
    j0 += j1;
    l8 = j0;
    i0 = p1;
    i1 = l6;
    i2 = 17169u;
    i1 += i2;
    i1 = i32_load8_u((&memory), (u64)(i1));
    l5 = i1;
    i0 = i0 <= i1;
    if (i0) {goto B2;}
    j0 = l8;
    j1 = l13;
    i0 = j0 <= j1;
    if (i0) {goto L37;}
    goto B2;
  B31:;
  i0 = p1;
  i1 = 23u;
  i0 *= i1;
  i1 = 5u;
  i0 >>= (i1 & 31);
  i1 = 7u;
  i0 &= i1;
  i1 = 8204u;
  i0 += i1;
  i0 = i32_load8_s((&memory), (u64)(i0));
  l9 = i0;
  j0 = 0ull;
  l8 = j0;
  i0 = p1;
  i1 = l6;
  i2 = 17169u;
  i1 += i2;
  i1 = i32_load8_u((&memory), (u64)(i1));
  l5 = i1;
  i0 = i0 <= i1;
  if (i0) {goto B40;}
  i0 = 0u;
  l4 = i0;
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  l14 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  p2 = i0;
  L42: 
    i0 = l5;
    i1 = l4;
    i2 = l9;
    i1 <<= (i2 & 31);
    i0 |= i1;
    l4 = i0;
    i0 = p2;
    i0 = i32_load((&memory), (u64)(i0));
    l6 = i0;
    i1 = l14;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 >= i1;
    if (i0) {goto B44;}
    i0 = p2;
    i1 = l6;
    i2 = 1u;
    i1 += i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l6;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l6 = i0;
    i1 = 17169u;
    i0 += i1;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l5 = i0;
    i0 = l4;
    i1 = 134217727u;
    i0 = i0 <= i1;
    if (i0) {goto B43;}
    goto B41;
    B44:;
    i0 = p0;
    i0 = f74(i0);
    l6 = i0;
    i1 = 17169u;
    i0 += i1;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l5 = i0;
    i0 = l4;
    i1 = 134217727u;
    i0 = i0 > i1;
    if (i0) {goto B41;}
    B43:;
    i0 = p1;
    i1 = l5;
    i0 = i0 > i1;
    if (i0) {goto L42;}
  B41:;
  i0 = l4;
  j0 = (u64)(i0);
  l8 = j0;
  B40:;
  i0 = p1;
  i1 = l5;
  i0 = i0 <= i1;
  if (i0) {goto B2;}
  j0 = 18446744073709551615ull;
  i1 = l9;
  j1 = (u64)(i1);
  l11 = j1;
  j0 >>= (j1 & 63);
  l12 = j0;
  j1 = l8;
  i0 = j0 < j1;
  if (i0) {goto B2;}
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  p2 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l4 = i0;
  L45: 
    j0 = l8;
    j1 = l11;
    j0 <<= (j1 & 63);
    l8 = j0;
    i0 = l5;
    j0 = (u64)(i0);
    j1 = 255ull;
    j0 &= j1;
    l10 = j0;
    i0 = l4;
    i0 = i32_load((&memory), (u64)(i0));
    l6 = i0;
    i1 = p2;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 >= i1;
    if (i0) {goto B47;}
    i0 = l4;
    i1 = l6;
    i2 = 1u;
    i1 += i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l6;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l6 = i0;
    goto B46;
    B47:;
    i0 = p0;
    i0 = f74(i0);
    l6 = i0;
    B46:;
    j0 = l8;
    j1 = l10;
    j0 |= j1;
    l8 = j0;
    i0 = p1;
    i1 = l6;
    i2 = 17169u;
    i1 += i2;
    i1 = i32_load8_u((&memory), (u64)(i1));
    l5 = i1;
    i0 = i0 <= i1;
    if (i0) {goto B2;}
    j0 = l8;
    j1 = l12;
    i0 = j0 <= j1;
    if (i0) {goto L45;}
    goto B2;
  B4:;
  i0 = p0;
  j1 = 0ull;
  f73(i0, j1);
  j0 = 0ull;
  goto Bfunc;
  B3:;
  i0 = l5;
  i1 = 9u;
  i0 = i0 > i1;
  if (i0) {goto B1;}
  B2:;
  i0 = p1;
  i1 = l6;
  i2 = 17169u;
  i1 += i2;
  i1 = i32_load8_u((&memory), (u64)(i1));
  i0 = i0 <= i1;
  if (i0) {goto B1;}
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  l4 = i0;
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l5 = i0;
  L49: 
    i0 = l5;
    i0 = i32_load((&memory), (u64)(i0));
    l6 = i0;
    i1 = l4;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 >= i1;
    if (i0) {goto B50;}
    i0 = l5;
    i1 = l6;
    i2 = 1u;
    i1 += i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = p1;
    i1 = l6;
    i1 = i32_load8_u((&memory), (u64)(i1));
    i2 = 17169u;
    i1 += i2;
    i1 = i32_load8_u((&memory), (u64)(i1));
    i0 = i0 > i1;
    if (i0) {goto L49;}
    goto B48;
    B50:;
    i0 = p1;
    i1 = p0;
    i1 = f74(i1);
    i2 = 17169u;
    i1 += i2;
    i1 = i32_load8_u((&memory), (u64)(i1));
    i0 = i0 > i1;
    if (i0) {goto L49;}
  B48:;
  i0 = f69();
  i1 = 34u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l7;
  i1 = 0u;
  j2 = p3;
  j3 = 1ull;
  j2 &= j3;
  i2 = !(j2);
  i0 = i2 ? i0 : i1;
  l7 = i0;
  j0 = p3;
  l8 = j0;
  B1:;
  i0 = p0;
  i1 = 104u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i0 = !(i0);
  if (i0) {goto B51;}
  i0 = p0;
  i1 = 4u;
  i0 += i1;
  l6 = i0;
  i1 = l6;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 4294967295u;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  B51:;
  j0 = l8;
  j1 = p3;
  i0 = j0 < j1;
  if (i0) {goto B52;}
  j0 = p3;
  j1 = 1ull;
  j0 &= j1;
  i0 = (u32)(j0);
  if (i0) {goto B53;}
  i0 = l7;
  if (i0) {goto B53;}
  i0 = f69();
  i1 = 34u;
  i32_store((&memory), (u64)(i0), i1);
  j0 = p3;
  j1 = 18446744073709551615ull;
  j0 += j1;
  goto Bfunc;
  B53:;
  j0 = l8;
  j1 = p3;
  i0 = j0 <= j1;
  if (i0) {goto B52;}
  i0 = f69();
  i1 = 34u;
  i32_store((&memory), (u64)(i0), i1);
  j0 = p3;
  goto Bfunc;
  B52:;
  j0 = l8;
  i1 = l7;
  j1 = (u64)(s64)(s32)(i1);
  l10 = j1;
  j0 ^= j1;
  j1 = l10;
  j0 -= j1;
  l8 = j0;
  B0:;
  j0 = l8;
  Bfunc:;
  FUNC_EPILOGUE;
  return j0;
}

static u64 f76(u32 p0, u32 p1, u32 p2) {
  u32 l3 = 0;
  u64 l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  u64 j0, j1, j3;
  i0 = g0;
  i1 = 144u;
  i0 -= i1;
  l3 = i0;
  g0 = i0;
  i0 = l3;
  i1 = p0;
  i32_store((&memory), (u64)(i0 + 4), i1);
  i0 = l3;
  i1 = p0;
  i32_store((&memory), (u64)(i0 + 44), i1);
  i0 = l3;
  i1 = 0u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = 4294967295u;
  i32_store((&memory), (u64)(i0 + 76), i1);
  i0 = l3;
  i1 = 4294967295u;
  i2 = p0;
  i3 = 2147483647u;
  i2 += i3;
  i3 = p0;
  i4 = 0u;
  i3 = (u32)((s32)i3 < (s32)i4);
  i1 = i3 ? i1 : i2;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = l3;
  j1 = 0ull;
  f73(i0, j1);
  i0 = l3;
  i1 = p2;
  i2 = 1u;
  j3 = 18446744073709551615ull;
  j0 = f75(i0, i1, i2, j3);
  l4 = j0;
  i0 = p1;
  i0 = !(i0);
  if (i0) {goto B0;}
  i0 = p1;
  i1 = p0;
  i2 = l3;
  i2 = i32_load((&memory), (u64)(i2 + 4));
  i3 = l3;
  i3 = i32_load((&memory), (u64)(i3 + 120));
  i2 += i3;
  i3 = l3;
  i4 = 8u;
  i3 += i4;
  i3 = i32_load((&memory), (u64)(i3));
  i2 -= i3;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  B0:;
  i0 = l3;
  i1 = 144u;
  i0 += i1;
  g0 = i0;
  j0 = l4;
  FUNC_EPILOGUE;
  return j0;
}

static u32 f77(u32 p0, u32 p1, u32 p2) {
  u32 l3 = 0, l4 = 0, l5 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p2;
  i1 = 0u;
  i0 = i0 != i1;
  l3 = i0;
  i0 = p2;
  i0 = !(i0);
  if (i0) {goto B4;}
  i0 = p0;
  i1 = 3u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B4;}
  i0 = p1;
  i1 = 255u;
  i0 &= i1;
  l3 = i0;
  L5: 
    i0 = p0;
    i0 = i32_load8_u((&memory), (u64)(i0));
    i1 = l3;
    i0 = i0 == i1;
    if (i0) {goto B3;}
    i0 = p2;
    i1 = 1u;
    i0 = i0 != i1;
    l4 = i0;
    i0 = p2;
    i1 = 4294967295u;
    i0 += i1;
    l5 = i0;
    i0 = p0;
    i1 = 1u;
    i0 += i1;
    p0 = i0;
    i0 = p2;
    i1 = 1u;
    i0 = i0 == i1;
    if (i0) {goto B6;}
    i0 = l5;
    p2 = i0;
    i0 = p0;
    i1 = 3u;
    i0 &= i1;
    if (i0) {goto L5;}
    B6:;
  i0 = l4;
  if (i0) {goto B2;}
  goto B1;
  B4:;
  i0 = p2;
  l5 = i0;
  i0 = l3;
  if (i0) {goto B2;}
  goto B1;
  B3:;
  i0 = p2;
  l5 = i0;
  B2:;
  i0 = p0;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i1 = p1;
  i2 = 255u;
  i1 &= i2;
  i0 = i0 != i1;
  if (i0) {goto B7;}
  i0 = l5;
  if (i0) {goto B0;}
  goto B1;
  B7:;
  i0 = l5;
  i1 = 4u;
  i0 = i0 < i1;
  if (i0) {goto B9;}
  i0 = p1;
  i1 = 255u;
  i0 &= i1;
  i1 = 16843009u;
  i0 *= i1;
  l3 = i0;
  L10: 
    i0 = p0;
    i0 = i32_load((&memory), (u64)(i0));
    i1 = l3;
    i0 ^= i1;
    p2 = i0;
    i1 = 4294967295u;
    i0 ^= i1;
    i1 = p2;
    i2 = 4278124287u;
    i1 += i2;
    i0 &= i1;
    i1 = 2155905152u;
    i0 &= i1;
    if (i0) {goto B8;}
    i0 = p0;
    i1 = 4u;
    i0 += i1;
    p0 = i0;
    i0 = l5;
    i1 = 4294967292u;
    i0 += i1;
    l5 = i0;
    i1 = 3u;
    i0 = i0 > i1;
    if (i0) {goto L10;}
  B9:;
  i0 = l5;
  i0 = !(i0);
  if (i0) {goto B1;}
  B8:;
  i0 = p1;
  i1 = 255u;
  i0 &= i1;
  p2 = i0;
  L11: 
    i0 = p0;
    i0 = i32_load8_u((&memory), (u64)(i0));
    i1 = p2;
    i0 = i0 == i1;
    if (i0) {goto B0;}
    i0 = p0;
    i1 = 1u;
    i0 += i1;
    p0 = i0;
    i0 = l5;
    i1 = 4294967295u;
    i0 += i1;
    l5 = i0;
    if (i0) {goto L11;}
  B1:;
  i0 = 0u;
  p0 = i0;
  B0:;
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f78(u32 p0, u32 p1, u32 p2) {
  u32 l3 = 0, l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = p2;
  i0 = !(i0);
  if (i0) {goto B1;}
  L2: 
    i0 = p0;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l3 = i0;
    i1 = p1;
    i1 = i32_load8_u((&memory), (u64)(i1));
    l4 = i1;
    i0 = i0 != i1;
    if (i0) {goto B0;}
    i0 = p1;
    i1 = 1u;
    i0 += i1;
    p1 = i0;
    i0 = p0;
    i1 = 1u;
    i0 += i1;
    p0 = i0;
    i0 = p2;
    i1 = 4294967295u;
    i0 += i1;
    p2 = i0;
    if (i0) {goto L2;}
  B1:;
  i0 = 0u;
  goto Bfunc;
  B0:;
  i0 = l3;
  i1 = l4;
  i0 -= i1;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f79(u32 p0) {
  u32 l1 = 0, l2 = 0, l3 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p0;
  l1 = i0;
  i0 = p0;
  i1 = 3u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B2;}
  i0 = p0;
  i0 = i32_load8_u((&memory), (u64)(i0));
  i0 = !(i0);
  if (i0) {goto B1;}
  i0 = p0;
  i1 = 1u;
  i0 += i1;
  l1 = i0;
  L3: 
    i0 = l1;
    i1 = 3u;
    i0 &= i1;
    i0 = !(i0);
    if (i0) {goto B2;}
    i0 = l1;
    i0 = i32_load8_u((&memory), (u64)(i0));
    l2 = i0;
    i0 = l1;
    i1 = 1u;
    i0 += i1;
    l3 = i0;
    l1 = i0;
    i0 = l2;
    if (i0) {goto L3;}
  i0 = l3;
  i1 = 4294967295u;
  i0 += i1;
  i1 = p0;
  i0 -= i1;
  goto Bfunc;
  B2:;
  i0 = l1;
  i1 = 4294967292u;
  i0 += i1;
  l1 = i0;
  L4: 
    i0 = l1;
    i1 = 4u;
    i0 += i1;
    l1 = i0;
    i0 = i32_load((&memory), (u64)(i0));
    l2 = i0;
    i1 = 4294967295u;
    i0 ^= i1;
    i1 = l2;
    i2 = 4278124287u;
    i1 += i2;
    i0 &= i1;
    i1 = 2155905152u;
    i0 &= i1;
    i0 = !(i0);
    if (i0) {goto L4;}
  i0 = l2;
  i1 = 255u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {goto B0;}
  L5: 
    i0 = l1;
    i0 = i32_load8_u((&memory), (u64)(i0 + 1));
    l2 = i0;
    i0 = l1;
    i1 = 1u;
    i0 += i1;
    l3 = i0;
    l1 = i0;
    i0 = l2;
    if (i0) {goto L5;}
  i0 = l3;
  i1 = p0;
  i0 -= i1;
  goto Bfunc;
  B1:;
  i0 = p0;
  i1 = p0;
  i0 -= i1;
  goto Bfunc;
  B0:;
  i0 = l1;
  i1 = p0;
  i0 -= i1;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f80(u32 p0, u32 p1, u32 p2) {
  u32 l3 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = 22u;
  l3 = i0;
  i0 = p1;
  i1 = 4u;
  i0 = i0 < i1;
  if (i0) {goto B1;}
  i0 = p1;
  i1 = p2;
  i0 = f81(i0, i1);
  p1 = i0;
  i0 = !(i0);
  if (i0) {goto B0;}
  i0 = p0;
  i1 = p1;
  i32_store((&memory), (u64)(i0), i1);
  i0 = 0u;
  l3 = i0;
  B1:;
  i0 = l3;
  goto Bfunc;
  B0:;
  i0 = f69();
  i0 = i32_load((&memory), (u64)(i0));
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f81(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = 0u;
  l2 = i0;
  i0 = 0u;
  i1 = p0;
  i0 -= i1;
  l3 = i0;
  i1 = p0;
  i0 &= i1;
  i1 = p0;
  i0 = i0 != i1;
  if (i0) {goto B1;}
  i0 = p0;
  i1 = 16u;
  i0 = i0 > i1;
  if (i0) {goto B0;}
  i0 = p1;
  i0 = f82(i0);
  goto Bfunc;
  B1:;
  i0 = f69();
  i1 = 22u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = 0u;
  goto Bfunc;
  B0:;
  i0 = p0;
  i1 = 4294967295u;
  i0 += i1;
  l4 = i0;
  i1 = p1;
  i0 += i1;
  i0 = f82(i0);
  p0 = i0;
  i0 = !(i0);
  if (i0) {goto B4;}
  i0 = p0;
  i1 = l4;
  i2 = p0;
  i1 += i2;
  i2 = l3;
  i1 &= i2;
  l2 = i1;
  i0 = i0 == i1;
  if (i0) {goto B3;}
  i0 = p0;
  i1 = 4294967292u;
  i0 += i1;
  l3 = i0;
  i0 = i32_load((&memory), (u64)(i0));
  l4 = i0;
  i1 = 7u;
  i0 &= i1;
  p1 = i0;
  i0 = !(i0);
  if (i0) {goto B2;}
  i0 = p0;
  i1 = l4;
  i2 = 4294967288u;
  i1 &= i2;
  i0 += i1;
  l4 = i0;
  i1 = 4294967288u;
  i0 += i1;
  l5 = i0;
  i0 = i32_load((&memory), (u64)(i0));
  l6 = i0;
  i0 = l3;
  i1 = p1;
  i2 = l2;
  i3 = p0;
  i2 -= i3;
  l7 = i2;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = 4294967292u;
  i0 += i1;
  i1 = l4;
  i2 = l2;
  i1 -= i2;
  l3 = i1;
  i2 = p1;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = 4294967288u;
  i0 += i1;
  i1 = l6;
  i2 = 7u;
  i1 &= i2;
  p1 = i1;
  i2 = l7;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l5;
  i1 = p1;
  i2 = l3;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  f85(i0);
  B4:;
  i0 = l2;
  goto Bfunc;
  B3:;
  i0 = p0;
  goto Bfunc;
  B2:;
  i0 = l2;
  i1 = 4294967288u;
  i0 += i1;
  i1 = p0;
  i2 = 4294967288u;
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = l2;
  i3 = p0;
  i2 -= i3;
  p0 = i2;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = 4294967292u;
  i0 += i1;
  i1 = l3;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = p0;
  i1 -= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f82(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = 8748u;
  i1 = p0;
  i0 = f83(i0, i1);
  FUNC_EPILOGUE;
  return i0;
}

static u32 f83(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0, l8 = 0, l9 = 0, 
      l10 = 0, l11 = 0, l12 = 0, l13 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  i0 = p1;
  i0 = !(i0);
  if (i0) {goto B0;}
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8384));
  l2 = i0;
  if (i0) {goto B1;}
  i0 = 16u;
  l2 = i0;
  i0 = p0;
  i1 = 8384u;
  i0 += i1;
  i1 = 16u;
  i32_store((&memory), (u64)(i0), i1);
  B1:;
  i0 = p1;
  i1 = 8u;
  i0 += i1;
  i1 = p1;
  i2 = 4u;
  i1 += i2;
  i2 = 7u;
  i1 &= i2;
  l3 = i1;
  i0 -= i1;
  i1 = p1;
  i2 = l3;
  i0 = i2 ? i0 : i1;
  l3 = i0;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8388));
  l4 = i0;
  i1 = l2;
  i0 = i0 >= i1;
  if (i0) {goto B4;}
  i0 = p0;
  i1 = l4;
  i2 = 12u;
  i1 *= i2;
  i0 += i1;
  i1 = 8192u;
  i0 += i1;
  p1 = i0;
  i0 = l4;
  if (i0) {goto B5;}
  i0 = p0;
  i1 = 8196u;
  i0 += i1;
  l2 = i0;
  i0 = i32_load((&memory), (u64)(i0));
  if (i0) {goto B5;}
  i0 = p1;
  i1 = 8192u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = p0;
  i32_store((&memory), (u64)(i0), i1);
  B5:;
  i0 = l3;
  i1 = 4u;
  i0 += i1;
  l4 = i0;
  L6: 
    i0 = p1;
    i0 = i32_load((&memory), (u64)(i0 + 8));
    l2 = i0;
    i1 = l4;
    i0 += i1;
    i1 = p1;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 > i1;
    if (i0) {goto B7;}
    i0 = p1;
    i0 = i32_load((&memory), (u64)(i0 + 4));
    i1 = l2;
    i0 += i1;
    l2 = i0;
    i1 = l2;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 2147483648u;
    i1 &= i2;
    i2 = l3;
    i1 |= i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = p1;
    i1 = 8u;
    i0 += i1;
    p1 = i0;
    i1 = p1;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = l4;
    i1 += i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l2;
    i1 = l2;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 2147483648u;
    i1 |= i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l2;
    i1 = 4u;
    i0 += i1;
    p1 = i0;
    if (i0) {goto B3;}
    B7:;
    i0 = p0;
    i0 = f84(i0);
    p1 = i0;
    if (i0) {goto L6;}
  B4:;
  i0 = 2147483644u;
  i1 = l3;
  i0 -= i1;
  l5 = i0;
  i0 = p0;
  i1 = 8392u;
  i0 += i1;
  l6 = i0;
  i0 = p0;
  i1 = 8384u;
  i0 += i1;
  l7 = i0;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8392));
  l8 = i0;
  l2 = i0;
  L8: 
    i0 = p0;
    i1 = l2;
    i2 = 12u;
    i1 *= i2;
    i0 += i1;
    p1 = i0;
    i1 = 8200u;
    i0 += i1;
    i0 = i32_load((&memory), (u64)(i0));
    i1 = p1;
    i2 = 8192u;
    i1 += i2;
    l9 = i1;
    i1 = i32_load((&memory), (u64)(i1));
    i0 = i0 == i1;
    if (i0) {goto B9;}
    i0 = 0u;
    i1 = 8222u;
    (*Z_envZ_eosio_assertZ_vii)(i0, i1);
    B9:;
    i0 = p1;
    i1 = 8196u;
    i0 += i1;
    i0 = i32_load((&memory), (u64)(i0));
    l10 = i0;
    i1 = 4u;
    i0 += i1;
    l2 = i0;
    L10: 
      i0 = l10;
      i1 = l9;
      i1 = i32_load((&memory), (u64)(i1));
      i0 += i1;
      l11 = i0;
      i0 = l2;
      i1 = 4294967292u;
      i0 += i1;
      l12 = i0;
      i0 = i32_load((&memory), (u64)(i0));
      l13 = i0;
      i1 = 2147483647u;
      i0 &= i1;
      p1 = i0;
      i0 = l13;
      i1 = 0u;
      i0 = (u32)((s32)i0 < (s32)i1);
      if (i0) {goto B11;}
      i0 = p1;
      i1 = l3;
      i0 = i0 >= i1;
      if (i0) {goto B12;}
      L13: 
        i0 = l2;
        i1 = p1;
        i0 += i1;
        l4 = i0;
        i1 = l11;
        i0 = i0 >= i1;
        if (i0) {goto B12;}
        i0 = l4;
        i0 = i32_load((&memory), (u64)(i0));
        l4 = i0;
        i1 = 0u;
        i0 = (u32)((s32)i0 < (s32)i1);
        if (i0) {goto B12;}
        i0 = p1;
        i1 = l4;
        i2 = 2147483647u;
        i1 &= i2;
        i0 += i1;
        i1 = 4u;
        i0 += i1;
        p1 = i0;
        i1 = l3;
        i0 = i0 < i1;
        if (i0) {goto L13;}
      B12:;
      i0 = l12;
      i1 = p1;
      i2 = l3;
      i3 = p1;
      i4 = l3;
      i3 = i3 < i4;
      i1 = i3 ? i1 : i2;
      i2 = l13;
      i3 = 2147483648u;
      i2 &= i3;
      i1 |= i2;
      i32_store((&memory), (u64)(i0), i1);
      i0 = p1;
      i1 = l3;
      i0 = i0 <= i1;
      if (i0) {goto B14;}
      i0 = l2;
      i1 = l3;
      i0 += i1;
      i1 = l5;
      i2 = p1;
      i1 += i2;
      i2 = 2147483647u;
      i1 &= i2;
      i32_store((&memory), (u64)(i0), i1);
      B14:;
      i0 = p1;
      i1 = l3;
      i0 = i0 >= i1;
      if (i0) {goto B2;}
      B11:;
      i0 = l2;
      i1 = p1;
      i0 += i1;
      i1 = 4u;
      i0 += i1;
      l2 = i0;
      i1 = l11;
      i0 = i0 < i1;
      if (i0) {goto L10;}
    i0 = 0u;
    p1 = i0;
    i0 = l6;
    i1 = 0u;
    i2 = l6;
    i2 = i32_load((&memory), (u64)(i2));
    i3 = 1u;
    i2 += i3;
    l2 = i2;
    i3 = l2;
    i4 = l7;
    i4 = i32_load((&memory), (u64)(i4));
    i3 = i3 == i4;
    i1 = i3 ? i1 : i2;
    l2 = i1;
    i32_store((&memory), (u64)(i0), i1);
    i0 = l2;
    i1 = l8;
    i0 = i0 != i1;
    if (i0) {goto L8;}
  B3:;
  i0 = p1;
  goto Bfunc;
  B2:;
  i0 = l12;
  i1 = l12;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 2147483648u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  goto Bfunc;
  B0:;
  i0 = 0u;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f84(u32 p0) {
  u32 l1 = 0, l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0, l8 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 8388));
  l1 = i0;
  i0 = 0u;
  i0 = i32_load8_u((&memory), (u64)(i0 + 8740));
  i0 = !(i0);
  if (i0) {goto B1;}
  i0 = 0u;
  i0 = i32_load((&memory), (u64)(i0 + 8744));
  l2 = i0;
  goto B0;
  B1:;
  i0 = memory.pages;
  l2 = i0;
  i0 = 0u;
  i1 = 1u;
  i32_store8((&memory), (u64)(i0 + 8740), i1);
  i0 = 0u;
  i1 = l2;
  i2 = 16u;
  i1 <<= (i2 & 31);
  l2 = i1;
  i32_store((&memory), (u64)(i0 + 8744), i1);
  B0:;
  i0 = l2;
  l3 = i0;
  i0 = l2;
  i1 = 65535u;
  i0 += i1;
  i1 = 16u;
  i0 >>= (i1 & 31);
  l4 = i0;
  i1 = memory.pages;
  l5 = i1;
  i0 = i0 <= i1;
  if (i0) {goto B5;}
  i0 = l4;
  i1 = l5;
  i0 -= i1;
  i0 = wasm_rt_grow_memory((&memory), i0);
  i0 = 0u;
  l5 = i0;
  i0 = l4;
  i1 = memory.pages;
  i0 = i0 != i1;
  if (i0) {goto B4;}
  i0 = 0u;
  i0 = i32_load((&memory), (u64)(i0 + 8744));
  l3 = i0;
  B5:;
  i0 = 0u;
  l5 = i0;
  i0 = 0u;
  i1 = l3;
  i32_store((&memory), (u64)(i0 + 8744), i1);
  i0 = l2;
  i1 = 0u;
  i0 = (u32)((s32)i0 < (s32)i1);
  if (i0) {goto B4;}
  i0 = l1;
  i1 = 12u;
  i0 *= i1;
  l4 = i0;
  i0 = l2;
  i1 = 65535u;
  i0 &= i1;
  l5 = i0;
  i1 = 64512u;
  i0 = i0 > i1;
  if (i0) {goto B7;}
  i0 = l2;
  i1 = 65536u;
  i0 += i1;
  i1 = l5;
  i0 -= i1;
  l5 = i0;
  goto B6;
  B7:;
  i0 = l2;
  i1 = 131072u;
  i0 += i1;
  i1 = l2;
  i2 = 131071u;
  i1 &= i2;
  i0 -= i1;
  l5 = i0;
  B6:;
  i0 = p0;
  i1 = l4;
  i0 += i1;
  l4 = i0;
  i0 = l5;
  i1 = l2;
  i0 -= i1;
  l2 = i0;
  i0 = 0u;
  i0 = i32_load8_u((&memory), (u64)(i0 + 8740));
  if (i0) {goto B8;}
  i0 = memory.pages;
  l3 = i0;
  i0 = 0u;
  i1 = 1u;
  i32_store8((&memory), (u64)(i0 + 8740), i1);
  i0 = 0u;
  i1 = l3;
  i2 = 16u;
  i1 <<= (i2 & 31);
  l3 = i1;
  i32_store((&memory), (u64)(i0 + 8744), i1);
  B8:;
  i0 = l4;
  i1 = 8192u;
  i0 += i1;
  l4 = i0;
  i0 = l2;
  i1 = 0u;
  i0 = (u32)((s32)i0 < (s32)i1);
  if (i0) {goto B3;}
  i0 = l3;
  l6 = i0;
  i0 = l2;
  i1 = 7u;
  i0 += i1;
  i1 = 4294967288u;
  i0 &= i1;
  l7 = i0;
  i1 = l3;
  i0 += i1;
  i1 = 65535u;
  i0 += i1;
  i1 = 16u;
  i0 >>= (i1 & 31);
  l5 = i0;
  i1 = memory.pages;
  l8 = i1;
  i0 = i0 <= i1;
  if (i0) {goto B9;}
  i0 = l5;
  i1 = l8;
  i0 -= i1;
  i0 = wasm_rt_grow_memory((&memory), i0);
  i0 = l5;
  i1 = memory.pages;
  i0 = i0 != i1;
  if (i0) {goto B3;}
  i0 = 0u;
  i0 = i32_load((&memory), (u64)(i0 + 8744));
  l6 = i0;
  B9:;
  i0 = 0u;
  i1 = l6;
  i2 = l7;
  i1 += i2;
  i32_store((&memory), (u64)(i0 + 8744), i1);
  i0 = l3;
  i1 = 4294967295u;
  i0 = i0 == i1;
  if (i0) {goto B3;}
  i0 = p0;
  i1 = l1;
  i2 = 12u;
  i1 *= i2;
  i0 += i1;
  l1 = i0;
  i1 = 8196u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  l6 = i0;
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1));
  l5 = i1;
  i0 += i1;
  i1 = l3;
  i0 = i0 == i1;
  if (i0) {goto B2;}
  i0 = l5;
  i1 = l1;
  i2 = 8200u;
  i1 += i2;
  l7 = i1;
  i1 = i32_load((&memory), (u64)(i1));
  l1 = i1;
  i0 = i0 == i1;
  if (i0) {goto B10;}
  i0 = l6;
  i1 = l1;
  i0 += i1;
  l6 = i0;
  i1 = l6;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 2147483648u;
  i1 &= i2;
  i2 = 4294967292u;
  i3 = l1;
  i2 -= i3;
  i3 = l5;
  i2 += i3;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l7;
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1));
  i32_store((&memory), (u64)(i0), i1);
  i0 = l6;
  i1 = l6;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 2147483647u;
  i1 &= i2;
  i32_store((&memory), (u64)(i0), i1);
  B10:;
  i0 = p0;
  i1 = 8388u;
  i0 += i1;
  l4 = i0;
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 1u;
  i1 += i2;
  l4 = i1;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = l4;
  i2 = 12u;
  i1 *= i2;
  i0 += i1;
  p0 = i0;
  i1 = 8192u;
  i0 += i1;
  l5 = i0;
  i1 = l2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = 8196u;
  i0 += i1;
  i1 = l3;
  i32_store((&memory), (u64)(i0), i1);
  B4:;
  i0 = l5;
  goto Bfunc;
  B3:;
  i0 = l4;
  i0 = i32_load((&memory), (u64)(i0));
  l5 = i0;
  i1 = p0;
  i2 = l1;
  i3 = 12u;
  i2 *= i3;
  i1 += i2;
  l3 = i1;
  i2 = 8200u;
  i1 += i2;
  l1 = i1;
  i1 = i32_load((&memory), (u64)(i1));
  l2 = i1;
  i0 = i0 == i1;
  if (i0) {goto B11;}
  i0 = l3;
  i1 = 8196u;
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = l2;
  i0 += i1;
  l3 = i0;
  i1 = l3;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 2147483648u;
  i1 &= i2;
  i2 = 4294967292u;
  i3 = l2;
  i2 -= i3;
  i3 = l5;
  i2 += i3;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l1;
  i1 = l4;
  i1 = i32_load((&memory), (u64)(i1));
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = l3;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 2147483647u;
  i1 &= i2;
  i32_store((&memory), (u64)(i0), i1);
  B11:;
  i0 = p0;
  i1 = p0;
  i2 = 8388u;
  i1 += i2;
  l2 = i1;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 1u;
  i1 += i2;
  l3 = i1;
  i32_store((&memory), (u64)(i0 + 8384), i1);
  i0 = l2;
  i1 = l3;
  i32_store((&memory), (u64)(i0), i1);
  i0 = 0u;
  goto Bfunc;
  B2:;
  i0 = l4;
  i1 = l5;
  i2 = l2;
  i1 += i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l4;
  Bfunc:;
  FUNC_EPILOGUE;
  return i0;
}

static void f85(u32 p0) {
  u32 l1 = 0, l2 = 0, l3 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p0;
  i0 = !(i0);
  if (i0) {goto B1;}
  i0 = 0u;
  i0 = i32_load((&memory), (u64)(i0 + 17132));
  l1 = i0;
  i1 = 1u;
  i0 = (u32)((s32)i0 < (s32)i1);
  if (i0) {goto B1;}
  i0 = 16940u;
  l2 = i0;
  i0 = l1;
  i1 = 12u;
  i0 *= i1;
  i1 = 16940u;
  i0 += i1;
  l3 = i0;
  L2: 
    i0 = l2;
    i1 = 4u;
    i0 += i1;
    i0 = i32_load((&memory), (u64)(i0));
    l1 = i0;
    i0 = !(i0);
    if (i0) {goto B1;}
    i0 = l1;
    i1 = 4u;
    i0 += i1;
    i1 = p0;
    i0 = i0 > i1;
    if (i0) {goto B3;}
    i0 = l1;
    i1 = l2;
    i1 = i32_load((&memory), (u64)(i1));
    i0 += i1;
    i1 = p0;
    i0 = i0 > i1;
    if (i0) {goto B0;}
    B3:;
    i0 = l2;
    i1 = 12u;
    i0 += i1;
    l2 = i0;
    i1 = l3;
    i0 = i0 < i1;
    if (i0) {goto L2;}
  B1:;
  goto Bfunc;
  B0:;
  i0 = p0;
  i1 = 4294967292u;
  i0 += i1;
  l2 = i0;
  i1 = l2;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 2147483647u;
  i1 &= i2;
  i32_store((&memory), (u64)(i0), i1);
  Bfunc:;
  FUNC_EPILOGUE;
}

static const u8 data_segment_data_0[] = {
  0x65, 0x6f, 0x73, 0x69, 0x6f, 0x2e, 0x74, 0x6f, 0x6b, 0x65, 0x6e, 0x00, 
  0x00, 0x01, 0x02, 0x04, 0x07, 0x03, 0x06, 0x05, 0x00, 
};

static const u8 data_segment_data_1[] = {
  0x74, 0x72, 0x61, 0x6e, 0x73, 0x66, 0x65, 0x72, 0x00, 0x6d, 0x61, 0x6c, 
  0x6c, 0x6f, 0x63, 0x5f, 0x66, 0x72, 0x6f, 0x6d, 0x5f, 0x66, 0x72, 0x65, 
  0x65, 0x64, 0x20, 0x77, 0x61, 0x73, 0x20, 0x64, 0x65, 0x73, 0x69, 0x67, 
  0x6e, 0x65, 0x64, 0x20, 0x74, 0x6f, 0x20, 0x6f, 0x6e, 0x6c, 0x79, 0x20, 
  0x62, 0x65, 0x20, 0x63, 0x61, 0x6c, 0x6c, 0x65, 0x64, 0x20, 0x61, 0x66, 
  0x74, 0x65, 0x72, 0x20, 0x5f, 0x68, 0x65, 0x61, 0x70, 0x20, 0x77, 0x61, 
  0x73, 0x20, 0x63, 0x6f, 0x6d, 0x70, 0x6c, 0x65, 0x74, 0x65, 0x6c, 0x79, 
  0x20, 0x61, 0x6c, 0x6c, 0x6f, 0x63, 0x61, 0x74, 0x65, 0x64, 0x00, 
};

static const u8 data_segment_data_2[] = {
  0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x20, 0x69, 0x73, 0x20, 0x74, 0x6f, 
  0x6f, 0x20, 0x6c, 0x6f, 0x6e, 0x67, 0x20, 0x74, 0x6f, 0x20, 0x62, 0x65, 
  0x20, 0x61, 0x20, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x20, 0x6e, 0x61, 0x6d, 
  0x65, 0x00, 
};

static const u8 data_segment_data_3[] = {
  0x74, 0x68, 0x69, 0x72, 0x74, 0x65, 0x65, 0x6e, 0x74, 0x68, 0x20, 0x63, 
  0x68, 0x61, 0x72, 0x61, 0x63, 0x74, 0x65, 0x72, 0x20, 0x69, 0x6e, 0x20, 
  0x6e, 0x61, 0x6d, 0x65, 0x20, 0x63, 0x61, 0x6e, 0x6e, 0x6f, 0x74, 0x20, 
  0x62, 0x65, 0x20, 0x61, 0x20, 0x6c, 0x65, 0x74, 0x74, 0x65, 0x72, 0x20, 
  0x74, 0x68, 0x61, 0x74, 0x20, 0x63, 0x6f, 0x6d, 0x65, 0x73, 0x20, 0x61, 
  0x66, 0x74, 0x65, 0x72, 0x20, 0x6a, 0x00, 
};

static const u8 data_segment_data_4[] = {
  0x63, 0x68, 0x61, 0x72, 0x61, 0x63, 0x74, 0x65, 0x72, 0x20, 0x69, 0x73, 
  0x20, 0x6e, 0x6f, 0x74, 0x20, 0x69, 0x6e, 0x20, 0x61, 0x6c, 0x6c, 0x6f, 
  0x77, 0x65, 0x64, 0x20, 0x63, 0x68, 0x61, 0x72, 0x61, 0x63, 0x74, 0x65, 
  0x72, 0x20, 0x73, 0x65, 0x74, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x6e, 0x61, 
  0x6d, 0x65, 0x73, 0x00, 
};

static const u8 data_segment_data_5[] = {
  0x79, 0x6f, 0x75, 0x20, 0x73, 0x68, 0x61, 0x6c, 0x6c, 0x20, 0x6e, 0x6f, 
  0x74, 0x20, 0x70, 0x61, 0x73, 0x73, 0x21, 0x00, 0x73, 0x74, 0x6f, 0x75, 
  0x6c, 0x6c, 0x00, 
};

static const u8 data_segment_data_6[] = {
  0x66, 0x72, 0x6f, 0x6d, 0x20, 0x3a, 0x20, 0x00, 
};

static const u8 data_segment_data_7[] = {
  0x20, 0x74, 0x6f, 0x20, 0x3a, 0x20, 0x00, 
};

static const u8 data_segment_data_8[] = {
  0x2c, 0x20, 0x71, 0x75, 0x61, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x20, 0x3a, 
  0x20, 0x00, 
};

static const u8 data_segment_data_9[] = {
  0x2c, 0x20, 0x6d, 0x65, 0x6d, 0x6f, 0x20, 0x3a, 0x20, 0x00, 
};

static const u8 data_segment_data_10[] = {
  0x3b, 0x00, 
};

static const u8 data_segment_data_11[] = {
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00, 
};

static const u8 data_segment_data_12[] = {
  0x73, 0x70, 0x65, 0x63, 0x69, 0x66, 0x79, 0x20, 0x72, 0x6f, 0x6c, 0x6c, 
  0x5f, 0x75, 0x6e, 0x64, 0x65, 0x72, 0x20, 0x69, 0x6e, 0x20, 0x6d, 0x65, 
  0x6d, 0x6f, 0x21, 0x00, 
};

static const u8 data_segment_data_13[] = {
  0x72, 0x6f, 0x6c, 0x6c, 0x5f, 0x75, 0x6e, 0x64, 0x65, 0x72, 0x20, 0x6d, 
  0x75, 0x73, 0x74, 0x20, 0x62, 0x65, 0x20, 0x69, 0x6e, 0x20, 0x5b, 0x33, 
  0x2e, 0x2e, 0x39, 0x36, 0x5d, 0x21, 0x00, 
};

static const u8 data_segment_data_14[] = {
  0x20, 0x72, 0x6f, 0x6c, 0x6c, 0x5f, 0x75, 0x6e, 0x64, 0x65, 0x72, 0x20, 
  0x69, 0x73, 0x20, 0x00, 
};

static const u8 data_segment_data_15[] = {
  0x20, 0x72, 0x6f, 0x6c, 0x6c, 0x20, 0x69, 0x73, 0x20, 0x00, 
};

static const u8 data_segment_data_16[] = {
  0x20, 0x77, 0x69, 0x6e, 0x20, 0x69, 0x73, 0x20, 0x00, 
};

static const u8 data_segment_data_17[] = {
  0x20, 0x6c, 0x6f, 0x73, 0x65, 0x21, 0x00, 
};

static const u8 data_segment_data_18[] = {
  0x69, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x20, 0x73, 0x79, 0x6d, 0x62, 
  0x6f, 0x6c, 0x20, 0x6e, 0x61, 0x6d, 0x65, 0x00, 
};

static const u8 data_segment_data_19[] = {
  0x61, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 
};

static const u8 data_segment_data_20[] = {
  0x59, 0x6f, 0x75, 0x20, 0x77, 0x69, 0x6e, 0x20, 0x69, 0x6e, 0x20, 0x64, 
  0x69, 0x63, 0x65, 0x20, 0x67, 0x61, 0x6d, 0x65, 0x21, 0x00, 
};

static const u8 data_segment_data_21[] = {
  0x77, 0x72, 0x69, 0x74, 0x65, 0x00, 
};

static const u8 data_segment_data_22[] = {
  0x72, 0x65, 0x61, 0x64, 0x00, 
};

static const u8 data_segment_data_23[] = {
  0x67, 0x65, 0x74, 0x00, 0x3a, 0x20, 0x6e, 0x6f, 0x20, 0x63, 0x6f, 0x6e, 
  0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 
};

static const u8 data_segment_data_24[] = {
  0x3a, 0x20, 0x6f, 0x75, 0x74, 0x20, 0x6f, 0x66, 0x20, 0x72, 0x61, 0x6e, 
  0x67, 0x65, 0x00, 
};

static const u8 data_segment_data_25[] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 
  0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 
  0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 
  0x20, 0x21, 0x22, 0x23, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 
};

static void init_memory(void) {
  wasm_rt_allocate_memory((&memory), 1, 65536);
  memcpy(&(memory.data[8192u]), data_segment_data_0, 21);
  memcpy(&(memory.data[8213u]), data_segment_data_1, 95);
  memcpy(&(memory.data[8308u]), data_segment_data_2, 38);
  memcpy(&(memory.data[8346u]), data_segment_data_3, 67);
  memcpy(&(memory.data[8413u]), data_segment_data_4, 52);
  memcpy(&(memory.data[8465u]), data_segment_data_5, 27);
  memcpy(&(memory.data[8492u]), data_segment_data_6, 8);
  memcpy(&(memory.data[8500u]), data_segment_data_7, 7);
  memcpy(&(memory.data[8507u]), data_segment_data_8, 14);
  memcpy(&(memory.data[8521u]), data_segment_data_9, 10);
  memcpy(&(memory.data[8531u]), data_segment_data_10, 2);
  memcpy(&(memory.data[8533u]), data_segment_data_11, 11);
  memcpy(&(memory.data[8544u]), data_segment_data_12, 28);
  memcpy(&(memory.data[8572u]), data_segment_data_13, 31);
  memcpy(&(memory.data[8603u]), data_segment_data_14, 16);
  memcpy(&(memory.data[8619u]), data_segment_data_15, 10);
  memcpy(&(memory.data[8629u]), data_segment_data_16, 9);
  memcpy(&(memory.data[8638u]), data_segment_data_17, 7);
  memcpy(&(memory.data[8645u]), data_segment_data_18, 20);
  memcpy(&(memory.data[8665u]), data_segment_data_19, 7);
  memcpy(&(memory.data[8672u]), data_segment_data_20, 22);
  memcpy(&(memory.data[8694u]), data_segment_data_21, 6);
  memcpy(&(memory.data[8700u]), data_segment_data_22, 5);
  memcpy(&(memory.data[8705u]), data_segment_data_23, 20);
  memcpy(&(memory.data[17144u]), data_segment_data_24, 15);
  memcpy(&(memory.data[17168u]), data_segment_data_25, 257);
}

static void init_table(void) {
  uint32_t offset;
  wasm_rt_allocate_table((&T0), 1, 1);
}

/* export: 'memory' */
wasm_rt_memory_t (*WASM_RT_ADD_PREFIX(Z_memory));
/* export: '__heap_base' */
u32 (*WASM_RT_ADD_PREFIX(Z___heap_baseZ_i));
/* export: '__data_end' */
u32 (*WASM_RT_ADD_PREFIX(Z___data_endZ_i));
/* export: 'apply' */
void (*WASM_RT_ADD_PREFIX(Z_applyZ_vjjj))(u64, u64, u64);
/* export: '_ZdlPv' */
void (*WASM_RT_ADD_PREFIX(Z__Z5AdlPvZ_vi))(u32);
/* export: '_Znwj' */
u32 (*WASM_RT_ADD_PREFIX(Z__Z5AnwjZ_ii))(u32);
/* export: '_Znaj' */
u32 (*WASM_RT_ADD_PREFIX(Z__Z5AnajZ_ii))(u32);
/* export: '_ZdaPv' */
void (*WASM_RT_ADD_PREFIX(Z__Z5AdaPvZ_vi))(u32);
/* export: '_ZnwjSt11align_val_t' */
u32 (*WASM_RT_ADD_PREFIX(Z__Z5AnwjSt11align_val_tZ_iii))(u32, u32);
/* export: '_ZnajSt11align_val_t' */
u32 (*WASM_RT_ADD_PREFIX(Z__Z5AnajSt11align_val_tZ_iii))(u32, u32);
/* export: '_ZdlPvSt11align_val_t' */
void (*WASM_RT_ADD_PREFIX(Z__Z5AdlPvSt11align_val_tZ_vii))(u32, u32);
/* export: '_ZdaPvSt11align_val_t' */
void (*WASM_RT_ADD_PREFIX(Z__Z5AdaPvSt11align_val_tZ_vii))(u32, u32);

static void init_exports(void) {
  /* export: 'memory' */
  WASM_RT_ADD_PREFIX(Z_memory) = (&memory);
  /* export: '__heap_base' */
  WASM_RT_ADD_PREFIX(Z___heap_baseZ_i) = (&__heap_base);
  /* export: '__data_end' */
  WASM_RT_ADD_PREFIX(Z___data_endZ_i) = (&__data_end);
  /* export: 'apply' */
  WASM_RT_ADD_PREFIX(Z_applyZ_vjjj) = (&apply);
  /* export: '_ZdlPv' */
  WASM_RT_ADD_PREFIX(Z__Z5AdlPvZ_vi) = (&_ZdlPv);
  /* export: '_Znwj' */
  WASM_RT_ADD_PREFIX(Z__Z5AnwjZ_ii) = (&_Znwj);
  /* export: '_Znaj' */
  WASM_RT_ADD_PREFIX(Z__Z5AnajZ_ii) = (&_Znaj);
  /* export: '_ZdaPv' */
  WASM_RT_ADD_PREFIX(Z__Z5AdaPvZ_vi) = (&_ZdaPv);
  /* export: '_ZnwjSt11align_val_t' */
  WASM_RT_ADD_PREFIX(Z__Z5AnwjSt11align_val_tZ_iii) = (&_ZnwjSt11align_val_t);
  /* export: '_ZnajSt11align_val_t' */
  WASM_RT_ADD_PREFIX(Z__Z5AnajSt11align_val_tZ_iii) = (&_ZnajSt11align_val_t);
  /* export: '_ZdlPvSt11align_val_t' */
  WASM_RT_ADD_PREFIX(Z__Z5AdlPvSt11align_val_tZ_vii) = (&_ZdlPvSt11align_val_t);
  /* export: '_ZdaPvSt11align_val_t' */
  WASM_RT_ADD_PREFIX(Z__Z5AdaPvSt11align_val_tZ_vii) = (&_ZdaPvSt11align_val_t);
}

void WASM_RT_ADD_PREFIX(init)(void) {
  init_func_types();
  init_globals();
  init_memory();
  init_table();
  init_exports();
}
