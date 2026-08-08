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

char *p101_mutation_copy_text(const struct p101_env *env, struct p101_error *err, const char *text)
{
    void  *p101_call_result_1;
    char  *copy;
    size_t length;

    P101_TRACE_SCOPE(env);
    length             = p101_strlen(env, text);
    p101_call_result_1 = p101_malloc(env, err, length + 1U);
    copy               = (char *)p101_call_result_1;
    if(copy != NULL)
    {
        p101_memcpy(env, copy, text, length + 1U);
    }
    return copy;
}

static bool operator_selected(const struct p101_env *env, const struct p101_mutation_arguments *arguments, enum p101_c_mutation_kind kind)
{
    size_t index;
    bool   selected;

    P101_TRACE_SCOPE(env);
    selected = arguments->operator_count == 0U;
    if(arguments->operator_count == 0U)
    {
        goto done;
    }
    for(index = 0U; index < arguments->operator_count; index++)
    {
        if(arguments->operators[index] == kind)
        {
            selected = true;
            break;
        }
    }

done:
    return selected;
}

bool p101_mutation_candidate_observer(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context)
{
    int                              p101_expression_result_4;
    bool                             p101_call_result_5;
    const char                      *p101_call_result_3;
    struct p101_mutation_candidates *candidates;
    struct p101_mutation_candidate  *candidate;
    char                             canonical_path[P101_MUTATION_PATH_SIZE];
    bool                             keep_going;

    P101_TRACE_SCOPE(env);
    candidates = (struct p101_mutation_candidates *)context;
    keep_going = true;
    if(record->kind != P101_C_ANALYSIS_MUTATION)
    {
        p101_expression_result_4 = 1;
    }
    else
    {
        p101_call_result_5 = operator_selected(env, candidates->arguments, record->mutation);
        if(!p101_call_result_5)
        {
            p101_expression_result_4 = 1;
        }
        else
        {
            p101_expression_result_4 = 0;
        }
    }
    if(p101_expression_result_4)
    {
        goto done;
    }
    if(candidates->count >= candidates->capacity)
    {
        goto done;
    }
    candidate = &candidates->items[candidates->count++];
    p101_memset(env, candidate, 0, sizeof(*candidate));
    p101_call_result_3 = p101_realpath(env, err, record->path, canonical_path);
    if(p101_call_result_3 == NULL)
    {
        keep_going = false;
        goto done;
    }
    p101_snprintf(env, err, candidate->path, sizeof(candidate->path), "%s", canonical_path);
    candidate->line        = record->line;
    candidate->start       = record->start_offset;
    candidate->end         = record->end_offset;
    candidate->kind        = record->mutation;
    candidate->original    = p101_mutation_copy_text(env, err, record->name);
    candidate->replacement = p101_mutation_copy_text(env, err, record->replacement);
    keep_going             = p101_error_has_no_error(err);

done:
    return keep_going;
}

void p101_mutation_destroy_candidates(const struct p101_env *env, struct p101_mutation_candidates *candidates)
{
    P101_TRACE_SCOPE(env);
    if(candidates->items != NULL)
    {
        size_t index;

        for(index = 0U; index < candidates->count; index++)
        {
            p101_free(env, candidates->items[index].replacement);
            p101_free(env, candidates->items[index].original);
        }
        p101_free(env, candidates->items);
    }
    candidates->items    = NULL;
    candidates->count    = 0U;
    candidates->capacity = 0U;
}
