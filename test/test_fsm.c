#include "unity.h"
#include "fsm.h"                 /* run_fsm(), struct arguments (via arguments.h) */
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stdlib.h>              /* EXIT_SUCCESS */

static struct p101_error *error;
static struct p101_env   *env;

void setUp(void)    { error = p101_error_create(false); env = p101_env_create(error, NULL); }
void tearDown(void) { p101_env_destroy(env); p101_error_destroy(error); }

/* run_fsm drives the state machine A -> B -> C -> exit. A clean run returns
   EXIT_SUCCESS and raises no error. This exercises run_fsm and, through the
   FSM, the state functions a/b/c in fsm.c. */
static void test_run_fsm_completes_cleanly(void)
{
    struct arguments args;
    int              ret;

    args.delay_str   = NULL;
    args.delay       = 0;
    args.verbose     = false;
    args.fsm_verbose = false;

    ret = run_fsm(env, error, &args);

    TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, ret);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_run_fsm_completes_cleanly);
    return UNITY_END();
}
