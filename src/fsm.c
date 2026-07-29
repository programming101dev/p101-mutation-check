#include "fsm.h"
#include "errors.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_fsm/fsm.h>
#include <p101_posix/p101_unistd.h>

static void a(const struct p101_env *env, struct p101_error *err, void *arg, struct p101_fsm_effect_sink *sink, struct p101_fsm_decision *decision);
static void b(const struct p101_env *env, struct p101_error *err, void *arg, struct p101_fsm_effect_sink *sink, struct p101_fsm_decision *decision);
static void c(const struct p101_env *env, struct p101_error *err, void *arg, struct p101_fsm_effect_sink *sink, struct p101_fsm_decision *decision);
static void will_change_state_notifier_func(const struct p101_env *env, struct p101_error *err, const struct p101_fsm_info *info, p101_fsm_state_t from_state_id, p101_fsm_state_t to_state_id);
static void did_change_state_notifier_func(const struct p101_env *env, struct p101_error *err, const struct p101_fsm_info *info, p101_fsm_state_t from_state_id, p101_fsm_state_t to_state_id, p101_fsm_state_t next_state_id);
static void bad_change_state_notifier_func(const struct p101_env *env, struct p101_error *err, const struct p101_fsm_info *info, p101_fsm_state_t from_state_id, p101_fsm_state_t to_state_id);

enum states
{
    A = P101_FSM_USER_START,    // 2
    B,
    C,
};

int run_fsm(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct p101_error                      *fsm_err;
    struct p101_env                        *fsm_env;
    struct p101_fsm_info                   *fsm = NULL;
    p101_fsm_run_result                     fsm_result;
    struct p101_fsm_step_result             last_step;
    static const struct p101_fsm_transition transitions[] = {
        {P101_FSM_INIT, A, a},
        {A,             B, b},
        {B,             C, c}
    };
    unsigned int delay;

    P101_TRACE(env);
    fsm_err = p101_error_create(args->fsm_verbose);
    fsm_env = p101_env_create(fsm_err, NULL);

    if(p101_error_has_error(fsm_err))
    {
        goto done;
    }

    fsm = p101_fsm_info_create(env, err, "fsm", fsm_env, fsm_err, transitions, sizeof(transitions) / sizeof(transitions[0]), NULL);

    if(p101_error_has_error(fsm_err))
    {
        goto done;
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(args->fsm_verbose)
    {
        p101_fsm_info_set_bad_change_state_notifier(fsm, bad_change_state_notifier_func);
        p101_fsm_info_set_will_change_state_notifier(fsm, will_change_state_notifier_func);
        p101_fsm_info_set_did_change_state_notifier(fsm, did_change_state_notifier_func);
    }

    delay      = args->delay;
    fsm_result = p101_fsm_run(fsm, &delay, NULL, &last_step);
    if(fsm_result != P101_FSM_RUN_EXITED && p101_error_has_no_error(err) && p101_error_has_no_error(fsm_err))
    {
        P101_ERROR_RAISE_USER(err, "FSM stopped before exit", ERR_FSM);
    }
done:
    p101_fsm_info_destroy(env, fsm_err, &fsm);

    if(p101_error_has_error(fsm_err))
    {
        const char *msg = p101_error_get_message(fsm_err);

        P101_ERROR_RAISE_USER(err, msg, ERR_FSM);
    }

    p101_env_destroy(fsm_env);
    p101_error_destroy(fsm_err);

    if(p101_error_has_error(err))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void a(const struct p101_env *env, struct p101_error *err, void *arg, struct p101_fsm_effect_sink *sink, struct p101_fsm_decision *decision)    // cppcheck-suppress constParameterCallback
{
    const unsigned int *delay;

    P101_TRACE(env);
    (void)sink;
    delay = (unsigned int *)arg;
    p101_printf(env, err, "a called\n");
    p101_sleep(env, *delay);

    p101_fsm_decide_transition(decision, B);
}

static void b(const struct p101_env *env, struct p101_error *err, void *arg, struct p101_fsm_effect_sink *sink, struct p101_fsm_decision *decision)    // cppcheck-suppress constParameterCallback
{
    const unsigned int *delay;

    P101_TRACE(env);
    (void)sink;
    delay = (unsigned int *)arg;
    p101_printf(env, err, "b called\n");
    p101_sleep(env, *delay);

    p101_fsm_decide_transition(decision, C);
}

static void c(const struct p101_env *env, struct p101_error *err, void *arg, struct p101_fsm_effect_sink *sink, struct p101_fsm_decision *decision)    // cppcheck-suppress constParameterCallback
{
    const unsigned int *delay;

    P101_TRACE(env);
    (void)sink;
    delay = (unsigned int *)arg;
    p101_printf(env, err, "c called\n");
    p101_sleep(env, *delay);

    p101_fsm_decide_exit(decision);
}

static void will_change_state_notifier_func(const struct p101_env *env, struct p101_error *err, const struct p101_fsm_info *info, p101_fsm_state_t from_state_id, p101_fsm_state_t to_state_id)    // cppcheck-suppress constParameterCallback
{
    P101_TRACE(env);
    p101_printf(env, err, "%s will change from %d to %d\n", p101_fsm_info_get_name(env, info), from_state_id, to_state_id);
}

static void did_change_state_notifier_func(const struct p101_env *env, struct p101_error *err, const struct p101_fsm_info *info, p101_fsm_state_t from_state_id, p101_fsm_state_t to_state_id,
                                           p101_fsm_state_t next_state_id)    // cppcheck-suppress constParameterCallback
{
    P101_TRACE(env);
    (void)next_state_id;
    p101_printf(env, err, "%s did change from %d to %d\n", p101_fsm_info_get_name(env, info), from_state_id, to_state_id);
}

static void bad_change_state_notifier_func(const struct p101_env *env, struct p101_error *err, const struct p101_fsm_info *info, p101_fsm_state_t from_state_id, p101_fsm_state_t to_state_id)    // cppcheck-suppress constParameterCallback
{
    P101_TRACE(env);
    p101_printf(env, err, "%s can't change from %d to %d\n", p101_fsm_info_get_name(env, info), from_state_id, to_state_id);
}
