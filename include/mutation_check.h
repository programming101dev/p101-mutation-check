#ifndef P101_MUTATION_CHECK_H
#define P101_MUTATION_CHECK_H

#include <p101_c_facts/analysis.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

enum
{
    P101_MUTATION_EXIT_FINDINGS = 1,
    P101_MUTATION_EXIT_TROUBLE  = 2,
    P101_MUTATION_PATH_SIZE     = 4096,
    P101_MUTATION_MAX_OPERATORS = 16,
    P101_MUTATION_MESSAGE_SIZE  = 256
};

enum p101_mutation_outcome
{
    P101_MUTATION_OUTCOME_INCONCLUSIVE = 0,
    P101_MUTATION_OUTCOME_SURVIVED,
    P101_MUTATION_OUTCOME_KILLED
};

struct p101_mutation_candidate
{
    char                      path[P101_MUTATION_PATH_SIZE];
    size_t                    line;
    size_t                    start;
    size_t                    end;
    char                     *original;
    char                     *replacement;
    enum p101_c_mutation_kind kind;
};

struct p101_mutation_arguments
{
    const char               *project;
    const char               *compile_database;
    enum p101_c_mutation_kind operators[P101_MUTATION_MAX_OPERATORS];
    size_t                    operator_count;
    size_t                    max_mutants;
    double                    timeout;
    bool                      list_only;
    bool                      json;
    char                    **test_command;
    size_t                    test_command_count;
};

struct p101_mutation_candidates
{
    struct p101_mutation_candidate       *items;
    size_t                                count;
    size_t                                capacity;
    const struct p101_mutation_arguments *arguments;
};

struct p101_mutation_result
{
    const struct p101_mutation_candidate *candidate;
    enum p101_mutation_outcome            outcome;
    int                                   return_code;
    bool                                  timed_out;
};

const char *p101_mutation_outcome_name(enum p101_mutation_outcome outcome);
void        p101_mutation_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int status);
bool        p101_mutation_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct p101_mutation_arguments *arguments);
bool        p101_mutation_candidate_observer(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context);
void        p101_mutation_destroy_candidates(const struct p101_env *env, struct p101_mutation_candidates *candidates);
char       *p101_mutation_copy_text(const struct p101_env *env, struct p101_error *err, const char *text);
bool        p101_mutation_copy_tree(const struct p101_env *env, struct p101_error *err, const char *source, const char *destination);
bool        p101_mutation_remove_tree(const struct p101_env *env, const char *path);
char       *p101_mutation_rewrite_path(const struct p101_env *env, struct p101_error *err, const char *project, const char *copy, const char *value);
bool        p101_mutation_apply_candidate(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_candidate *candidate, const char *copy);
int         p101_mutation_run_command(const struct p101_env *env, struct p101_error *err, char **command, const char *directory, double timeout, bool *timed_out);
bool        p101_mutation_execute(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_candidate *candidate, struct p101_mutation_result *result);
void        p101_mutation_report_results(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_result results[], size_t result_count);
void        p101_mutation_list_candidates(const struct p101_env *env, struct p101_error *err, const struct p101_mutation_arguments *arguments, const struct p101_mutation_candidates *candidates);

#endif
