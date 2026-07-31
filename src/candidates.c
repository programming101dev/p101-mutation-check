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

char *p101_mutation_copy_text(const struct p101_env *env, struct p101_error *err, const char *text)
{
    char  *copy;
    size_t length;

    P101_TRACE_SCOPE(env);
    length = p101_strlen(env, text);
    copy   = (char *)p101_malloc(env, err, length + 1U);
    if(copy != NULL)
    {
        p101_memcpy(env, copy, text, length + 1U);
    }
    return copy;
}

static bool operator_selected(const struct p101_env *env, const struct p101_mutation_arguments *arguments, enum p101_c_mutation_kind kind)
{
    const char *name;
    size_t      index;

    P101_TRACE_SCOPE(env);
    if(arguments->operator_count == 0U)
    {
        return true;
    }
    name = p101_c_mutation_kind_name(kind);
    for(index = 0U; index < arguments->operator_count; index++)
    {
        if(p101_strcmp(env, arguments->operators[index], name) == 0)
        {
            return true;
        }
    }
    return false;
}

bool p101_mutation_candidate_observer(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context)
{
    struct p101_mutation_candidates *candidates;
    struct p101_mutation_candidate  *candidate;
    char                             canonical_path[P101_MUTATION_PATH_SIZE];

    P101_TRACE_SCOPE(env);
    candidates = (struct p101_mutation_candidates *)context;
    if(record->kind != P101_C_ANALYSIS_MUTATION || !operator_selected(env, candidates->arguments, record->mutation))
    {
        return true;
    }
    if(candidates->count >= candidates->capacity)
    {
        return true;
    }
    candidate = &candidates->items[candidates->count++];
    p101_memset(env, candidate, 0, sizeof(*candidate));
    if(p101_realpath(env, err, record->path, canonical_path) == NULL)
    {
        return false;
    }
    p101_snprintf(env, err, candidate->path, sizeof(candidate->path), "%s", canonical_path);
    candidate->line        = record->line;
    candidate->start       = record->start_offset;
    candidate->end         = record->end_offset;
    candidate->kind        = record->mutation;
    candidate->original    = p101_mutation_copy_text(env, err, record->name);
    candidate->replacement = p101_mutation_copy_text(env, err, record->replacement);
    return p101_error_has_no_error(err);
}

void p101_mutation_destroy_candidates(const struct p101_env *env, struct p101_mutation_candidates *candidates)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    if(candidates->items == NULL)
    {
        candidates->count    = 0U;
        candidates->capacity = 0U;
        return;
    }
    for(index = 0U; index < candidates->count; index++)
    {
        p101_free(env, candidates->items[index].replacement);
        p101_free(env, candidates->items[index].original);
    }
    p101_free(env, candidates->items);
    candidates->items    = NULL;
    candidates->count    = 0U;
    candidates->capacity = 0U;
}
