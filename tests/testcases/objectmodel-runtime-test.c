/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#include <noct/noct.h>
#include <stdio.h>

static const char source[] =
	"func main(): int {\n"
	"  var a = [1, 2, 3];\n"
	"  var d = {x: 10};\n"
	"  Array.resize(a, 5);\n"
	"  a[3] = 4; a[4] = 5; d.x = d.x + a[4];\n"
	"  return d.x + Array.size(a);\n"
	"}\n";

static int
run_vm(NoctVM *vm, NoctEnv *env)
{
	NoctValue result = NOCT_ZERO;
	int value;

	if (!noct_enter_vm(env, "main", 0, NULL, &result) ||
	    !noct_get_int(env, &result, &value))
		return 0;
	(void)vm;
	return value == 20;
}

int
main(void)
{
	NoctConfig config0;
	NoctConfig config1;
	NoctConfig invalid;
	NoctVM *vm0 = NULL;
	NoctVM *vm1 = NULL;
	NoctVM *bad_vm = (NoctVM *)1;
	NoctEnv *env0 = NULL;
	NoctEnv *env1 = NULL;
	NoctEnv *bad_env = (NoctEnv *)1;

	noct_set_default_config(&config0);
	config0.object_model = NOCT_OBJECT_MODEL_SINGLE;
	config0.jit_enable = false;
	config1 = config0;
	config1.object_model = NOCT_OBJECT_MODEL_MULTI;
	invalid = config0;
	invalid.object_model = 2;

	if (noct_create_vm(&bad_vm, &bad_env, &invalid) ||
	    bad_vm != NULL || bad_env != NULL)
		return 1;
	if (!noct_create_vm(&vm0, &env0, &config0) ||
	    !noct_create_vm(&vm1, &env1, &config1))
		return 2;
	if (!noct_register_source(env0, "objectmodel-runtime-test-m0.noct", source) ||
	    !noct_register_source(env1, "objectmodel-runtime-test-m1.noct", source))
		return 3;
	if (!run_vm(vm0, env0) || !run_vm(vm1, env1) ||
	    !run_vm(vm0, env0) || !run_vm(vm1, env1))
		return 4;
	if (!noct_destroy_vm(vm1) || !noct_destroy_vm(vm0))
		return 5;
	puts("VM-local object model test: PASS");
	return 0;
}
