/*
 * libFuzzer harness for the template's OWN argument parser
 * (src/main.c: parse_arguments()). This fuzzes the code YOU write, not a
 * library function.
 *
 * parse_arguments() is static, so we #include the whole main.c: that makes it
 * visible AND compiles it WITH the fuzzer instrumentation, which is what makes
 * this coverage-guided (watch "cov:" climb) instead of a blind black-box run.
 * Two collisions are handled by -D defines in fuzz/CMakeLists.txt, so nothing
 * in src/ has to change:
 *
 *   -Dmain=p101_unused_main  the program's main() would clash with libFuzzer's
 *                            own main(); it is renamed aside. We never call it
 *                            -- we call parse_arguments() directly, so run_fsm()
 *                            never runs and nothing sleeps.
 *   -Dexit=p101_fuzz_exit    usage() (the -h / bad-usage path) is _Noreturn and
 *                            calls exit(). An exit() mid-iteration looks like a
 *                            crash and kills the whole run, so we redirect it
 *                            into a longjmp back here. This is option-agnostic:
 *                            it keeps working if you add or remove options.
 *
 * src/fsm.c is compiled in only so the (unused) renamed main() can resolve
 * run_fsm() at link time.
 */
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* longjmp target for the redirected exit() -- see -Dexit in fuzz/CMakeLists.txt. */
static jmp_buf g_fuzz_exit_jmp;

/* The code under test. main -> p101_unused_main, exit -> p101_fuzz_exit (via -D). */
#include "../src/main.c"

/* The redirected exit(): unwind back into the harness instead of terminating
 * the process. _Noreturn matches exit()'s contract (usage() is _Noreturn);
 * longjmp guarantees it never actually returns. */
_Noreturn void p101_fuzz_exit(int code)
{
    (void)code;
    longjmp(g_fuzz_exit_jmp, 1);
}

#define FUZZ_MAX_ARGS 64

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char              *buf;
    char              *argv[FUZZ_MAX_ARGS];
    int                argc;
    char              *p;
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;

    /* getopt/argv need a writable, NUL-terminated C string. */
    buf = (char *)malloc(size + 1);
    if(buf == NULL)
    {
        return 0;
    }
    memcpy(buf, data, size);
    buf[size] = '\0';

    /* Carve the input into an argv, splitting on whitespace. argv[0] is a fixed
     * program name; the fuzzer controls every token after it. */
    argv[0] = (char *)"prog";
    argc    = 1;
    p       = buf;
    while(argc < FUZZ_MAX_ARGS - 1)
    {
        while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f')
        {
            p++;
        }
        if(*p == '\0')
        {
            break;
        }
        argv[argc++] = p;
        while(*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != '\v' && *p != '\f')
        {
            p++;
        }
        if(*p != '\0')
        {
            *p++ = '\0';
        }
    }
    argv[argc] = NULL;

    /* getopt keeps a global cursor across calls; reset it before every parse. */
#if defined(__GLIBC__)
    optind = 0; /* glibc: 0 forces a full re-init */
#else
    {
        extern int optreset; /* BSD / macOS / FreeBSD getopt */
        optreset = 1;
        optind   = 1;
    }
#endif

    err = p101_error_create(false);
    env = p101_env_create(err, NULL);
    memset(&args, 0, sizeof(args));

    /* If parse_arguments takes the -h path, usage()->exit()->longjmp lands here
     * with a non-zero return -- a normal outcome, not a crash. */
    if(setjmp(g_fuzz_exit_jmp) == 0)
    {
        parse_arguments(env, err, argc, argv, &args);
    }

    p101_env_destroy(env);
    p101_error_destroy(err);
    free(buf);
    return 0;
}
