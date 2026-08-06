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
#include <p101_record/record.h>
#include <p101_time/p101_time.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    int p101_call_result_1;
    P101_TRACE_SCOPE(env);
    p101_call_result_1 = p101_record_write_json_string(stream, text == NULL ? "" : text);
    if(p101_call_result_1 != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
}

void p101_mutation_report_results(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_result results[], size_t result_count)
{
    int         p101_call_result_9;
    int         p101_call_result_2;
    int         p101_call_result_3;
    const char *p101_call_result_4;
    int         p101_call_result_5;
    const char *p101_call_result_6;
    size_t      index;
    size_t      killed;
    size_t      survived;
    size_t      inconclusive;

    P101_TRACE_SCOPE(env);
    killed       = 0U;
    survived     = 0U;
    inconclusive = 0U;
    for(index = 0U; index < result_count; index++)
    {
        p101_call_result_2 = p101_strcmp(env, results[index].outcome, "killed");
        if(p101_call_result_2 == 0)
        {
            killed++;
        }
        else
        {
            p101_call_result_9 = p101_strcmp(env, results[index].outcome, "survived");
            if(p101_call_result_9 == 0)
            {
                survived++;
            }
            else
            {
                inconclusive++;
            }
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

            p101_call_result_3 = p101_strcmp(env, results[index].outcome, "survived");
            if(p101_call_result_3 != 0)
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
            p101_call_result_4 = p101_c_mutation_kind_name(candidate->kind);
            json_string(env, err, stdout, p101_call_result_4);
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

            p101_call_result_5 = p101_strcmp(env, results[index].outcome, "survived");
            if(p101_call_result_5 != 0)
            {
                continue;
            }
            candidate          = results[index].candidate;
            p101_call_result_6 = p101_c_mutation_kind_name(candidate->kind);
            p101_fprintf(env, err, stdout, "P101-MUTATION-001: %s:%zu: survived %s: %s -> %s\n", candidate->path, candidate->line, p101_call_result_6, candidate->original, candidate->replacement);
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
    const char *p101_call_result_7;
    const char *p101_call_result_8;
    size_t      index;

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
            p101_call_result_7 = p101_c_mutation_kind_name(candidate->kind);
            json_string(env, err, stdout, p101_call_result_7);
            p101_fputs(env, err, ",\"original\":", stdout);
            json_string(env, err, stdout, candidate->original);
            p101_fputs(env, err, ",\"replacement\":", stdout);
            json_string(env, err, stdout, candidate->replacement);
            p101_fputc(env, err, '}', stdout);
        }
        else
        {
            p101_call_result_8 = p101_c_mutation_kind_name(candidate->kind);
            p101_fprintf(env, err, stdout, "%s:%zu: %s: %s -> %s\n", candidate->path, candidate->line, p101_call_result_8, candidate->original, candidate->replacement);
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
