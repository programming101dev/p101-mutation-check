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
    COPY_BUFFER_SIZE = 16384
};

static const mode_t PERMISSION_BITS = 0777U;

static bool ignored_name(const struct p101_env *env, const char *name)
{
    static const char *const ignored[] = {".git", ".pytest_cache", "__pycache__", "build", "compile_commands.json", "coverage", "debug", "profile"};
    size_t                   index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < sizeof(ignored) / sizeof(ignored[0]); index++)
    {
        if(p101_strcmp(env, name, ignored[index]) == 0)
        {
            return true;
        }
    }
    if(p101_strncmp(env, name, "build-", sizeof("build-") - 1U) == 0 || p101_strncmp(env, name, "coverage-", sizeof("coverage-") - 1U) == 0 || p101_strncmp(env, name, "debug-", sizeof("debug-") - 1U) == 0 ||
       p101_strncmp(env, name, "profile-", sizeof("profile-") - 1U) == 0)
    {
        return true;
    }
    return false;
}

static bool copy_file(const struct p101_env *env, struct p101_error *err, const char *source, const char *destination, mode_t mode)
{
    int           input;
    int           output;
    unsigned char buffer[COPY_BUFFER_SIZE];
    ssize_t       count;
    bool          result;

    P101_TRACE_SCOPE(env);
    result = false;
    input  = p101_open(env, err, source, O_RDONLY);
    if(input < 0)
    {
        return false;
    }
    output = p101_open(env, err, destination, O_WRONLY | O_CREAT | O_TRUNC, mode & PERMISSION_BITS);
    if(output < 0)
    {
        p101_close(env, NULL, input);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: cleanup preserves the open failure.
        return false;
    }
    while((count = p101_read(env, err, input, buffer, sizeof(buffer))) > 0 && p101_error_has_no_error(err))
    {
        ssize_t total;

        total = 0;
        while(total < count)
        {
            ssize_t written;

            written = p101_write(env, err, output, buffer + total, (size_t)(count - total));
            if(written <= 0)
            {
                break;
            }
            total += written;
        }
        if(total != count)
        {
            break;
        }
    }
    if(count == 0 && p101_error_has_no_error(err))
    {
        result = true;
    }
    p101_close(env, NULL, output);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: cleanup preserves the copy result.
    p101_close(env, NULL, input);     // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: cleanup preserves the copy result.
    return result;
}

// NOLINTNEXTLINE(misc-no-recursion)
bool p101_mutation_copy_tree(const struct p101_env *env, struct p101_error *err, const char *source, const char *destination)
{
    struct stat status;

    P101_TRACE_SCOPE(env);
    if(p101_lstat(env, err, source, &status) != 0)
    {
        return false;
    }
    if(S_ISDIR(status.st_mode))
    {
        DIR           *directory;
        struct dirent *entry;
        bool           result;

        if(p101_mkdir(env, err, destination, status.st_mode & PERMISSION_BITS) != 0 && !p101_error_is_errno(err, EEXIST))
        {
            return false;
        }
        if(p101_error_has_error(err))
        {
            p101_error_reset(err);
        }
        directory = p101_opendir(env, err, source);
        if(directory == NULL)
        {
            return false;
        }
        result = true;
        while((entry = p101_readdir(env, err, directory)) != NULL && p101_error_has_no_error(err))
        {
            char source_child[P101_MUTATION_PATH_SIZE];
            char destination_child[P101_MUTATION_PATH_SIZE];

            if(p101_strcmp(env, entry->d_name, ".") == 0 || p101_strcmp(env, entry->d_name, "..") == 0 || ignored_name(env, entry->d_name))
            {
                continue;
            }
            p101_snprintf(env, err, source_child, sizeof(source_child), "%s/%s", source, entry->d_name);
            p101_snprintf(env, err, destination_child, sizeof(destination_child), "%s/%s", destination, entry->d_name);
            if(p101_error_has_error(err) || !p101_mutation_copy_tree(env, err, source_child, destination_child))
            {
                result = false;
                break;
            }
        }
        if(p101_error_has_error(err))
        {
            result = false;
        }
        p101_closedir(env, NULL, directory);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: cleanup preserves the traversal result.
        return result;
    }
    if(S_ISLNK(status.st_mode))
    {
        char    target[P101_MUTATION_PATH_SIZE];
        ssize_t length;

        length = p101_readlink(env, err, source, target, sizeof(target) - 1U);
        if(length < 0)
        {
            return false;
        }
        target[length] = '\0';
        return p101_symlink(env, err, target, destination) == 0;
    }
    if(S_ISREG(status.st_mode))
    {
        return copy_file(env, err, source, destination, status.st_mode);
    }
    return true;
}

// NOLINTNEXTLINE(misc-no-recursion)
bool p101_mutation_remove_tree(const struct p101_env *env, const char *path)
{
    struct stat status;

    P101_TRACE_SCOPE(env);
    /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: recursive cleanup reports failure through its boolean result. */
    if(p101_lstat(env, NULL, path, &status) != 0)
    {
        return false;
    }
    if(!S_ISDIR(status.st_mode))
    {
        /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: recursive cleanup reports failure through its boolean result. */
        return p101_unlink(env, NULL, path) == 0;
    }
    {
        DIR           *directory;
        struct dirent *entry;
        bool           result;

        /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: recursive cleanup reports failure through its boolean result. */
        directory = p101_opendir(env, NULL, path);
        if(directory == NULL)
        {
            return false;
        }
        result = true;
        /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: recursive cleanup reports failure through its boolean result. */
        while((entry = p101_readdir(env, NULL, directory)) != NULL)
        {
            char child[P101_MUTATION_PATH_SIZE];

            if(p101_strcmp(env, entry->d_name, ".") == 0 || p101_strcmp(env, entry->d_name, "..") == 0)
            {
                continue;
            }
            child[0] = '\0';
            /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: an empty result records path construction failure. */
            p101_snprintf(env, NULL, child, sizeof(child), "%s/%s", path, entry->d_name);
            if(child[0] == '\0' || !p101_mutation_remove_tree(env, child))
            {
                result = false;
            }
        }
        p101_closedir(env, NULL, directory);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: recursive cleanup preserves its boolean result.
        /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: recursive cleanup reports failure through its boolean result. */
        if(p101_rmdir(env, NULL, path) != 0)
        {
            result = false;
        }
        return result;
    }
}

char *p101_mutation_rewrite_path(const struct p101_env *env, struct p101_error *err, const char *project, const char *copy, const char *value)
{
    char   canonical_value[P101_MUTATION_PATH_SIZE];
    char  *rewritten;
    size_t project_length;

    P101_TRACE_SCOPE(env);
    project_length = p101_strlen(env, project);
    /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: a path that cannot be canonicalized is left unchanged. */
    if(value[0] == '/' && p101_realpath(env, NULL, value, canonical_value) != NULL && p101_strncmp(env, canonical_value, project, project_length) == 0 && (canonical_value[project_length] == '/' || canonical_value[project_length] == '\0'))
    {
        size_t length;

        length    = p101_strlen(env, copy) + p101_strlen(env, canonical_value + project_length) + 1U;
        rewritten = (char *)p101_malloc(env, err, length);
        if(rewritten != NULL)
        {
            p101_snprintf(env, err, rewritten, length, "%s%s", copy, canonical_value + project_length);
        }
    }
    else
    {
        rewritten = p101_mutation_copy_text(env, err, value);
    }
    return rewritten;
}

bool p101_mutation_apply_candidate(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_candidate *candidate, const char *copy)
{
    char        canonical_project[P101_MUTATION_PATH_SIZE];
    const char *relative;
    char        target[P101_MUTATION_PATH_SIZE];
    FILE       *stream;
    long        raw_size;
    size_t      size;
    char       *contents;
    bool        result;

    P101_TRACE_SCOPE(env);
    if(p101_realpath(env, err, arguments->project, canonical_project) == NULL)
    {
        return false;
    }
    if(p101_strncmp(env, candidate->path, canonical_project, p101_strlen(env, canonical_project)) != 0)
    {
        P101_ERROR_RAISE_USER(err, "Mutation candidate is outside the project.", 1);
        return false;
    }
    relative = candidate->path + p101_strlen(env, canonical_project);
    if(*relative == '/')
    {
        relative++;
    }
    p101_snprintf(env, err, target, sizeof(target), "%s/%s", copy, relative);
    stream = p101_fopen(env, err, target, "rb");
    if(stream == NULL || p101_fseek(env, err, stream, 0L, SEEK_END) != 0)
    {
        return false;
    }
    raw_size = p101_ftell(env, err, stream);
    if(raw_size < 0L || p101_fseek(env, err, stream, 0L, SEEK_SET) != 0)
    {
        p101_fclose(env, NULL, stream);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: cleanup preserves the seek failure.
        return false;
    }
    size     = (size_t)raw_size;
    contents = (char *)p101_malloc(env, err, size + 1U);
    if(contents == NULL || p101_fread(env, err, contents, 1U, size, stream) != size)
    {
        p101_fclose(env, NULL, stream);    // P101_ERROR_CONTRACT_ALLOW_NO_ERROR: cleanup preserves the read failure.
        p101_free(env, contents);
        return false;
    }
    p101_fclose(env, err, stream);
    result = false;
    if(candidate->end <= size && candidate->start <= candidate->end && candidate->end - candidate->start == p101_strlen(env, candidate->original) && p101_memcmp(env, contents + candidate->start, candidate->original, candidate->end - candidate->start) == 0)
    {
        FILE *output;

        output = p101_fopen(env, err, target, "wb");
        if(output != NULL)
        {
            size_t prefix;
            size_t replacement_length;
            size_t suffix;

            prefix             = candidate->start;
            replacement_length = p101_strlen(env, candidate->replacement);
            suffix             = size - candidate->end;
            if(p101_fwrite(env, err, contents, 1U, prefix, output) == prefix && p101_fwrite(env, err, candidate->replacement, 1U, replacement_length, output) == replacement_length &&
               p101_fwrite(env, err, contents + candidate->end, 1U, suffix, output) == suffix)
            {
                result = true;
            }
            p101_fclose(env, err, output);
        }
    }
    else
    {
        P101_ERROR_RAISE_USER(err, "Mutation candidate source changed.", 1);
    }
    p101_free(env, contents);
    if(result && p101_error_has_no_error(err))
    {
        return true;
    }
    return false;
}
