#include "unity.h"
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>

static struct p101_error *error;
static struct p101_env   *env;

void setUp(void)    { error = p101_error_create(false); env = p101_env_create(error, NULL); }
void tearDown(void) { p101_env_destroy(env); p101_error_destroy(error); }

static void test_strlen_counts_characters(void)
{
    TEST_ASSERT_EQUAL_size_t(5, p101_strlen(env, "hello"));
}

static void test_strlen_empty_is_zero(void)
{
    TEST_ASSERT_EQUAL_size_t(0, p101_strlen(env, ""));
    TEST_ASSERT_FALSE(p101_error_has_error(error));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_strlen_counts_characters);
    RUN_TEST(test_strlen_empty_is_zero);
    return UNITY_END();
}
