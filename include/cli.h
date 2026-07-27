#ifndef TEMPLATE_C_PROGRAM_CLI_H
#define TEMPLATE_C_PROGRAM_CLI_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void process_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);

#endif    // TEMPLATE_C_PROGRAM_CLI_H
