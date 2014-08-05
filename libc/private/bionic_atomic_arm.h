/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef BIONIC_ATOMIC_ARM_H
#define BIONIC_ATOMIC_ARM_H

#include <machine/cpu-features.h>

/* Some of the harware instructions used below are not available in Thumb-1
 * mode (they are if you build in ARM or Thumb-2 mode though). To solve this
 * problem, we're going to use the same technique than libatomics_ops,
 * which is to temporarily switch to ARM, do the operation, then switch
 * back to Thumb-1.
 *
 * This results in two 'bx' jumps, just like a normal function call, but
 * everything is kept inlined, avoids loading or computing the function's
 * address, and prevents a little I-cache trashing too.
 *
 * However, it is highly recommended to avoid compiling any C library source
 * file that use these functions in Thumb-1 mode.
 *
 * Define three helper macros to implement this:
 */
#if defined(__thumb__) && !defined(__thumb2__)
#  define  __ATOMIC_SWITCH_TO_ARM \
            "adr r3, 5f\n" \
            "bx  r3\n" \
            ".align\n" \
            ".arm\n" \
        "5:\n"
/* note: the leading \n below is intentional */
#  define __ATOMIC_SWITCH_TO_THUMB \
            "\n" \
            "adr r3, 6f+1\n" \
            "bx  r3\n" \
            ".thumb\n" \
        "6:\n"

#  define __ATOMIC_CLOBBERS   "r3",  /* list of clobbered registers */
#else
#  define  __ATOMIC_SWITCH_TO_ARM   /* nothing */
#  define  __ATOMIC_SWITCH_TO_THUMB /* nothing */
#  define  __ATOMIC_CLOBBERS        /* nothing */
#endif

__ATOMIC_INLINE__ void __bionic_memory_barrier() {
#if defined(ANDROID_SMP) && ANDROID_SMP == 1
  __asm__ __volatile__ ( "dmb ish" : : : "memory" );
#elif (__ARM_ARCH__ >= 7)
  /* A simple compiler barrier. */
  __asm__ __volatile__ ( "" : : : "memory" );
#endif
}

/* Compare-and-swap, without any explicit barriers. Note that this functions
 * returns 0 on success, and 1 on failure. The opposite convention is typically
 * used on other platforms.
 */
__ATOMIC_INLINE__ int
__bionic_cmpxchg(int32_t old_value, int32_t new_value, volatile int32_t* ptr)
{
    int32_t prev, status;
    do {
        __asm__ __volatile__ (
            __ATOMIC_SWITCH_TO_ARM
            "ldrex %0, [%3]\n"
            "mov %1, #0\n"
            "teq %0, %4\n"
#ifdef __thumb2__
            "it eq\n"
#endif
            "strexeq %1, %5, [%3]"
            __ATOMIC_SWITCH_TO_THUMB
            : "=&r" (prev), "=&r" (status), "+m"(*ptr)
            : "r" (ptr), "Ir" (old_value), "r" (new_value)
            : __ATOMIC_CLOBBERS "cc");
    } while (__builtin_expect(status != 0, 0));
    return prev != old_value;
}

/* Swap, without any explicit barriers. */
__ATOMIC_INLINE__ int32_t __bionic_swap(int32_t new_value, volatile int32_t* ptr) {
  int32_t prev, status;
  do {
    __asm__ __volatile__ (
          "ldrex %0, [%3]\n"
          "strex %1, %4, [%3]"
          : "=&r" (prev), "=&r" (status), "+m" (*ptr)
          : "r" (ptr), "r" (new_value)
          : "cc");
  } while (__builtin_expect(status != 0, 0));
  return prev;
}

/* Atomic decrement, without explicit barriers. */
__ATOMIC_INLINE__ int32_t __bionic_atomic_dec(volatile int32_t* ptr) {
  int32_t prev, tmp, status;
  do {
    __asm__ __volatile__ (
          "ldrex %0, [%4]\n"
          "sub %1, %0, #1\n"
          "strex %2, %1, [%4]"
          : "=&r" (prev), "=&r" (tmp), "=&r" (status), "+m"(*ptr)
          : "r" (ptr)
          : "cc");
  } while (__builtin_expect(status != 0, 0));
  return prev;
}

#endif /* SYS_ATOMICS_ARM_H */
