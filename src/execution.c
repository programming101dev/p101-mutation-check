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
    PROCESS_SIGNAL_BASE = 128
};

static const double NANOSECONDS_PER_SECOND = 1000000000.0;
static const long   POLL_NANOSECONDS       = 10000000L;

struct command_copy
{
    char  *directory;
    char **arguments;
    size_t argument_count;
};

static bool command_observer(const struct p101_env *env, struct p101_error *err, const struct p101_c_compile_command *command, void *context)
{
    struct command_copy *copy;
    size_t               index;

    P101_TRACE_SCOPE(env);
    copy                 = (struct command_copy *)context;
    copy->directory      = p101_mutation_copy_text(env, err, command->directory);
    copy->argument_count = command->argument_count;
    copy->arguments      = (char **)p101_calloc(env, err, command->argument_count + 2U, sizeof(*copy->arguments));
    for(index = 0U; index < command->argument_count && p101_error_has_no_error(err); index++)
    {
        copy->arguments[index] = p101_mutation_copy_text(env, err, command->arguments[index]);
    }
    return p101_error_has_no_error(err);
}

static void destroy_command(const struct p101_env *env, struct command_copy *command)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < command->argument_count; index++)
    {
        p101_free(env, command->arguments[index]);
    }
    p101_free(env, (void *)command->arguments);
    p101_free(env, command->directory);
}

static bool compile_option_takes_value(const struct p101_env *env, const char *argument)
{
    P101_TRACE_SCOPE(env);
    if(p101_strcmp(env, argument, "-o") == 0 || p101_strcmp(env, argument, "-MF") == 0 || p101_strcmp(env, argument, "-MT") == 0 || p101_strcmp(env, argument, "-MQ") == 0)
    {
        return true;
    }
    return false;
}

static bool is_compile_output_option(const struct p101_env *env, const char *argument)
{
    P101_TRACE_SCOPE(env);
    if((argument[0] == '-' && argument[1] == 'o' && p101_strcmp(env, argument, "-ObjC") != 0) || p101_strncmp(env, argument, "-MF", sizeof("-MF") - 1U) == 0 || p101_strncmp(env, argument, "-MT", sizeof("-MT") - 1U) == 0 ||
       p101_strncmp(env, argument, "-MQ", sizeof("-MQ") - 1U) == 0)
    {
        return true;
    }
    return false;
}

static bool build_compile_command(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_candidate *candidate, const char *copy, struct command_copy *command)
{
    struct command_copy source;
    const char         *canonical_project;
    char                project_path[P101_MUTATION_PATH_SIZE];
    size_t              read_index;
    size_t              write_index;

    P101_TRACE_SCOPE(env);
    p101_memset(env, &source, 0, sizeof(source));
    p101_memset(env, command, 0, sizeof(*command));
    if(!p101_c_facts_with_compile_command(env, err, arguments->compile_database, candidate->path, command_observer, &source))
    {
        return false;
    }
    canonical_project = p101_realpath(env, err, arguments->project, project_path);
    if(canonical_project == NULL)
    {
        destroy_command(env, &source);
        return false;
    }
    command->arguments = (char **)p101_calloc(env, err, source.argument_count + 2U, sizeof(*command->arguments));
    command->directory = p101_mutation_rewrite_path(env, err, project_path, copy, source.directory);
    write_index        = 0U;
    for(read_index = 0U; read_index < source.argument_count && p101_error_has_no_error(err); read_index++)
    {
        const char *value;

        value = source.arguments[read_index];
        if(p101_strcmp(env, value, "-c") == 0)
        {
            continue;
        }
        if(compile_option_takes_value(env, value))
        {
            read_index++;
            continue;
        }
        if(is_compile_output_option(env, value))
        {
            continue;
        }
        command->arguments[write_index++] = p101_mutation_rewrite_path(env, err, project_path, copy, value);
    }
    command->arguments[write_index++] = p101_mutation_copy_text(env, err, "-fsyntax-only");
    command->argument_count           = write_index;
    destroy_command(env, &source);
    return p101_error_has_no_error(err);
}

int p101_mutation_run_command(const struct p101_env *env, struct p101_error *err, char *const command[], const char *directory, double timeout, bool *timed_out)
{
    pid_t           child;
    struct timespec start;
    struct timespec now;
    struct timespec pause_time;
    int             status;

    P101_TRACE_SCOPE(env);
    *timed_out = false;
    child      = p101_fork(env, err);
    if(child < 0)
    {
        return -1;
    }
    if(child == 0)
    {
        if(directory != NULL &&
           /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: the child reports setup failure only through its exit status. */
           p101_chdir(env, NULL, directory) != 0)
        {
            p101_exit_immediately(env, P101_MUTATION_EXIT_TROUBLE);
        }
        /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: the child reports exec failure only through its exit status. */
        (void)p101_execvp(env, NULL, command[0], command);
        p101_exit_immediately(env, P101_MUTATION_EXIT_TROUBLE);
    }
    p101_clock_gettime(env, err, CLOCK_MONOTONIC, &start);
    pause_time.tv_sec  = 0;
    pause_time.tv_nsec = POLL_NANOSECONDS;
    for(;;)
    {
        pid_t  waited;
        double elapsed;

        waited = p101_waitpid(env, err, child, &status, WNOHANG);
        if(waited == child)
        {
            break;
        }
        if(waited < 0 || p101_error_has_error(err))
        {
            return -1;
        }
        p101_clock_gettime(env, err, CLOCK_MONOTONIC, &now);
        elapsed = (double)(now.tv_sec - start.tv_sec) + ((double)(now.tv_nsec - start.tv_nsec) / NANOSECONDS_PER_SECOND);
        if(elapsed >= timeout)
        {
            *timed_out = true;
            p101_kill(env, NULL, child, SIGKILL);          // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: best-effort timeout cleanup.
            p101_waitpid(env, NULL, child, &status, 0);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: best-effort timeout cleanup.
            return -1;
        }
        p101_nanosleep(env, NULL, &pause_time, NULL);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: an interrupted poll simply retries.
    }
    if(WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    if(WIFSIGNALED(status))
    {
        return PROCESS_SIGNAL_BASE + WTERMSIG(status);
    }
    return -1;
}

bool p101_mutation_execute(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_candidate *candidate, struct p101_mutation_result *result)
{
    char                temp[] = "/tmp/p101-mutation-check.XXXXXX";
    char                copy[P101_MUTATION_PATH_SIZE];
    struct command_copy compile_command;
    char              **test_command;
    char                canonical_project[P101_MUTATION_PATH_SIZE];
    size_t              index;
    int                 status;
    bool                timed_out;
    bool                success;
    bool                temp_created;
    bool                completed;

    P101_TRACE_SCOPE(env);
    p101_memset(env, result, 0, sizeof(*result));
    p101_memset(env, &compile_command, 0, sizeof(compile_command));
    result->candidate = candidate;
    test_command      = NULL;
    temp_created      = false;
    completed         = false;
    if(p101_mkdtemp(env, err, temp) == NULL)
    {
        goto cleanup;
    }
    temp_created = true;
    if(p101_realpath(env, err, arguments->project, canonical_project) == NULL)
    {
        goto cleanup;
    }
    p101_snprintf(env, err, copy, sizeof(copy), "%s/project", temp);
    success = p101_mutation_copy_tree(env, err, canonical_project, copy);
    if(success)
    {
        success = p101_mutation_apply_candidate(env, err, arguments, candidate, copy);
    }
    if(success)
    {
        success = build_compile_command(env, err, arguments, candidate, copy, &compile_command);
    }
    if(!success)
    {
        goto cleanup;
    }
    status = p101_mutation_run_command(env, err, compile_command.arguments, compile_command.directory, arguments->timeout, &timed_out);
    if(timed_out || status != 0)
    {
        result->outcome     = "inconclusive";
        result->return_code = status;
        result->timed_out   = timed_out;
        completed           = true;
        goto cleanup;
    }
    test_command = (char **)p101_calloc(env, err, arguments->test_command_count + 1U, sizeof(*test_command));
    for(index = 0U; index < arguments->test_command_count && p101_error_has_no_error(err); index++)
    {
        test_command[index] = p101_mutation_rewrite_path(env, err, canonical_project, copy, arguments->test_command[index]);
    }
    if(p101_error_has_error(err))
    {
        goto cleanup;
    }
    status              = p101_mutation_run_command(env, err, test_command, copy, arguments->timeout, &timed_out);
    result->return_code = status;
    result->timed_out   = timed_out;
    if(timed_out)
    {
        result->outcome = "inconclusive";
    }
    else if(status == 0)
    {
        result->outcome = "survived";
    }
    else
    {
        result->outcome = "killed";
    }
    completed = true;

cleanup:
    for(index = 0U; test_command != NULL && index < arguments->test_command_count; index++)
    {
        p101_free(env, test_command[index]);
    }
    p101_free(env, (void *)test_command);
    destroy_command(env, &compile_command);
    if(temp_created)
    {
        (void)p101_mutation_remove_tree(env, temp);
    }
    return completed;
}
