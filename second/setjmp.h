typedef struct {
	char *__pc;
	char *__fp;
} jmp_buf[1];

extern void longjmp (jmp_buf __env, int __val) __attribute__((noreturn));
extern int setjmp (jmp_buf __env);
                    