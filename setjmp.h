#ifndef MK_SETJMP_STUB
#define MK_SETJMP_STUB
typedef int jmp_buf[8];
#define setjmp(b) 0
#define longjmp(b,v) __builtin_trap()
#endif
