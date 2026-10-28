#ifndef MEMBER_A_SWITCH_H
#define MEMBER_A_SWITCH_H
#include <defs.h>
#include <trap.h>
#define MEMBER_A_SWITCH_RESUME 0
#define MEMBER_A_SWITCH_FINISH 1
/* Single core, non-nested, integer-only experiment. Caller owns mappings. */
long member_a_enter_user(uintptr_t entry, uintptr_t user_sp,
                         uintptr_t kernel_trap_stack_top);
void member_a_user_trap_entry(void);
/* Upper-layer integration point, supplied by C; fixture only during A tests. */
long member_a_user_trap(struct trapframe *tf);
#endif
