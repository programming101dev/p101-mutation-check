#include "mutation_check.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/compile_command.h>
#include <p101_filesystem/filesystem.h>
#include <p101_io/io.h>
#include <p101_process/process.h>
#include <p101_time/time.h>
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
    unsigned long parsed;
    char         *end;

    P101_TRACE_SCOPE(env);
    end    = NULL;
    parsed = p101_strtoul(env, err, text, &end, INTEGER_BASE);
    if(p101_error_has_error(err) || end == text || *end != '\0' || parsed == 0UL || parsed > MAX_MUTANTS)
    {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parse_timeout(const struct p101_env *env, struct p101_error *err, const char *text, double *value)
{
    char  *end;
    double parsed;

    P101_TRACE_SCOPE(env);
    end    = NULL;
    parsed = p101_strtod(env, err, text, &end);
    if(p101_error_has_error(err) || end == text || *end != '\0' || parsed <= 0.0)
    {
        return false;
    }
    *value = parsed;
    return true;
}

bool p101_mutation_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct p101_mutation_arguments *arguments)
{
    int index;

    P101_TRACE_SCOPE(env);
    p101_memset(env, arguments, 0, sizeof(*arguments));
    arguments->max_mutants = DEFAULT_MAX_MUTANTS;
    arguments->timeout     = DEFAULT_TIMEOUT_SECONDS;
    for(index = 1; index < argc; index++)
    {
        const char *argument;

        argument = argv[index];
        if(p101_strcmp(env, argument, "--") == 0)
        {
            arguments->test_command       = &argv[index + 1];
            arguments->test_command_count = (size_t)(argc - index - 1);
            break;
        }
        if(p101_strcmp(env, argument, "-h") == 0 || p101_strcmp(env, argument, "--help") == 0)
        {
            p101_mutation_usage(env, err, argv[0], EXIT_SUCCESS);
            return false;
        }
        if(p101_strcmp(env, argument, "--compile-db") == 0 && index + 1 < argc)
        {
            arguments->compile_database = argv[++index];
        }
        else if(p101_strcmp(env, argument, "--operator") == 0 && index + 1 < argc && arguments->operator_count < P101_MUTATION_MAX_OPERATORS)
        {
            arguments->operators[arguments->operator_count++] = argv[++index];
        }
        else if(p101_strcmp(env, argument, "--max-mutants") == 0 && index + 1 < argc)
        {
            if(!parse_size(env, err, argv[++index], &arguments->max_mutants))
            {
                return false;
            }
        }
        else if(p101_strcmp(env, argument, "--timeout") == 0 && index + 1 < argc)
        {
            if(!parse_timeout(env, err, argv[++index], &arguments->timeout))
            {
                return false;
            }
        }
        else if(p101_strcmp(env, argument, "--list") == 0)
        {
            arguments->list_only = true;
        }
        else if(p101_strcmp(env, argument, "--json") == 0)
        {
            arguments->json = true;
        }
        else
        {
            if(argument[0] == '-' || arguments->project != NULL)
            {
                return false;
            }
            arguments->project = argument;
        }
    }
    if(arguments->project == NULL || arguments->compile_database == NULL || (!arguments->list_only && arguments->test_command_count == 0U))
    {
        return false;
    }
    return true;
}
