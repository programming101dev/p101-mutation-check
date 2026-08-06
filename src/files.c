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
    COPY_BUFFER_SIZE = 16384
};

static const mode_t PERMISSION_BITS = 0777U;

static bool ignored_name(const struct p101_env *env, const char *name)
{
    int                      p101_expression_result_17;
    int                      p101_expression_result_18;
    int                      p101_expression_result_19;
    int                      p101_expression_result_20;
    int                      p101_call_result_21;
    int                      p101_call_result_22;
    int                      p101_call_result_23;
    int                      p101_call_result_24;
    int                      p101_call_result_1;
    static const char *const ignored[] = {".git", ".pytest_cache", "__pycache__", "build", "compile_commands.json", "coverage", "debug", "profile"};
    size_t                   index;
    bool                     ignored_name_result;

    P101_TRACE_SCOPE(env);
    ignored_name_result = false;
    for(index = 0U; index < sizeof(ignored) / sizeof(ignored[0]); index++)
    {
        p101_call_result_1 = p101_strcmp(env, name, ignored[index]);
        if(p101_call_result_1 == 0)
        {
            ignored_name_result = true;
            break;
        }
    }
    p101_expression_result_17 = 0;
    if(!ignored_name_result)
    {
        p101_call_result_21 = p101_strncmp(env, name, "build-", sizeof("build-") - 1U);
        if(p101_call_result_21 == 0)
        {
            p101_expression_result_20 = 1;
        }
        else
        {
            p101_call_result_22 = p101_strncmp(env, name, "coverage-", sizeof("coverage-") - 1U);
            if(p101_call_result_22 == 0)
            {
                p101_expression_result_20 = 1;
            }
            else
            {
                p101_expression_result_20 = 0;
            }
        }
        if(p101_expression_result_20)
        {
            p101_expression_result_19 = 1;
        }
        else
        {
            p101_call_result_23 = p101_strncmp(env, name, "debug-", sizeof("debug-") - 1U);
            if(p101_call_result_23 == 0)
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
        else
        {
            p101_call_result_24 = p101_strncmp(env, name, "profile-", sizeof("profile-") - 1U);
            if(p101_call_result_24 == 0)
            {
                p101_expression_result_18 = 1;
            }
            else
            {
                p101_expression_result_18 = 0;
            }
        }
        if(p101_expression_result_18)
        {
            p101_expression_result_17 = 1;
        }
    }
    if(p101_expression_result_17)
    {
        ignored_name_result = true;
    }
    return ignored_name_result;
}

static bool copy_file(const struct p101_env *env, struct p101_error *err, const char *source, const char *destination, mode_t mode)
{
    int           p101_expression_result_25;
    bool          p101_call_result_26;
    int           input;
    int           output;
    unsigned char buffer[COPY_BUFFER_SIZE];
    ssize_t       count;
    bool          result;
    bool          no_error;

    P101_TRACE_SCOPE(env);
    result = false;
    output = -1;
    input  = p101_open(env, err, source, O_RDONLY);
    if(input < 0)
    {
        goto done;
    }
    output = p101_open(env, err, destination, O_WRONLY | O_CREAT | O_TRUNC, mode & PERMISSION_BITS);
    if(output < 0)
    {
        goto done;
    }
    for(;;)
    {
        ssize_t total;

        count = p101_read(env, err, input, buffer, sizeof(buffer));
        if(count <= 0)
        {
            break;
        }
        no_error = p101_error_has_no_error(err);
        if(!no_error)
        {
            break;
        }
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
    p101_expression_result_25 = 0;
    if(count == 0)
    {
        p101_call_result_26 = p101_error_has_no_error(err);
        if(p101_call_result_26)
        {
            p101_expression_result_25 = 1;
        }
    }
    if(p101_expression_result_25)
    {
        result = true;
    }

done:
    if(output >= 0)
    {
        p101_close(env, P101_ERROR_OPTIONAL, output);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the copy result.
    }
    if(input >= 0)
    {
        p101_close(env, P101_ERROR_OPTIONAL, input);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the copy result.
    }
    return result;
}

// NOLINTNEXTLINE(misc-no-recursion)
bool p101_mutation_copy_tree(const struct p101_env *env, struct p101_error *err, const char *source, const char *destination)
{
    int         p101_expression_result_27;
    int         p101_call_result_28;
    bool        p101_call_result_29;
    int         p101_expression_result_30;
    int         p101_expression_result_31;
    int         p101_call_result_32;
    int         p101_call_result_33;
    bool        p101_call_result_34;
    int         p101_expression_result_35;
    bool        p101_call_result_36;
    bool        p101_call_result_37;
    int         p101_call_result_2;
    bool        p101_call_result_3;
    bool        p101_call_result_4;
    int         p101_call_result_5;
    struct stat status;
    bool        result;
    bool        no_error;

    P101_TRACE_SCOPE(env);
    result             = false;
    p101_call_result_2 = p101_lstat(env, err, source, &status);
    if(p101_call_result_2 != 0)
    {
        goto done;
    }
    if(S_ISDIR(status.st_mode))
    {
        DIR           *directory;
        struct dirent *entry;

        directory                 = NULL;
        p101_call_result_28       = p101_mkdir(env, err, destination, status.st_mode & PERMISSION_BITS);
        p101_expression_result_27 = 0;
        if(p101_call_result_28 != 0)
        {
            p101_call_result_29 = p101_error_is_errno(err, EEXIST);
            if(!p101_call_result_29)
            {
                p101_expression_result_27 = 1;
            }
        }
        if(p101_expression_result_27)
        {
            goto done;
        }
        p101_call_result_3 = p101_error_has_error(err);
        if(p101_call_result_3)
        {
            p101_error_reset(err);
        }
        directory = p101_opendir(env, err, source);
        if(directory == NULL)
        {
            goto done;
        }
        result = true;
        for(;;)
        {
            char source_child[P101_MUTATION_PATH_SIZE];
            char destination_child[P101_MUTATION_PATH_SIZE];

            entry = p101_readdir(env, err, directory);
            if(entry == NULL)
            {
                break;
            }
            no_error = p101_error_has_no_error(err);
            if(!no_error)
            {
                break;
            }
            p101_call_result_32 = p101_strcmp(env, entry->d_name, ".");
            if(p101_call_result_32 == 0)
            {
                p101_expression_result_31 = 1;
            }
            else
            {
                p101_call_result_33 = p101_strcmp(env, entry->d_name, "..");
                if(p101_call_result_33 == 0)
                {
                    p101_expression_result_31 = 1;
                }
                else
                {
                    p101_expression_result_31 = 0;
                }
            }
            if(p101_expression_result_31)
            {
                p101_expression_result_30 = 1;
            }
            else
            {
                p101_call_result_34 = ignored_name(env, entry->d_name);
                if(p101_call_result_34)
                {
                    p101_expression_result_30 = 1;
                }
                else
                {
                    p101_expression_result_30 = 0;
                }
            }
            if(p101_expression_result_30)
            {
                continue;
            }
            p101_snprintf(env, err, source_child, sizeof(source_child), "%s/%s", source, entry->d_name);
            p101_snprintf(env, err, destination_child, sizeof(destination_child), "%s/%s", destination, entry->d_name);
            p101_call_result_36 = p101_error_has_error(err);
            if(p101_call_result_36)
            {
                p101_expression_result_35 = 1;
            }
            else
            {
                p101_call_result_37 = p101_mutation_copy_tree(env, err, source_child, destination_child);
                if(!p101_call_result_37)
                {
                    p101_expression_result_35 = 1;
                }
                else
                {
                    p101_expression_result_35 = 0;
                }
            }
            if(p101_expression_result_35)
            {
                result = false;
                break;
            }
        }
        p101_call_result_4 = p101_error_has_error(err);
        if(p101_call_result_4)
        {
            result = false;
        }
        p101_closedir(env, P101_ERROR_OPTIONAL, directory);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the traversal result.
    }
    else if(S_ISLNK(status.st_mode))
    {
        char    target[P101_MUTATION_PATH_SIZE];
        ssize_t length;

        length = p101_readlink(env, err, source, target, sizeof(target) - 1U);
        if(length >= 0)
        {
            target[length]     = '\0';
            p101_call_result_5 = p101_symlink(env, err, target, destination);
            result             = p101_call_result_5 == 0;
        }
    }
    else if(S_ISREG(status.st_mode))
    {
        result = copy_file(env, err, source, destination, status.st_mode);
    }
    else
    {
        result = true;
    }

done:
    return result;
}

// NOLINTNEXTLINE(misc-no-recursion)
bool p101_mutation_remove_tree(const struct p101_env *env, const char *path)
{
    int         p101_expression_result_38;
    int         p101_call_result_39;
    int         p101_call_result_40;
    int         p101_expression_result_41;
    bool        p101_call_result_42;
    int         p101_call_result_6;
    int         p101_call_result_7;
    int         p101_call_result_8;
    struct stat status;
    bool        result;

    P101_TRACE_SCOPE(env);
    result = false;
    /* P101_ERROR_OPTIONAL rationale: recursive cleanup reports failure through its boolean result. */
    p101_call_result_6 = p101_lstat(env, P101_ERROR_OPTIONAL, path, &status);
    if(p101_call_result_6 != 0)
    {
        goto done;
    }
    if(!S_ISDIR(status.st_mode))
    {
        /* P101_ERROR_OPTIONAL rationale: recursive cleanup reports failure through its boolean result. */
        p101_call_result_7 = p101_unlink(env, P101_ERROR_OPTIONAL, path);
        result             = p101_call_result_7 == 0;
    }
    else
    {
        DIR           *directory;
        struct dirent *entry;

        /* P101_ERROR_OPTIONAL rationale: recursive cleanup reports failure through its boolean result. */
        directory = p101_opendir(env, P101_ERROR_OPTIONAL, path);
        if(directory == NULL)
        {
            goto done;
        }
        result = true;
        /* P101_ERROR_OPTIONAL rationale: recursive cleanup reports failure through its boolean result. */
        for(;;)
        {
            char child[P101_MUTATION_PATH_SIZE];

            entry = p101_readdir(env, P101_ERROR_OPTIONAL, directory);
            if(entry == NULL)
            {
                break;
            }
            p101_call_result_39 = p101_strcmp(env, entry->d_name, ".");
            if(p101_call_result_39 == 0)
            {
                p101_expression_result_38 = 1;
            }
            else
            {
                p101_call_result_40 = p101_strcmp(env, entry->d_name, "..");
                if(p101_call_result_40 == 0)
                {
                    p101_expression_result_38 = 1;
                }
                else
                {
                    p101_expression_result_38 = 0;
                }
            }
            if(p101_expression_result_38)
            {
                continue;
            }
            child[0] = '\0';
            /* P101_ERROR_OPTIONAL rationale: an empty result records path construction failure. */
            p101_snprintf(env, P101_ERROR_OPTIONAL, child, sizeof(child), "%s/%s", path, entry->d_name);
            if(child[0] == '\0')
            {
                p101_expression_result_41 = 1;
            }
            else
            {
                p101_call_result_42 = p101_mutation_remove_tree(env, child);
                if(!p101_call_result_42)
                {
                    p101_expression_result_41 = 1;
                }
                else
                {
                    p101_expression_result_41 = 0;
                }
            }
            if(p101_expression_result_41)
            {
                result = false;
            }
        }
        p101_closedir(env, P101_ERROR_OPTIONAL, directory);    // P101_ERROR_OPTIONAL rationale: recursive cleanup preserves its boolean result.
        /* P101_ERROR_OPTIONAL rationale: recursive cleanup reports failure through its boolean result. */
        p101_call_result_8 = p101_rmdir(env, P101_ERROR_OPTIONAL, path);
        if(p101_call_result_8 != 0)
        {
            result = false;
        }
    }

done:
    return result;
}

char *p101_mutation_rewrite_path(const struct p101_env *env, struct p101_error *err, const char *project, const char *copy, const char *value)
{
    int    p101_expression_result_43;
    int    p101_expression_result_44;
    int    p101_expression_result_45;
    char  *p101_call_result_46;
    int    p101_call_result_47;
    int    p101_expression_result_48;
    size_t p101_call_result_9;
    size_t p101_call_result_10;
    void  *p101_call_result_11;
    char   canonical_value[P101_MUTATION_PATH_SIZE];
    char  *rewritten;
    size_t project_length;

    P101_TRACE_SCOPE(env);
    project_length = p101_strlen(env, project);
    /* P101_ERROR_OPTIONAL rationale: a path that cannot be canonicalized is left unchanged. */
    p101_expression_result_45 = 0;
    if(value[0] == '/')
    {
        p101_call_result_46 = p101_realpath(env, P101_ERROR_OPTIONAL, value, canonical_value);
        if(p101_call_result_46 != NULL)
        {
            p101_expression_result_45 = 1;
        }
    }
    p101_expression_result_44 = 0;
    if(p101_expression_result_45)
    {
        p101_call_result_47 = p101_strncmp(env, canonical_value, project, project_length);
        if(p101_call_result_47 == 0)
        {
            p101_expression_result_44 = 1;
        }
    }
    p101_expression_result_43 = 0;
    if(p101_expression_result_44)
    {
        if(canonical_value[project_length] == '/')
        {
            p101_expression_result_48 = 1;
        }
        else
        {
            if(canonical_value[project_length] == '\0')
            {
                p101_expression_result_48 = 1;
            }
            else
            {
                p101_expression_result_48 = 0;
            }
        }
        if(p101_expression_result_48)
        {
            p101_expression_result_43 = 1;
        }
    }
    if(p101_expression_result_43)
    {
        size_t length;

        p101_call_result_9  = p101_strlen(env, copy);
        p101_call_result_10 = p101_strlen(env, canonical_value + project_length);
        length              = p101_call_result_9 + p101_call_result_10 + 1U;
        p101_call_result_11 = p101_malloc(env, err, length);
        rewritten           = (char *)p101_call_result_11;
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
    int         p101_expression_result_49;
    int         p101_call_result_50;
    int         p101_expression_result_51;
    int         p101_call_result_52;
    int         p101_expression_result_53;
    size_t      p101_call_result_54;
    int         p101_expression_result_55;
    int         p101_expression_result_56;
    int         p101_expression_result_57;
    size_t      p101_call_result_58;
    int         p101_call_result_59;
    int         p101_expression_result_60;
    int         p101_expression_result_61;
    size_t      p101_call_result_62;
    size_t      p101_call_result_63;
    size_t      p101_call_result_64;
    int         p101_expression_result_65;
    bool        p101_call_result_66;
    char       *p101_call_result_12;
    int         p101_call_result_13;
    size_t      p101_call_result_14;
    size_t      p101_call_result_15;
    void       *p101_call_result_16;
    char        canonical_project[P101_MUTATION_PATH_SIZE];
    const char *relative;
    char        target[P101_MUTATION_PATH_SIZE];
    FILE       *stream;
    long        raw_size;
    size_t      size;
    char       *contents;
    bool        result;

    P101_TRACE_SCOPE(env);
    stream              = NULL;
    contents            = NULL;
    result              = false;
    p101_call_result_12 = p101_realpath(env, err, arguments->project, canonical_project);
    if(p101_call_result_12 == NULL)
    {
        goto done;
    }
    p101_call_result_14 = p101_strlen(env, canonical_project);
    p101_call_result_13 = p101_strncmp(env, candidate->path, canonical_project, p101_call_result_14);
    if(p101_call_result_13 != 0)
    {
        P101_ERROR_RAISE_USER(err, "Mutation candidate is outside the project.", 1);
        goto done;
    }
    p101_call_result_15 = p101_strlen(env, canonical_project);
    relative            = candidate->path + p101_call_result_15;
    if(*relative == '/')
    {
        relative++;
    }
    p101_snprintf(env, err, target, sizeof(target), "%s/%s", copy, relative);
    stream = p101_fopen(env, err, target, "rb");
    if(stream == NULL)
    {
        p101_expression_result_49 = 1;
    }
    else
    {
        p101_call_result_50 = p101_fseek(env, err, stream, 0L, SEEK_END);
        if(p101_call_result_50 != 0)
        {
            p101_expression_result_49 = 1;
        }
        else
        {
            p101_expression_result_49 = 0;
        }
    }
    if(p101_expression_result_49)
    {
        goto done;
    }
    raw_size = p101_ftell(env, err, stream);
    if(raw_size < 0L)
    {
        p101_expression_result_51 = 1;
    }
    else
    {
        p101_call_result_52 = p101_fseek(env, err, stream, 0L, SEEK_SET);
        if(p101_call_result_52 != 0)
        {
            p101_expression_result_51 = 1;
        }
        else
        {
            p101_expression_result_51 = 0;
        }
    }
    if(p101_expression_result_51)
    {
        goto done;
    }
    size                = (size_t)raw_size;
    p101_call_result_16 = p101_malloc(env, err, size + 1U);
    contents            = (char *)p101_call_result_16;
    if(contents == NULL)
    {
        p101_expression_result_53 = 1;
    }
    else
    {
        p101_call_result_54 = p101_fread(env, err, contents, 1U, size, stream);
        if(p101_call_result_54 != size)
        {
            p101_expression_result_53 = 1;
        }
        else
        {
            p101_expression_result_53 = 0;
        }
    }
    if(p101_expression_result_53)
    {
        goto done;
    }
    p101_fclose(env, err, stream);
    stream                    = NULL;
    p101_expression_result_57 = 0;
    if(candidate->end <= size)
    {
        if(candidate->start <= candidate->end)
        {
            p101_expression_result_57 = 1;
        }
    }
    p101_expression_result_56 = 0;
    if(p101_expression_result_57)
    {
        p101_call_result_58 = p101_strlen(env, candidate->original);
        if(candidate->end - candidate->start == p101_call_result_58)
        {
            p101_expression_result_56 = 1;
        }
    }
    p101_expression_result_55 = 0;
    if(p101_expression_result_56)
    {
        p101_call_result_59 = p101_memcmp(env, contents + candidate->start, candidate->original, candidate->end - candidate->start);
        if(p101_call_result_59 == 0)
        {
            p101_expression_result_55 = 1;
        }
    }
    if(p101_expression_result_55)
    {
        FILE *output;

        output = p101_fopen(env, err, target, "wb");
        if(output != NULL)
        {
            size_t prefix;
            size_t replacement_length;
            size_t suffix;

            prefix                    = candidate->start;
            replacement_length        = p101_strlen(env, candidate->replacement);
            suffix                    = size - candidate->end;
            p101_call_result_62       = p101_fwrite(env, err, contents, 1U, prefix, output);
            p101_expression_result_61 = 0;
            if(p101_call_result_62 == prefix)
            {
                p101_call_result_63 = p101_fwrite(env, err, candidate->replacement, 1U, replacement_length, output);
                if(p101_call_result_63 == replacement_length)
                {
                    p101_expression_result_61 = 1;
                }
            }
            p101_expression_result_60 = 0;
            if(p101_expression_result_61)
            {
                p101_call_result_64 = p101_fwrite(env, err, contents + candidate->end, 1U, suffix, output);
                if(p101_call_result_64 == suffix)
                {
                    p101_expression_result_60 = 1;
                }
            }
            if(p101_expression_result_60)
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
done:
    if(stream != NULL)
    {
        p101_fclose(env, P101_ERROR_OPTIONAL, stream);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the mutation failure.
    }
    p101_free(env, contents);
    p101_expression_result_65 = 0;
    if(result)
    {
        p101_call_result_66 = p101_error_has_no_error(err);
        if(p101_call_result_66)
        {
            p101_expression_result_65 = 1;
        }
    }
    result = p101_expression_result_65 != 0;
    return result;
}
