# dev/c.mk: build rules of the C host (TinyCC port, PORT-PLAN g).
#
# Run from the repository root: make -f dev/c.mk c-check
# c-check compiles each C twin with tcc -Wall -Werror into build/obj/.
# c-build links build/sole-comb (the public CLI, bin/main.c).
# The Makefile keeps the Bend targets until the flip (stage S9).

TCC ?= tcc
C_FLAGS = -Wall -Werror -Ilib
C_OBJ_DIR = build/obj

# S1 (both parts): runtime, foundation, kernel_error, kernel_budget,
# kernel_level, kernel_literal, kernel_quantity, kernel_shape, kernel_term,
# kernel_positivity, kernel_pp, kernel_value, kernel_recursor_group.
C_SOURCES = \
	lib/runtime.c \
	lib/foundation.c \
	lib/kernel_error.c \
	lib/kernel_budget.c \
	lib/kernel_level.c \
	lib/kernel_literal.c \
	lib/kernel_quantity.c \
	lib/kernel_shape.c \
	lib/kernel_term.c \
	lib/kernel_positivity.c \
	lib/kernel_pp.c \
	lib/kernel_value.c \
	lib/kernel_recursor_group.c \
	lib/kernel_rules.c \
	lib/kernel_conv.c \
	lib/kernel_prim.c \
	lib/kernel_global.c \
	lib/kernel_eval.c \
	lib/kernel_recursor.c \
	lib/kernel_recursor_uniform.c \
	lib/kernel_recursor_instance.c \
	lib/kernel_recursor_route.c \
	lib/kernel_recursor_motive.c \
	lib/kernel_recursor_branch.c \
	lib/kernel_recursor_body.c \
	lib/kernel_recursor_cover.c \
	lib/kernel_recursor_elim.c \
	lib/kernel_check.c \
	lib/erase_eterm.c \
	lib/erase_runtime.c \
	lib/erase_dependencies.c \
	lib/erase_type.c \
	lib/erase_family.c \
	lib/erase_pair.c \
	lib/erase_sum.c \
	lib/erase_mu.c \
	lib/erase_core.c \
	lib/surface_token.c \
	lib/surface_lexer.c \
	lib/surface_source.c \
	lib/surface_syntax.c \
	lib/surface_record.c \
	lib/surface_order.c \
	lib/surface_totality.c \
	lib/surface_elab_program.c \
	lib/surface_constructor.c \
	lib/surface_constructor_infer.c \
	lib/surface_sugar.c \
	lib/surface_family.c \
	lib/surface_parser.c \
	lib/surface_elab.c \
	lib/surface_program.c \
	lib/surface_check.c \
	bin/main.c \
	test/pinfront/token.c \
	test/pinfront/lexer.c \
	test/pinfront/parser.c \
	test/pinfront/elab.c \
	test/pinfront/program.c
# S2: kernel_rules (sessions 2-5b: Bend lines 1-1376), kernel_conv and
# kernel_prim (session 6).
# S3 session 1: kernel_global, kernel_eval.
# S3 session 2: kernel_recursor*.
# S3 session 3: kernel_check.
# S4 session 1: erase_eterm, erase_runtime, erase_dependencies.
# S4 session 1b: erase_type.
# S4 session 2: erase_family, erase_pair, erase_sum, erase_mu.
# S4 session 3: erase_core.
# S5 session 1: surface_token, surface_lexer, surface_source, surface_syntax.
# S5 session 2: surface_record, surface_order, surface_totality.
# S5 session 3: surface_elab_program, surface_constructor, surface_constructor_infer, surface_sugar, surface_family.
# S6 session 1: surface_parser.
# S6 session 2: surface_elab.
# S6 session 3: surface_program, surface_check, bin/main.
# S7 session 1: test/pinfront token, lexer (syntax shares the surface twin).
# S7 session 2: test/pinfront parser.
# S7 session 3: test/pinfront elab, elab_program.
# S7 session 4: test/pinfront order, totality, program.

C_OBJECTS = $(patsubst test/pinfront/%.c,$(C_OBJ_DIR)/pinfront_%.o,$(patsubst bin/%.c,$(C_OBJ_DIR)/%.o,$(patsubst lib/%.c,$(C_OBJ_DIR)/%.o,$(C_SOURCES))))

.PHONY: c-check c-build c-test
c-check: $(C_OBJECTS)

# c-build links the public CLI from all C sources (PORT-PLAN g).
c-build: build/sole-comb

c-test: build/test-runner
	./build/test-runner

build/test-runner: test/runner.c $(C_SOURCES) lib/sole.h | $(C_OBJ_DIR)
	$(TCC) $(C_FLAGS) -o $@ test/runner.c $(filter-out bin/main.c,$(C_SOURCES)) -lpthread

build/sole-comb: $(C_SOURCES) lib/sole.h | $(C_OBJ_DIR)
	$(TCC) $(C_FLAGS) -o $@ $(C_SOURCES) -lpthread

$(C_OBJ_DIR)/%.o: lib/%.c lib/sole.h | $(C_OBJ_DIR)
	$(TCC) $(C_FLAGS) -c $< -o $@

$(C_OBJ_DIR)/%.o: bin/%.c lib/sole.h | $(C_OBJ_DIR)
	$(TCC) $(C_FLAGS) -c $< -o $@

# The test/pinfront C twins (stage S7: the internal --kan front end).
$(C_OBJ_DIR)/pinfront_%.o: test/pinfront/%.c lib/sole.h | $(C_OBJ_DIR)
	$(TCC) $(C_FLAGS) -c $< -o $@

$(C_OBJ_DIR):
	mkdir -p $@
