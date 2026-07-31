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

static void json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    const unsigned char *cursor;

    P101_TRACE_SCOPE(env);
    p101_fputc(env, err, '"', stream);
    for(cursor = (const unsigned char *)text; *cursor != '\0'; cursor++)
    {
        if(*cursor == '"' || *cursor == '\\')
        {
            p101_fputc(env, err, '\\', stream);
            p101_fputc(env, err, *cursor, stream);
        }
        else if(*cursor == '\n')
        {
            p101_fputs(env, err, "\\n", stream);
        }
        else
        {
            p101_fputc(env, err, *cursor, stream);
        }
    }
    p101_fputc(env, err, '"', stream);
}

void p101_mutation_report_results(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_result results[], size_t result_count)
{
    size_t index;
    size_t killed;
    size_t survived;
    size_t inconclusive;

    P101_TRACE_SCOPE(env);
    killed       = 0U;
    survived     = 0U;
    inconclusive = 0U;
    for(index = 0U; index < result_count; index++)
    {
        if(p101_strcmp(env, results[index].outcome, "killed") == 0)
        {
            killed++;
        }
        else if(p101_strcmp(env, results[index].outcome, "survived") == 0)
        {
            survived++;
        }
        else
        {
            inconclusive++;
        }
    }
    if(arguments->json)
    {
        bool first;

        p101_fputs(env, err, "{\"schema\":\"p101-mutation-check-findings-v2\",\"findings\":[", stdout);
        first = true;
        for(index = 0U; index < result_count; index++)
        {
            const struct p101_mutation_candidate *candidate;

            if(p101_strcmp(env, results[index].outcome, "survived") != 0)
            {
                continue;
            }
            candidate = results[index].candidate;
            if(!first)
            {
                p101_fputc(env, err, ',', stdout);
            }
            first = false;
            p101_fputs(env,
                       err,
                       "{\"id\":\"P101-MUTATION-001\",\"severity\":\"warning\","
                       "\"location\":{\"path\":",
                       stdout);
            json_string(env, err, stdout, candidate->path);
            p101_fprintf(env,
                         err,
                         stdout,
                         ",\"line\":%zu},\"message\":\"the test command passed after "
                         "a focused source mutation\",\"evidence\":{\"operator\":",
                         candidate->line);
            json_string(env, err, stdout, p101_c_mutation_kind_name(candidate->kind));
            p101_fputs(env, err, ",\"original\":", stdout);
            json_string(env, err, stdout, candidate->original);
            p101_fputs(env, err, ",\"replacement\":", stdout);
            json_string(env, err, stdout, candidate->replacement);
            p101_fputs(env, err, "}}", stdout);
        }
        p101_fprintf(env,
                     err,
                     stdout,
                     "],\"summary\":{\"selected\":%zu,\"killed\":%zu,\"survived\":%"
                     "zu,\"inconclusive\":%zu}}\n",
                     result_count,
                     killed,
                     survived,
                     inconclusive);
    }
    else
    {
        for(index = 0U; index < result_count; index++)
        {
            const struct p101_mutation_candidate *candidate;

            if(p101_strcmp(env, results[index].outcome, "survived") != 0)
            {
                continue;
            }
            candidate = results[index].candidate;
            p101_fprintf(env, err, stdout, "P101-MUTATION-001: %s:%zu: survived %s: %s -> %s\n", candidate->path, candidate->line, p101_c_mutation_kind_name(candidate->kind), candidate->original, candidate->replacement);
        }
        p101_fprintf(env,
                     err,
                     stdout,
                     "p101-mutation-check: selected=%zu killed=%zu survived=%zu "
                     "inconclusive=%zu\n",
                     result_count,
                     killed,
                     survived,
                     inconclusive);
    }
}

void p101_mutation_list_candidates(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_candidates *candidates)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    if(arguments->json)
    {
        p101_fputs(env, err, "{\"schema\":\"p101-mutation-candidates-v2\",\"candidates\":[", stdout);
    }
    for(index = 0U; index < candidates->count; index++)
    {
        const struct p101_mutation_candidate *candidate;

        candidate = &candidates->items[index];
        if(arguments->json)
        {
            if(index > 0U)
            {
                p101_fputc(env, err, ',', stdout);
            }
            p101_fputs(env, err, "{\"path\":", stdout);
            json_string(env, err, stdout, candidate->path);
            p101_fprintf(env, err, stdout, ",\"line\":%zu,\"operator\":", candidate->line);
            json_string(env, err, stdout, p101_c_mutation_kind_name(candidate->kind));
            p101_fputs(env, err, ",\"original\":", stdout);
            json_string(env, err, stdout, candidate->original);
            p101_fputs(env, err, ",\"replacement\":", stdout);
            json_string(env, err, stdout, candidate->replacement);
            p101_fputc(env, err, '}', stdout);
        }
        else
        {
            p101_fprintf(env, err, stdout, "%s:%zu: %s: %s -> %s\n", candidate->path, candidate->line, p101_c_mutation_kind_name(candidate->kind), candidate->original, candidate->replacement);
        }
    }
    if(arguments->json)
    {
        p101_fprintf(env, err, stdout, "],\"summary\":{\"selected\":%zu}}\n", candidates->count);
    }
    else
    {
        p101_fprintf(env, err, stdout, "p101-mutation-check: %zu candidate(s)\n", candidates->count);
    }
}
