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

int main(int argc, char *argv[])
{
    struct p101_error              *err;
    struct p101_env                *env;
    struct p101_mutation_arguments  arguments;
    struct p101_mutation_candidates candidates;
    struct p101_c_analysis_options  scan_options;
    struct p101_mutation_result    *results;
    size_t                          index;
    int                             baseline;
    bool                            timed_out;
    int                             return_value;

    err          = p101_error_create(false);
    env          = p101_env_create(err, NULL);
    return_value = P101_MUTATION_EXIT_TROUBLE;
    if(!p101_mutation_parse_arguments(env, err, argc, argv, &arguments))
    {
        if(p101_error_has_no_error(err) && argc == 2 && (p101_strcmp(env, argv[1], "-h") == 0 || p101_strcmp(env, argv[1], "--help") == 0))
        {
            return_value = EXIT_SUCCESS;
        }
        if(p101_error_has_no_error(err) && !(argc == 2 && (p101_strcmp(env, argv[1], "-h") == 0 || p101_strcmp(env, argv[1], "--help") == 0)))
        {
            p101_mutation_usage(env, err, argv[0], P101_MUTATION_EXIT_TROUBLE);
        }
        goto done;
    }
    p101_memset(env, &candidates, 0, sizeof(candidates));
    p101_memset(env, &scan_options, 0, sizeof(scan_options));
    candidates.arguments = &arguments;
    candidates.capacity  = arguments.max_mutants;
    candidates.items     = (struct p101_mutation_candidate *)p101_calloc(env, err, candidates.capacity, sizeof(*candidates.items));
    if(candidates.items == NULL)
    {
        goto cleanup_candidates;
    }
    scan_options.compile_database      = arguments.compile_database;
    scan_options.paths                 = &arguments.project;
    scan_options.path_count            = 1U;
    scan_options.compile_database_only = true;
    if(!p101_c_analysis_scan(env, err, &scan_options, p101_mutation_candidate_observer, &candidates))
    {
        goto cleanup_candidates;
    }
    if(arguments.list_only)
    {
        p101_mutation_list_candidates(env, err, &arguments, &candidates);
        return_value = EXIT_SUCCESS;
        if(p101_error_has_error(err))
        {
            return_value = P101_MUTATION_EXIT_TROUBLE;
        }
        goto cleanup_candidates;
    }
    if(candidates.count == 0U)
    {
        p101_fputs(env,
                   err,
                   "p101-mutation-check: candidate discovery produced no mutants; "
                   "refusing a vacuous mutation-check pass\n",
                   stderr);
        goto cleanup_candidates;
    }

    baseline = p101_mutation_run_command(env, err, arguments.test_command, arguments.project, arguments.timeout, &timed_out);
    if(timed_out || baseline != 0 || p101_error_has_error(err))
    {
        const char *reason;

        reason = "failed";
        if(timed_out)
        {
            reason = "timed out";
        }
        p101_fprintf(env, err, stderr, "p101-mutation-check: baseline test command %s\n", reason);
        goto cleanup_candidates;
    }
    results = (struct p101_mutation_result *)p101_calloc(env, err, candidates.count, sizeof(*results));
    if(results == NULL)
    {
        goto cleanup_candidates;
    }
    for(index = 0U; index < candidates.count; index++)
    {
        if(!p101_mutation_execute(env, err, &arguments, &candidates.items[index], &results[index]))
        {
            p101_free(env, results);
            goto cleanup_candidates;
        }
    }
    p101_mutation_report_results(env, err, &arguments, results, candidates.count);
    return_value = EXIT_SUCCESS;
    for(index = 0U; index < candidates.count; index++)
    {
        if(p101_strcmp(env, results[index].outcome, "inconclusive") == 0)
        {
            return_value = P101_MUTATION_EXIT_TROUBLE;
        }
        else if(p101_strcmp(env, results[index].outcome, "survived") == 0 && return_value == EXIT_SUCCESS)
        {
            return_value = P101_MUTATION_EXIT_FINDINGS;
        }
    }
    p101_free(env, results);

cleanup_candidates:
    p101_mutation_destroy_candidates(env, &candidates);
done:
    if(p101_error_has_error(err))
    {
        p101_fprintf(env, NULL, stderr, "p101-mutation-check: %s\n", p101_error_get_message(err));
        return_value = P101_MUTATION_EXIT_TROUBLE;
    }
    p101_env_destroy(env);
    p101_error_destroy(err);
    return return_value;
}
