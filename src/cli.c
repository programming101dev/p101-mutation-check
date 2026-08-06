#include "mutation_check.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/compile_command.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/p101_fnmatch.h>
#include <p101_filesystem/p101_ftw.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_filesystem/p101_libgen.h>
#include <p101_filesystem/p101_stdio.h>
#include <p101_filesystem/p101_stdlib.h>
#include <p101_filesystem/p101_unistd.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_filesystem/sys/p101_statvfs.h>
#include <p101_io/p101_aio.h>
#include <p101_io/p101_fcntl.h>
#include <p101_io/p101_poll.h>
#include <p101_io/p101_stdio.h>
#include <p101_io/p101_unistd.h>
#include <p101_io/sys/p101_select.h>
#include <p101_io/sys/p101_uio.h>
#include <p101_process/p101_sched.h>
#include <p101_process/p101_setjmp.h>
#include <p101_process/p101_signal.h>
#include <p101_process/p101_spawn.h>
#include <p101_process/p101_stdio.h>
#include <p101_process/p101_stdlib.h>
#include <p101_process/p101_unistd.h>
#include <p101_process/sys/p101_resource.h>
#include <p101_process/sys/p101_times.h>
#include <p101_process/sys/p101_wait.h>
#include <p101_time/p101_time.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum
{
    MAX_MUTANTS         = 4096,
    INTEGER_BASE        = 10,
    DEFAULT_MAX_MUTANTS = 100
};

static const double DEFAULT_TIMEOUT_SECONDS = 120.0;

void p101_mutation_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int status)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = status == EXIT_SUCCESS ? stdout : stderr;
    p101_fprintf(env, err, stream, "Usage: %s --compile-db FILE [options] PROJECT [-- TEST-COMMAND...]\n", program_name);
    p101_fputs(env, err, "Options:\n", stream);
    p101_fputs(env, err, "  --operator NAME      select a mutation operator; repeatable\n", stream);
    p101_fputs(env, err, "  --max-mutants N      cap selected mutants (default 100)\n", stream);
    p101_fputs(env, err, "  --timeout SECONDS    per-command timeout (default 120)\n", stream);
    p101_fputs(env, err, "  --list               list candidates without running tests\n", stream);
    p101_fputs(env, err, "  --json               emit JSON\n", stream);
    p101_fputs(env, err, "  -h, --help           show this help\n", stream);
    p101_fputs(env, err, "Exit status: 0 all killed, 1 survived, 2 tool trouble.\n", stream);
}

static bool parse_size(const struct p101_env *env, struct p101_error *err, const char *text, size_t *value)
{
    int           p101_expression_result_6;
    int           p101_expression_result_7;
    int           p101_expression_result_8;
    int           p101_expression_result_9;
    bool          p101_call_result_10;
    unsigned long parsed;
    char         *end;
    bool          valid;

    P101_TRACE_SCOPE(env);
    end                      = NULL;
    parsed                   = p101_strtoul(env, err, text, &end, INTEGER_BASE);
    p101_call_result_10      = p101_error_has_no_error(err);
    p101_expression_result_9 = 0;
    if(p101_call_result_10)
    {
        if(end != text)
        {
            p101_expression_result_9 = 1;
        }
    }
    p101_expression_result_8 = 0;
    if(p101_expression_result_9)
    {
        if(*end == '\0')
        {
            p101_expression_result_8 = 1;
        }
    }
    p101_expression_result_7 = 0;
    if(p101_expression_result_8)
    {
        if(parsed > 0UL)
        {
            p101_expression_result_7 = 1;
        }
    }
    p101_expression_result_6 = 0;
    if(p101_expression_result_7)
    {
        if(parsed <= MAX_MUTANTS)
        {
            p101_expression_result_6 = 1;
        }
    }
    valid = p101_expression_result_6 != 0;
    if(valid)
    {
        *value = parsed;
    }
    return valid;
}

static bool parse_timeout(const struct p101_env *env, struct p101_error *err, const char *text, double *value)
{
    int    p101_expression_result_11;
    int    p101_expression_result_12;
    int    p101_expression_result_13;
    bool   p101_call_result_14;
    char  *end;
    double parsed;
    bool   valid;

    P101_TRACE_SCOPE(env);
    end                       = NULL;
    parsed                    = p101_strtod(env, err, text, &end);
    p101_call_result_14       = p101_error_has_no_error(err);
    p101_expression_result_13 = 0;
    if(p101_call_result_14)
    {
        if(end != text)
        {
            p101_expression_result_13 = 1;
        }
    }
    p101_expression_result_12 = 0;
    if(p101_expression_result_13)
    {
        if(*end == '\0')
        {
            p101_expression_result_12 = 1;
        }
    }
    p101_expression_result_11 = 0;
    if(p101_expression_result_12)
    {
        if(parsed > 0.0)
        {
            p101_expression_result_11 = 1;
        }
    }
    valid = p101_expression_result_11 != 0;
    if(valid)
    {
        *value = parsed;
    }
    return valid;
}

bool p101_mutation_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct p101_mutation_arguments *arguments)
{
    int  p101_expression_result_15;
    int  p101_call_result_16;
    int  p101_call_result_17;
    int  p101_expression_result_18;
    int  p101_call_result_19;
    int  p101_expression_result_20;
    int  p101_expression_result_21;
    int  p101_call_result_22;
    int  p101_expression_result_23;
    int  p101_call_result_24;
    int  p101_expression_result_25;
    int  p101_call_result_26;
    int  p101_call_result_5;
    int  p101_call_result_4;
    int  p101_call_result_1;
    bool p101_call_result_2;
    bool p101_call_result_3;
    int  index;
    bool valid;

    P101_TRACE_SCOPE(env);
    p101_memset(env, arguments, 0, sizeof(*arguments));
    arguments->max_mutants = DEFAULT_MAX_MUTANTS;
    arguments->timeout     = DEFAULT_TIMEOUT_SECONDS;
    valid                  = true;
    for(index = 1; index < argc; index++)
    {
        const char *argument;

        argument           = argv[index];
        p101_call_result_1 = p101_strcmp(env, argument, "--");
        if(p101_call_result_1 == 0)
        {
            arguments->test_command       = &argv[index + 1];
            arguments->test_command_count = (size_t)(argc - index - 1);
            break;
        }
        p101_call_result_16 = p101_strcmp(env, argument, "-h");
        if(p101_call_result_16 == 0)
        {
            p101_expression_result_15 = 1;
        }
        else
        {
            p101_call_result_17 = p101_strcmp(env, argument, "--help");
            if(p101_call_result_17 == 0)
            {
                p101_expression_result_15 = 1;
            }
            else
            {
                p101_expression_result_15 = 0;
            }
        }
        if(p101_expression_result_15)
        {
            p101_mutation_usage(env, err, argv[0], EXIT_SUCCESS);
            valid = false;
            break;
        }
        p101_call_result_19       = p101_strcmp(env, argument, "--compile-db");
        p101_expression_result_18 = 0;
        if(p101_call_result_19 == 0)
        {
            if(index + 1 < argc)
            {
                p101_expression_result_18 = 1;
            }
        }
        if(p101_expression_result_18)
        {
            arguments->compile_database = argv[++index];
        }
        else
        {
            p101_call_result_22       = p101_strcmp(env, argument, "--operator");
            p101_expression_result_21 = 0;
            if(p101_call_result_22 == 0)
            {
                if(index + 1 < argc)
                {
                    p101_expression_result_21 = 1;
                }
            }
            p101_expression_result_20 = 0;
            if(p101_expression_result_21)
            {
                if(arguments->operator_count < P101_MUTATION_MAX_OPERATORS)
                {
                    p101_expression_result_20 = 1;
                }
            }
            if(p101_expression_result_20)
            {
                arguments->operators[arguments->operator_count++] = argv[++index];
            }
            else
            {
                p101_call_result_24       = p101_strcmp(env, argument, "--max-mutants");
                p101_expression_result_23 = 0;
                if(p101_call_result_24 == 0)
                {
                    if(index + 1 < argc)
                    {
                        p101_expression_result_23 = 1;
                    }
                }
                if(p101_expression_result_23)
                {
                    p101_call_result_2 = parse_size(env, err, argv[++index], &arguments->max_mutants);
                    if(!p101_call_result_2)
                    {
                        valid = false;
                        break;
                    }
                }
                else
                {
                    p101_call_result_26       = p101_strcmp(env, argument, "--timeout");
                    p101_expression_result_25 = 0;
                    if(p101_call_result_26 == 0)
                    {
                        if(index + 1 < argc)
                        {
                            p101_expression_result_25 = 1;
                        }
                    }
                    if(p101_expression_result_25)
                    {
                        p101_call_result_3 = parse_timeout(env, err, argv[++index], &arguments->timeout);
                        if(!p101_call_result_3)
                        {
                            valid = false;
                            break;
                        }
                    }
                    else
                    {
                        p101_call_result_4 = p101_strcmp(env, argument, "--list");
                        if(p101_call_result_4 == 0)
                        {
                            arguments->list_only = true;
                        }
                        else
                        {
                            p101_call_result_5 = p101_strcmp(env, argument, "--json");
                            if(p101_call_result_5 == 0)
                            {
                                arguments->json = true;
                            }
                            else
                            {
                                if(argument[0] == '-' || arguments->project != NULL)
                                {
                                    valid = false;
                                    break;
                                }
                                arguments->project = argument;
                            }
                        }
                    }
                }
            }
        }
    }
    if(valid && (arguments->project == NULL || arguments->compile_database == NULL || (!arguments->list_only && arguments->test_command_count == 0U)))
    {
        valid = false;
    }
    return valid;
}
