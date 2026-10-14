/* Regression checks for missing, unreadable and incomplete golden suites,
   and for the directory walk (a symlink to a directory, a nested directory
   that does not open). */
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef CLI_SOURCE
#define CLI_SOURCE "../bin/main.c"
#endif
#define main sole_cli_main
#include CLI_SOURCE
#undef main

static int failures = 0;

static void require(int yes, const char *name) {
  if (!yes) {
    fprintf(stderr, "FAIL runner: %s\n", name);
    failures++;
  }
}

static int command(int n, char **args) {
  const char *extra[16];
  return test_command(n, args, extra, 0);
}

static void write_source(const char *path) {
  FILE *out = fopen(path, "wb");
  if (out == NULL) exit(2);
  fputs("def answer : Nat := 1\n", out);
  if (fclose(out) != 0) exit(2);
}

static void write_golden(const char *path, const char *source) {
  FILE *out = fopen(path, "wb");
  if (out == NULL) exit(2);
  fprintf(out, "exit 0\n--- stdout\nCHECK %s defs=1 ok\n--- stderr\n", source);
  if (fclose(out) != 0) exit(2);
}

/* The temp dir template: $TMPDIR when it is set and not empty (one trailing
   "/" removed), else /tmp. Returns 0 when the template does not fit. */
/* S14 speed cases: a dependent codomain (id), mu indices that no binder
   reads (Agreement), a def with many references (two) and an application
   whose head stays neutral (observe n). */
static const char *kan_ok =
    "mu N : Type 0 with\n| zero : N\n| succ : N -> N\n"
    "mu Agreement : (0 a : Nat) -> (0 b : Nat) -> Type 0 with\n"
    "| same : (0 a : Nat) -> Agreement a a\n"
    "def rec observe : N -> Nat := fun (n : N) => case n as x in N return "
    "Nat with | zero => 0 | succ m => natAdd 1 (observe m)\n"
    "def id : (A : Type 0) -> A -> A := fun (A : Type 0) (x : A) => x\n"
    "def two : Nat := natAdd 1 1\n"
    "def dep : Agreement (id Nat two) 2 := same (natAdd two (natMul two 0))\n"
    "def many : Agreement (natAdd two (natAdd two (natAdd two two))) 8 := "
    "same 8\n"
    "def stuck : (n : N) -> Agreement (observe n) (observe n) := "
    "fun (n : N) => same (observe n)\n";
static const char *kan_bad =
    "mu N : Type 0 with\n| zero : N\n| succ : N -> N\n"
    "mu Agreement : (0 a : Nat) -> (0 b : Nat) -> Type 0 with\n"
    "| same : (0 a : Nat) -> Agreement a a\n"
    "def rec observe : N -> Nat := fun (n : N) => case n as x in N return "
    "Nat with | zero => 0 | succ m => natAdd 1 (observe m)\n"
    "def stuck : (n : N) -> Agreement (observe n) (observe (succ n)) := "
    "fun (n : N) => same (observe n)\n";
/* S15 memo cases (H6): a closed former (N) under many lambdas, and an open
   former (Agreement a a, it reads the bound a) at two values of a in one
   def. Each def has a new globals map, thus the two values must be in one
   def. A memo that keeps the value of the open former accepts wrong. */
#define KAN_LAN                                                              \
  "mu N : Type 0 with\n| zero : N\n| succ : N -> N\n"                        \
  "mu Agreement : (0 a : Nat) -> (0 b : Nat) -> Type 0 with\n"               \
  "| same : (0 a : Nat) -> Agreement a a\n"                                  \
  "def k : N -> N -> N -> N -> N := "                                        \
  "fun (a : N) (b : N) (c : N) (d : N) => succ d\n"                          \
  "def use : N := k zero (succ zero) zero (k zero zero (succ zero) zero)\n"  \
  "def lift : (a : Nat) -> Agreement a a := fun (a : Nat) => same a\n"       \
  "def one : Agreement 1 1 := lift 1\n"                                      \
  "def two : Agreement 2 2 := lift 2\n"                                     \
  "def keep : Agreement 1 1 -> Agreement 1 1 -> Agreement 1 1 := "           \
  "fun (x : Agreement 1 1) (y : Agreement 1 1) => y\n"                       \
  "def fine : Agreement 1 1 := keep (lift 1) one\n"
static const char *kan_lan_ok = KAN_LAN;
static const char *kan_lan_bad =
    KAN_LAN "def wrong : Agreement 1 1 := keep (lift 1) (lift 2)\n";
static char nested[SOLE_SOURCE_LIMIT + 1];

/* Write text to path, then check it (with --kan when kan is not 0). The
   result is the check exit code. */
static int check_text(const char *path, const char *text, int kan) {
  const char *extra[16];
  char *args[2];
  FILE *out = fopen(path, "wb");
  if (out == NULL) exit(2);
  fputs(text, out);
  if (fclose(out) != 0) exit(2);
  args[0] = "--kan";
  args[1] = (char *)path;
  return kan ? check_command(2, args, extra, 0)
             : check_command(1, args + 1, extra, 0);
}

/* The deepest nesting under SOLE_SOURCE_LIMIT: `prim 1 (prim 1 ( ... 0
   ... ))`, 11 bytes for each level (2975 levels). */
static void nested_source(const char *prim) {
  const char *head = "axiom Nat : Type 0\ndef answer : Nat := ";
  size_t levels = (SOLE_SOURCE_LIMIT - strlen(head) - 2) / 11;
  size_t at;
  char *end = nested + strlen(strcpy(nested, head));
  for (at = 0; at < levels; at++) end += sprintf(end, "%s 1 (", prim);
  *end++ = '0';
  for (at = 0; at < levels; at++) *end++ = ')';
  *end++ = '\n';
  *end = '\0';
}

static int temp_template(char *dir, size_t size) {
  const char *base = getenv("TMPDIR");
  size_t length;
  int written;
  if (base == NULL || base[0] == 0) base = "/tmp";
  length = strlen(base);
  if (base[length - 1] == '/') length--;
  written = snprintf(dir, size, "%.*s/sole-comb-runner-XXXXXX", (int)length,
                     base);
  return written >= 0 && (size_t)written < size;
}

int main(void) {
  char dir[1000];
  char source[1024];
  char golden[1100];
  char unreadable[1100];
  char empty[1024];
  char links[1024];
  char loop[1100];
  char locked[1024];
  char closed[1100];
  char *args[4];
  const char *invalid[] = {"0", "-1", "abc", "18446744073709551616000"};
  size_t at;
  TestCount count;
  if (!temp_template(dir, sizeof dir) || mkdtemp(dir) == NULL) return 2;
  snprintf(source, sizeof source, "%s/answer.sole-comb", dir);
  snprintf(golden, sizeof golden, "%s.check.out", source);
  snprintf(unreadable, sizeof unreadable, "%s.print.out", source);
  snprintf(empty, sizeof empty, "%s/empty", dir);
  snprintf(links, sizeof links, "%s/links", dir);
  snprintf(loop, sizeof loop, "%s/loop", links);
  snprintf(locked, sizeof locked, "%s/locked", dir);
  snprintf(closed, sizeof closed, "%s/closed", locked);
  if (mkdir(empty, 0700) != 0) return 2;
  write_source(source);
  write_golden(golden, source);

  count.runs = count.passes = count.fails = 0;
  test_source(source, &count);
  require(count.runs == 1 && count.passes == 1 && count.fails == 0,
          "one valid golden passes");
  args[0] = source;
  args[1] = "--expect";
  args[2] = "1";
  require(command(3, args) == 0, "the expected row count passes");
  args[2] = "2";
  require(command(3, args) == CLI_EXIT_FAIL, "a missing comparison fails");
  for (at = 0; at < sizeof invalid / sizeof invalid[0]; at++) {
    args[2] = (char *)invalid[at];
    require(command(3, args) == CLI_EXIT_USAGE, "invalid expected counts fail");
  }

  args[0] = empty;
  require(command(1, args) == CLI_EXIT_FAIL, "an empty suite fails");
  args[1] = source;
  require(command(2, args) == CLI_EXIT_FAIL,
          "an empty input fails alongside a passing suite");

  write_golden(unreadable, source);
  if (chmod(unreadable, 0000) != 0) return 2;
  count.runs = count.passes = count.fails = 0;
  test_source(source, &count);
  require(count.runs == 2 && count.passes == 1 && count.fails == 1,
          "an unreadable golden fails instead of being skipped");
  if (chmod(unreadable, 0600) != 0 || unlink(unreadable) != 0 ||
      unlink(golden) != 0) return 2;
  count.runs = count.passes = count.fails = 0;
  test_source(source, &count);
  require(count.runs == 1 && count.fails == 1,
          "a source without goldens fails");

  if (mkdir(links, 0700) != 0 || symlink(".", loop) != 0) return 2;
  count.runs = count.passes = count.fails = 0;
  test_walk(links, &count);
  require(count.runs == 1 && count.fails == 1,
          "a symlink to a directory fails and is not followed");

  if (mkdir(locked, 0700) != 0 || mkdir(closed, 0700) != 0 ||
      chmod(closed, 0000) != 0) return 2;
  count.runs = count.passes = count.fails = 0;
  test_walk(locked, &count);
  require(count.runs == 1 && count.fails == 1,
          "a nested directory that does not open fails");
  if (chmod(closed, 0700) != 0) return 2;

  write_golden(golden, source);
  if (unlink(loop) != 0 || symlink("loop", loop) != 0) return 2;
  count.runs = count.passes = count.fails = 0;
  test_walk(dir, &count);
  require(count.runs == 2 && count.passes == 1 && count.fails == 1,
          "a symlink with an unresolvable target fails beside a passing row");
  args[0] = dir;
  require(command(1, args) == CLI_EXIT_FAIL,
          "an unresolvable nested symlink makes the suite fail");
  if (unlink(loop) != 0 || chmod(locked, 0400) != 0) return 2;
  count.runs = count.passes = count.fails = 0;
  test_walk(dir, &count);
  require(count.runs == 2 && count.passes == 1 && count.fails == 1,
          "an entry whose metadata is inaccessible fails beside a passing row");
  require(command(1, args) == CLI_EXIT_FAIL,
          "an inaccessible nested entry makes the suite fail");
  if (chmod(locked, 0700) != 0 || unlink(golden) != 0) return 2;

  nested_source("natAdd");
  require(check_text(source, nested, 0) == 0,
          "the deepest nested natAdd checks");
  nested_source("natMul");
  require(check_text(source, nested, 0) == 0,
          "the deepest nested natMul checks");
  require(check_text(source, kan_ok, 1) == 0,
          "a dependent codomain, unread indices, a shared def and a neutral "
          "head check");
  require(check_text(source, kan_bad, 1) == CLI_EXIT_FAIL,
          "a neutral head with a different index fails");
  require(check_text(source, kan_lan_ok, 1) == 0,
          "a closed former under many lambdas and an open former check");
  require(check_text(source, kan_lan_bad, 1) == CLI_EXIT_FAIL,
          "an open former with a different index fails");

  if (rmdir(links) != 0 || rmdir(closed) != 0 ||
      rmdir(locked) != 0) return 2;
  if (unlink(source) != 0 || rmdir(empty) != 0 || rmdir(dir) != 0) return 2;
  printf("RUNNER failures=%d\n", failures);
  return failures == 0 ? 0 : 1;
}
