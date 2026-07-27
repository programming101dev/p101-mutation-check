#include "cli.h"
#include "errors.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_convert/integer.h>
#include <p101_posix/p101_string.h>
#include <p101_posix/p101_unistd.h>
#include <stdio.h>

static void           parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
static void           check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static void           convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args);
_Noreturn static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message);

enum
{
    MSG_LEN = 256
};

void process_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    P101_TRACE(env);
    parse_arguments(env, err, argc, argv, args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    check_arguments(env, err, args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    convert_arguments(env, err, args);

done:
    if(p101_error_is_error(err, P101_ERROR_USER, ERR_USAGE))
    {
        const char *msg;

        msg = p101_error_get_message(err);
        usage(env, err, argv[0], EXIT_FAILURE, msg);
    }
}

static void parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE(env);

    opterr = 0;

    while((opt = p101_getopt(env, argc, argv, ":hvVd:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                usage(env, err, argv[0], EXIT_SUCCESS, NULL);
            }
            case 'v':
            {
                args->verbose = true;
                break;
            }
            case 'V':
            {
                args->fsm_verbose = true;
                break;
            }
            case 'd':
            {
                if(args->delay_str != NULL)
                {
                    P101_ERROR_RAISE_USER(err, "Option '-d' specified more than once.", ERR_USAGE);
                }

                if(optarg == NULL || optarg[0] == '\0')
                {
                    P101_ERROR_RAISE_USER(err, "Option '-d' requires a non-empty value.", ERR_USAGE);
                }

                args->delay_str = optarg;
                break;
            }
            case ':':
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof msg, "Option '-%c' requires an argument.", optopt ? optopt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            {
                char msg[MSG_LEN];

                if(p101_isprint(env, optopt))
                {
                    p101_snprintf(env, err, msg, sizeof msg, "Unknown option '-%c'.", optopt);
                }
                else
                {
                    p101_snprintf(env, err, msg, sizeof msg, "Unknown option character 0x%02X.", (unsigned)(unsigned char)optopt);
                }

                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            default:
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof msg, "Internal error: unhandled option '-%c' returned by getopt.", p101_isprint(env, opt) ? opt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
        }
    }

    if(p101_error_has_no_error(err) && optind < argc)
    {
        char   msg[MSG_LEN];
        size_t off;

        off = 0;
        off += (size_t)p101_snprintf(env, err, msg + off, sizeof msg - off, "Unexpected argument%s:", (argc - optind) > 1 ? "s" : "");

        for(int i = optind; i < argc && off < sizeof msg; ++i)
        {
            size_t rem;

            rem = sizeof msg - off;

            if(rem <= 1U)
            {
                break;
            }

            msg[off++] = ' ';
            rem        = sizeof msg - off;

            if(rem > 0U)
            {
                size_t ncopy;

                ncopy = p101_strnlen(env, argv[i], rem - 1U);
                p101_memcpy(env, msg + off, argv[i], ncopy);
                off += ncopy;
                msg[off] = '\0';
            }
        }

        msg[sizeof msg - 1U] = '\0';
        P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
    }
}

static void check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE(env);

    if(args->delay_str == NULL || args->delay_str[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The delay is required.", ERR_USAGE);
    }
}

static void convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args)
{
    P101_TRACE(env);

    args->delay = p101_parse_unsigned_int(env, err, args->delay_str, 0);

    if(p101_error_has_error(err))
    {
        P101_ERROR_RAISE_USER(err, "delay must be a positive integer.", ERR_USAGE);
    }
}

_Noreturn static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    P101_TRACE(env);

    if(message)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-v] [-V] -d <delay>\n", program_name);
    p101_fputs(env, err, "Options:\n", stderr);
    p101_fputs(env, err, "  -h                Display this help message and exit\n", stderr);
    p101_fputs(env, err, "  -v                Enable verbose tracing\n", stderr);
    p101_fputs(env, err, "  -V                Enable FSM state-change notifiers\n", stderr);
    p101_fputs(env, err, "  -d <delay>        delay in seconds (required)\n", stderr);
    p101_exit(env, exit_code);
}
