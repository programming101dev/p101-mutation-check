#include "arguments.h"
#include "errors.h"
#include "fsm.h"
#include <ctype.h>
#include <p101_c/p101_string.h>
#include <p101_convert/integer.h>
#include <p101_posix/p101_string.h>
#include <p101_posix/p101_unistd.h>
#include <stdio.h>
#include <stdlib.h>

static void           parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
static void           check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static void           convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args);
_Noreturn static void usage(const struct p101_env *env, const char *program_name, int exit_code, const char *message);

#define MSG_LEN 256    // NOLINT(cppcoreguidelines-macro-to-enum, modernize-macro-to-enum)

int main(int argc, char *argv[])
{
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;
    int                ret_val;

    ret_val = EXIT_FAILURE;
    err     = p101_error_create(false);
    env     = p101_env_create(err, NULL);
    p101_memset(env, &args, 0, sizeof(args));
    parse_arguments(env, err, argc, argv, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(args.verbose)
    {
        p101_env_set_tracer(env, p101_env_default_tracer);
    }

    check_arguments(env, err, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    convert_arguments(env, err, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    ret_val = run_fsm(env, err, &args);

done:
    if(p101_error_has_error(err))
    {
        if(p101_error_is_error(err, P101_ERROR_USER, ERR_USAGE))
        {
            const char *msg;

            msg = p101_error_get_message(err);
            usage(env, argv[0], EXIT_FAILURE, msg);
        }

        fprintf(stderr, "%s\n", p101_error_get_message(err));
        ret_val = EXIT_FAILURE;
    }

    p101_env_destroy(env);
    p101_error_destroy(err);

    return ret_val;
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
                P101_ERROR_RAISE_USER(err, NULL, ERR_USAGE);
                break;
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

                snprintf(msg, sizeof msg, "Option '-%c' requires an argument.", optopt ? optopt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            {
                char msg[MSG_LEN];

                if(isprint(optopt))
                {
                    snprintf(msg, sizeof msg, "Unknown option '-%c'.", optopt);
                }
                else
                {
                    snprintf(msg, sizeof msg, "Unknown option character 0x%02X.", (unsigned)(unsigned char)optopt);
                }

                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            default:
            {
                char msg[MSG_LEN];

                snprintf(msg, sizeof msg, "Internal error: unhandled option '-%c' returned by getopt.", isprint(opt) ? opt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
        }
    }

    if(p101_error_has_no_error(err))
    {
        if(optind < argc)
        {
            char   msg[MSG_LEN];
            size_t off;

            off = 0;
            off += (size_t)snprintf(msg + off, sizeof msg - off, "Unexpected argument%s:", (argc - optind) > 1 ? "s" : "");

            for(int i = optind; i < argc && off < sizeof msg; ++i)
            {
                size_t rem;

                rem = sizeof msg - off; /* bytes left including NUL */

                if(rem <= 1)
                {
                    break; /* no room for anything */
                }

                /* append a single leading space if possible */
                msg[off++] = ' ';
                rem        = sizeof msg - off; /* recompute remaining space */

                if(rem > 0)
                {
                    /* copy at most rem-1 chars to leave room for the NUL */
                    size_t ncopy = p101_strnlen(env, argv[i], rem - 1);
                    p101_memcpy(env, msg + off, argv[i], ncopy);
                    off += ncopy;
                    msg[off] = '\0'; /* always NUL-terminate */
                }
            }

            msg[sizeof msg - 1] = '\0';
            P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
        }
    }
}

void check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE(env);

    if(args->delay_str == NULL || args->delay_str[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The delay is required.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

void convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args)
{
    P101_TRACE(env);

    args->delay = p101_parse_unsigned_int(env, err, args->delay_str, 0);

    if(p101_error_has_error(err))
    {
        P101_ERROR_RAISE_USER(err, "delay must be a positive integer.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

_Noreturn static void usage(const struct p101_env *env, const char *program_name, int exit_code, const char *message)
{
    P101_TRACE(env);

    if(message)
    {
        fprintf(stderr, "%s\n\n", message);
    }

    fprintf(stderr, "Usage: %s [-h] [-v] -d <delay>\n", program_name);
    fputs("Options:\n", stderr);
    fputs("  -h                Display this help message and exit\n", stderr);
    fputs("  -v                Enable verbose tracing\n", stderr);
    fputs("  -d <delay>        delay in seconds (required)\n", stderr);
    exit(exit_code);
}
