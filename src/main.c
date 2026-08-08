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

int main(int argc, char *argv[])
{
    int                             p101_expression_result_10;
    int                             p101_expression_result_11;
    bool                            p101_call_result_12;
    int                             p101_expression_result_13;
    int                             p101_call_result_14;
    int                             p101_call_result_15;
    int                             p101_expression_result_16;
    bool                            p101_call_result_17;
    int                             p101_expression_result_18;
    int                             p101_expression_result_19;
    int                             p101_call_result_20;
    int                             p101_call_result_21;
    int                             p101_expression_result_22;
    int                             p101_expression_result_23;
    bool                            p101_call_result_24;
    int                             p101_expression_result_25;
    bool                            p101_call_result_9;
    bool                            p101_call_result_1;
    void                           *p101_call_result_2;
    bool                            p101_call_result_3;
    bool                            p101_call_result_4;
    void                           *p101_call_result_5;
    bool                            p101_call_result_6;
    const char                     *p101_call_result_8;
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

    err                = p101_error_create(false);
    env                = p101_env_create(err, NULL);
    return_value       = P101_MUTATION_EXIT_TROUBLE;
    p101_call_result_1 = p101_mutation_parse_arguments(env, err, argc, argv, &arguments);
    if(!p101_call_result_1)
    {
        p101_call_result_12       = p101_error_has_no_error(err);
        p101_expression_result_11 = 0;
        if(p101_call_result_12)
        {
            if(argc == 2)
            {
                p101_expression_result_11 = 1;
            }
        }
        p101_expression_result_10 = 0;
        if(p101_expression_result_11)
        {
            p101_call_result_14 = p101_strcmp(env, argv[1], "-h");
            if(p101_call_result_14 == 0)
            {
                p101_expression_result_13 = 1;
            }
            else
            {
                p101_call_result_15 = p101_strcmp(env, argv[1], "--help");
                if(p101_call_result_15 == 0)
                {
                    p101_expression_result_13 = 1;
                }
                else
                {
                    p101_expression_result_13 = 0;
                }
            }
            if(p101_expression_result_13)
            {
                p101_expression_result_10 = 1;
            }
        }
        if(p101_expression_result_10)
        {
            return_value = EXIT_SUCCESS;
        }
        p101_call_result_17       = p101_error_has_no_error(err);
        p101_expression_result_16 = 0;
        if(p101_call_result_17)
        {
            p101_expression_result_18 = 0;
            if(argc == 2)
            {
                p101_call_result_20 = p101_strcmp(env, argv[1], "-h");
                if(p101_call_result_20 == 0)
                {
                    p101_expression_result_19 = 1;
                }
                else
                {
                    p101_call_result_21 = p101_strcmp(env, argv[1], "--help");
                    if(p101_call_result_21 == 0)
                    {
                        p101_expression_result_19 = 1;
                    }
                    else
                    {
                        p101_expression_result_19 = 0;
                    }
                }
                if(p101_expression_result_19)
                {
                    p101_expression_result_18 = 1;
                }
            }
            if(!p101_expression_result_18)
            {
                p101_expression_result_16 = 1;
            }
        }
        if(p101_expression_result_16)
        {
            p101_mutation_usage(env, err, argv[0], P101_MUTATION_EXIT_TROUBLE);
        }
        goto done;
    }
    p101_memset(env, &candidates, 0, sizeof(candidates));
    p101_memset(env, &scan_options, 0, sizeof(scan_options));
    candidates.arguments = &arguments;
    candidates.capacity  = arguments.max_mutants;
    p101_call_result_2   = p101_calloc(env, err, candidates.capacity, sizeof(*candidates.items));
    candidates.items     = (struct p101_mutation_candidate *)p101_call_result_2;
    if(candidates.items == NULL)
    {
        goto cleanup_candidates;
    }
    scan_options.compile_database      = arguments.compile_database;
    scan_options.paths                 = &arguments.project;
    scan_options.path_count            = 1U;
    scan_options.compile_database_only = true;
    p101_call_result_3                 = p101_c_analysis_scan(env, err, &scan_options, p101_mutation_candidate_observer, &candidates);
    if(!p101_call_result_3)
    {
        goto cleanup_candidates;
    }
    if(arguments.list_only)
    {
        p101_mutation_list_candidates(env, err, &arguments, &candidates);
        return_value       = EXIT_SUCCESS;
        p101_call_result_4 = p101_error_has_error(err);
        if(p101_call_result_4)
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
    if(timed_out)
    {
        p101_expression_result_23 = 1;
    }
    else
    {
        if(baseline != 0)
        {
            p101_expression_result_23 = 1;
        }
        else
        {
            p101_expression_result_23 = 0;
        }
    }
    if(p101_expression_result_23)
    {
        p101_expression_result_22 = 1;
    }
    else
    {
        p101_call_result_24 = p101_error_has_error(err);
        if(p101_call_result_24)
        {
            p101_expression_result_22 = 1;
        }
        else
        {
            p101_expression_result_22 = 0;
        }
    }
    if(p101_expression_result_22)
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
    p101_call_result_5 = p101_calloc(env, err, candidates.count, sizeof(*results));
    results            = (struct p101_mutation_result *)p101_call_result_5;
    if(results == NULL)
    {
        goto cleanup_candidates;
    }
    for(index = 0U; index < candidates.count; index++)
    {
        p101_call_result_6 = p101_mutation_execute(env, err, &arguments, &candidates.items[index], &results[index]);
        if(!p101_call_result_6)
        {
            p101_free(env, results);
            goto cleanup_candidates;
        }
    }
    p101_mutation_report_results(env, err, &arguments, results, candidates.count);
    return_value = EXIT_SUCCESS;
    for(index = 0U; index < candidates.count; index++)
    {
        p101_expression_result_25 = 0;
        if(results[index].outcome == P101_MUTATION_OUTCOME_SURVIVED)
        {
            if(return_value == EXIT_SUCCESS)
            {
                p101_expression_result_25 = 1;
            }
        }
        if(results[index].outcome == P101_MUTATION_OUTCOME_INCONCLUSIVE)
        {
            return_value = P101_MUTATION_EXIT_TROUBLE;
        }
        else if(p101_expression_result_25)
        {
            return_value = P101_MUTATION_EXIT_FINDINGS;
        }
    }
    p101_free(env, results);

cleanup_candidates:
    p101_mutation_destroy_candidates(env, &candidates);
done:
{
    p101_call_result_9 = p101_error_has_error(err);
    if(p101_call_result_9)
    {
        /* P101_ERROR_OPTIONAL rationale: diagnostic output must not overwrite the reported failure. */
        p101_call_result_8 = p101_error_get_message(err);
        p101_fprintf(env, P101_ERROR_OPTIONAL, stderr, "p101-mutation-check: %s\n", p101_call_result_8);
        return_value = P101_MUTATION_EXIT_TROUBLE;
    }
}
    p101_env_destroy(env);
    p101_error_destroy(err);
    return return_value;
}
