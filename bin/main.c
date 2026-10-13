/* bin/main.c: the public CLI of dev/cli.py with one in-process host (S6
   session 3b). bin/sole-comb.bend is the hex endpoint of the Bend
   toolchain; the C host reads the source file itself and calls
   surface_check_source or surface_check_erased_source on the big stack
   (PORT-PLAN hazard 4). The usage and help texts are the argparse bytes of
   Python 3.14 at 80 columns (kit capture S6-s3b-CLI.txt). `check --kan`
   (S7 session 5) runs the internal pinfront front end in place of the
   public one: see the kan section. `test PATH...` (S8 session 1) runs the
   golden rows beside each source in this process: see the test section.
   An answer goes through byte sinks (the sink section), so that the CLI
   and the test verb share one check path and one kan path. */
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "sole.h"

/* Exit codes of dev/cli.py. */
#define CLI_EXIT_FAIL 1   /* CHECK PATH FAIL */
#define CLI_EXIT_IO 2     /* E-IO, E-HOST */
#define CLI_EXIT_SOURCE 5 /* E-SOURCE-SIZE, E-SOURCE-NOT-YET */
#define CLI_EXIT_USAGE 64 /* usage errors, E-CLI, no command */

#define CLI_TEXT(x) #x
#define CLI_DECIMAL(x) CLI_TEXT(x)

/* The top usage and help with the test verb (S8 session 1, deviation 14
   from dev/cli.py, which has no test verb): argparse puts the help column
   at 24 for a 22 byte choice list, and so does this text. */
static const char top_usage[] =
    "usage: sole-comb [-h] {check,build,run,test} ...\n";

static const char top_help_body[] =
    "\n"
    "Check .sole-comb source with the public compiler.\n"
    "\n"
    "positional arguments:\n"
    "  {check,build,run,test}\n"
    "    check               parse and type-check a source file\n"
    "    build               pending the erasure and WebAssembly backend\n"
    "    run                 pending the erasure and WebAssembly backend\n"
    "    test                compare the golden files beside each source\n"
    "\n"
    "options:\n"
    "  -h, --help            show this help message and exit\n";

static const char check_usage[] =
    "usage: sole-comb check [-h] [--print] [--erased]\n"
    "                       [--host {bun,node-worker,native}]\n"
    "                       file\n";

static const char check_help_body[] =
    "\n"
    "positional arguments:\n"
    "  file\n"
    "\n"
    "options:\n"
    "  -h, --help            show this help message and exit\n"
    "  --print               also print the checked kernel definitions\n"
    "  --erased              print type-directed erased definitions "
    "(ordinary\n"
    "                        functions and Nat)\n"
    "  --host {bun,node-worker,native}\n"
    "                        override the development execution host\n";

static int same(const char *a, const char *b) { return strcmp(a, b) == 0; }

static int starts_with(const char *word, const char *prefix) {
  for (; *prefix != '\0'; word++, prefix++)
    if (*word != *prefix) return 0;
  return 1;
}

/* An argparse option word: it starts with '-' and is not '-' alone. */
static int is_option(const char *word) {
  return word[0] == '-' && word[1] != '\0';
}

static int is_help(const char *word) {
  return same(word, "-h") || same(word, "--help");
}

/* ---------------------------------------------------------------- usage */

static void put_byte(FILE *out, unsigned char c) { fwrite(&c, 1, 1, out); }

/* One byte of the Python repr of a str. A byte from 0x80 stays as it is
   (deviation: Python escapes the non-printable code points). */
static void put_repr_byte(FILE *out, unsigned char c, unsigned char quote) {
  static const char hex[] = "0123456789abcdef";
  char escape[4];
  switch (c) {
  case '\\': fputs("\\\\", out); return;
  case '\n': fputs("\\n", out); return;
  case '\r': fputs("\\r", out); return;
  case '\t': fputs("\\t", out); return;
  default: break;
  }
  if (c == quote) {
    put_byte(out, '\\');
    put_byte(out, c);
    return;
  }
  if (c < 0x20 || c == 0x7f) {
    escape[0] = '\\';
    escape[1] = 'x';
    escape[2] = hex[c >> 4];
    escape[3] = hex[c & 0x0f];
    fwrite(escape, 1, 4, out);
    return;
  }
  put_byte(out, c);
}

/* The Python repr of a str (argparse invalid choice): single quotes,
   unless the word has a single quote and no double quote. */
static void put_repr(FILE *out, const char *word) {
  const char *at;
  int single = 0;
  int twin = 0;
  unsigned char quote;
  for (at = word; *at != '\0'; at++) {
    single |= *at == '\'';
    twin |= *at == '"';
  }
  quote = single && !twin ? '"' : '\'';
  put_byte(out, quote);
  for (at = word; *at != '\0'; at++)
    put_repr_byte(out, (unsigned char)*at, quote);
  put_byte(out, quote);
}

/* argparse error (cli.py prints the usage, then "sole-comb: MESSAGE"). */
static int usage_error(const char *usage, const char *message) {
  fputs(usage, stderr);
  fputs("sole-comb: ", stderr);
  fputs(message, stderr);
  fputs("\n", stderr);
  return CLI_EXIT_USAGE;
}

static int invalid_choice(const char *usage, const char *argument,
                          const char *word, const char *choices) {
  fputs(usage, stderr);
  fputs("sole-comb: argument ", stderr);
  fputs(argument, stderr);
  fputs(": invalid choice: ", stderr);
  put_repr(stderr, word);
  fputs(" (choose from ", stderr);
  fputs(choices, stderr);
  fputs(")\n", stderr);
  return CLI_EXIT_USAGE;
}

static int unrecognized(const char *const *words, int count) {
  int at;
  fputs(top_usage, stderr);
  fputs("sole-comb: unrecognized arguments:", stderr);
  for (at = 0; at < count; at++) {
    fputs(" ", stderr);
    fputs(words[at], stderr);
  }
  fputs("\n", stderr);
  return CLI_EXIT_USAGE;
}

static int help(FILE *out, const char *usage, const char *body, int code) {
  fputs(usage, out);
  fputs(body, out);
  return code;
}

/* ---------------------------------------------------------------- path */

/* str(pathlib.Path(file)): empty and "." parts go; two leading slashes
   stay, one or three and more give one; no part and no root gives ".". */
static const char *pathlib_form(const char *file) {
  size_t length = strlen(file);
  char *out = (char *)arena_alloc(length + 2);
  size_t at = 0;
  size_t put = 0;
  size_t root;
  while (file[at] == '/') at++;
  if (at == 2) out[put++] = '/';
  if (at > 0) out[put++] = '/';
  root = put;
  while (file[at] != '\0') {
    size_t start = at;
    while (file[at] != '\0' && file[at] != '/') at++;
    if (at - start > 1 || (at - start == 1 && file[start] != '.')) {
      size_t copy;
      if (put > root) out[put++] = '/';
      for (copy = start; copy < at; copy++) out[put++] = file[copy];
    }
    while (file[at] == '/') at++;
  }
  if (put == 0) out[put++] = '.';
  out[put] = '\0';
  return out;
}

/* The suffix test of cli.py on the last part: it ends with ".sole-comb"
   and has more bytes than the suffix. */
static int has_source_suffix(const char *path) {
  static const char suffix[] = ".sole-comb";
  const size_t suffix_length = sizeof suffix - 1;
  const char *name = path;
  const char *at;
  size_t length;
  for (at = path; *at != '\0'; at++)
    if (*at == '/') name = at + 1;
  length = strlen(name);
  return length > suffix_length && same(name + length - suffix_length, suffix);
}

/* ---------------------------------------------------------------- sink */

/* The bytes of one stream (stdout or stderr) of an answer, in the arena,
   doubled by a copy when full (as read_all). The check path and the kan
   path write their answer into sinks: the CLI flushes them to the streams
   at the end (answer_flush); the test verb compares them with a golden. */
typedef struct {
  unsigned char *items;
  size_t size;
  size_t room;
} Sink;

static Sink sink_new(void) {
  Sink sink;
  sink.room = 256;
  sink.items = (unsigned char *)arena_alloc(sink.room);
  sink.size = 0;
  return sink;
}

static void sink_bytes(Sink *sink, const unsigned char *bytes, size_t count) {
  if (count > sink->room - sink->size) {
    size_t room = sink->room * 2;
    unsigned char *bigger;
    while (count > room - sink->size) room *= 2;
    bigger = (unsigned char *)arena_alloc(room);
    memmove(bigger, sink->items, sink->size);
    sink->items = bigger;
    sink->room = room;
  }
  memmove(sink->items + sink->size, bytes, count);
  sink->size += count;
}

static void sink_puts(Sink *sink, const char *ascii) {
  sink_bytes(sink, (const unsigned char *)ascii, strlen(ascii));
}

/* A Text as UTF-8 (the write_utf8 encoding). */
static void sink_text(Sink *sink, Text text) {
  U32 at;
  for (at = 0; at < text.size; at++) {
    unsigned char bytes[4];
    sink_bytes(sink, bytes, utf8_encode(text.items[at], bytes));
  }
}

/* The two streams of one answer. */
typedef struct {
  Sink out;
  Sink err;
} Answer;

static Answer answer_new(void) {
  Answer answer;
  answer.out = sink_new();
  answer.err = sink_new();
  return answer;
}

static void answer_flush(const Answer *answer) {
  fwrite(answer->out.items, 1, answer->out.size, stdout);
  fwrite(answer->err.items, 1, answer->err.size, stderr);
}

/* ---------------------------------------------------------------- answer */

/* The check runs on the big stack: the elab, order, totality, kernel and
   erase recursion follow the term depth. */
typedef struct {
  Text source;
  int erased;
  Text answer;
} CheckJob;

static void *check_job(void *argument) {
  CheckJob *job = (CheckJob *)argument;
  job->answer = job->erased ? surface_check_erased_source(job->source)
                            : surface_check_source(job->source);
  return NULL;
}

static int has_prefix(Text text, const char *ascii) {
  U32 at;
  for (at = 0; ascii[at] != '\0'; at++)
    if (at >= text.size || text.items[at] != (unsigned char)ascii[at])
      return 0;
  return 1;
}

/* The index of the first "\n" at or after from; text.size when none. */
static U32 newline_at(Text text, U32 from) {
  U32 at;
  for (at = from; at < text.size; at++)
    if (text.items[at] == '\n') return at;
  return text.size;
}

/* A decimal count of the answer: 0, or [1-9][0-9]*, in [from, to). */
static int is_count(Text text, U32 from, U32 to) {
  U32 at;
  if (to <= from) return 0;
  if (to - from > 1 && text.items[from] == '0') return 0;
  for (at = from; at < to; at++)
    if (text.items[at] < '0' || text.items[at] > '9') return 0;
  return 1;
}

static void put_line_start(Sink *out, const char *path) {
  sink_puts(out, "CHECK ");
  sink_puts(out, path);
}

/* The answer split of cli.py. response is "FAIL\n" error, or "OK\n" count
   "\n" form (the Bend IO.print newline that cli.py removes is not there). */
static int report(Answer *answer, const char *path, int show, Text response) {
  U32 first;
  U32 second;
  if (has_prefix(response, "FAIL\n")) {
    put_line_start(&answer->err, path);
    sink_puts(&answer->err, " FAIL ");
    sink_text(&answer->err, text_drop(response, 5));
    sink_puts(&answer->err, "\n");
    return CLI_EXIT_FAIL;
  }
  first = newline_at(response, 0);
  second =
      first < response.size ? newline_at(response, first + 1) : response.size;
  if (second >= response.size || first != 2 || !has_prefix(response, "OK") ||
      !is_count(response, first + 1, second)) {
    sink_puts(&answer->err, "E-HOST: malformed compiler response\n");
    return CLI_EXIT_IO;
  }
  put_line_start(&answer->out, path);
  sink_puts(&answer->out, " defs=");
  sink_text(&answer->out, text_drop(text_take(response, second), first + 1));
  sink_puts(&answer->out, " ok\n");
  if (show) sink_text(&answer->out, text_drop(response, second + 1));
  return 0;
}

/* ---------------------------------------------------------------- check */

static int io_error(Answer *answer, const char *path, int code) {
  sink_puts(&answer->err, "E-IO: ");
  sink_puts(&answer->err, path);
  sink_puts(&answer->err, ": ");
  sink_puts(&answer->err, strerror(code));
  sink_puts(&answer->err, "\n");
  return CLI_EXIT_IO;
}

static int check_file(Answer *answer, const char *path, int show, int erased) {
  unsigned char *bytes;
  size_t size;
  FILE *in;
  CheckJob job;
  if (!has_source_suffix(path)) {
    sink_puts(&answer->err,
              "E-CLI: source files must use the .sole-comb suffix\n");
    return CLI_EXIT_USAGE;
  }
  in = fopen(path, "rb");
  if (in == NULL) return io_error(answer, path, errno);
  bytes = (unsigned char *)arena_alloc(SOLE_SOURCE_LIMIT + 1);
  size = fread(bytes, 1, SOLE_SOURCE_LIMIT + 1, in);
  if (ferror(in)) {
    int code = errno;
    fclose(in);
    return io_error(answer, path, code);
  }
  fclose(in);
  if (size > SOLE_SOURCE_LIMIT) {
    sink_puts(&answer->err, "E-SOURCE-SIZE: ");
    sink_puts(&answer->err, path);
    sink_puts(&answer->err,
              ": this source-checking increment supports at most "
              CLI_DECIMAL(SOLE_SOURCE_LIMIT) " bytes\n");
    return CLI_EXIT_SOURCE;
  }
  job.source = text_of_bytes(bytes, size);
  job.erased = erased;
  job.answer = text_end();
  if (!run_on_big_stack(check_job, &job)) {
    sink_puts(&answer->err, "E-HOST: the check thread did not run to the end\n");
    return CLI_EXIT_IO;
  }
  return report(answer, path, show, job.answer);
}

/* ---------------------------------------------------------------- kan */

/* `check --kan FILE`: the internal pinfront front end (test/pinfront, the
   Kanon .kan syntax) in place of the public surface front end, with the
   answer bytes of the deleted OCaml adapter test/program-oracle.ml (kit
   oracle/kan, S7 session 1): no "CHECK PATH" line and no defs count.
   check: exit 0 and no output. --print: the checked_form on stdout.
   --erased: the erased text on stdout (it wins over --print, as in
   check_command). A parse, elab or erase error: its line on stderr, exit
   1. The path has no suffix test, and the read has no SOLE_SOURCE_LIMIT
   (five agreement-*.kan sources are above 64 KiB). */
typedef enum { KAN_CHECK, KAN_PRINT, KAN_ERASED } KanMode;

static KanMode kan_mode(int print, int erased) {
  if (erased) return KAN_ERASED;
  if (print) return KAN_PRINT;
  return KAN_CHECK;
}

typedef struct {
  Text source;
  KanMode mode;
  int done; /* 1: out is the stdout text; 0: error is the failure */
  Text out;
  const KError *error;
} KanJob;

/* The pinfront pipeline on the big stack (as check_job): parse_bytes, then
   elab_program_in with budget 16384 and the initial globals
   (test/pinfront/program.bend line 101, test/program.bend lines 25-26),
   then checked_form (shared with the surface program), or erase_core_program
   with a second budget 16384 and erase_core_print (test/family-erasure.bend
   lines 10-19). */
static void *kan_job(void *argument) {
  KanJob *job = (KanJob *)argument;
  const SurfaceDecls *ds;
  SurfaceProgramChecked value;
  const EraseCoreRows *erased = NULL;
  if (!pinfront_parser_parse_bytes(job->source, &ds, &job->error))
    return NULL;
  if (!pinfront_program_elab_program_in(budget_create(16384),
                                        global_initial(), ds, &value,
                                        &job->error))
    return NULL;
  switch (job->mode) {
  case KAN_CHECK: break;
  case KAN_PRINT: job->out = surface_program_checked_form(value.rows); break;
  case KAN_ERASED:
    if (!erase_core_program(value.globals, budget_create(16384), value.rows,
                            &erased, &job->error))
      return NULL;
    job->out = erase_core_print(erased);
    break;
  }
  job->done = 1;
  return NULL;
}

/* The whole file with no limit: a 64 KiB arena buffer, doubled (an arena
   copy) each time a read fills it; the last read is short. */
static int read_all(FILE *in, unsigned char **out, size_t *size) {
  size_t room = (size_t)1 << 16;
  unsigned char *bytes = (unsigned char *)arena_alloc(room);
  size_t got = fread(bytes, 1, room, in);
  while (got == room) {
    unsigned char *bigger = (unsigned char *)arena_alloc(room * 2);
    memmove(bigger, bytes, got);
    bytes = bigger;
    got += fread(bytes + got, 1, room, in);
    room *= 2;
  }
  if (ferror(in)) return 0;
  *out = bytes;
  *size = got;
  return 1;
}

/* The read failure of the OCaml adapter: exit 64 (no oracle row tests it). */
static int kan_cannot_read(Answer *answer, const char *path) {
  sink_puts(&answer->err, "kanon: cannot read ");
  sink_puts(&answer->err, path);
  sink_puts(&answer->err, "\n");
  return CLI_EXIT_USAGE;
}

static int kan_file(Answer *answer, const char *path, KanMode mode) {
  unsigned char *bytes;
  size_t size;
  KanJob job;
  FILE *in = fopen(path, "rb");
  if (in == NULL) return kan_cannot_read(answer, path);
  if (!read_all(in, &bytes, &size)) {
    fclose(in);
    return kan_cannot_read(answer, path);
  }
  fclose(in);
  job.source = text_of_bytes(bytes, size);
  job.mode = mode;
  job.done = 0;
  job.out = text_end();
  job.error = NULL;
  if (!run_on_big_stack(kan_job, &job)) {
    sink_puts(&answer->err, "E-HOST: the check thread did not run to the end\n");
    return CLI_EXIT_IO;
  }
  if (!job.done) {
    sink_text(&answer->err, error_to_string(job.error));
    sink_puts(&answer->err, "\n");
    return CLI_EXIT_FAIL;
  }
  sink_text(&answer->out, job.out);
  return 0;
}

static int is_host(const char *value) {
  return same(value, "bun") || same(value, "node-worker") ||
         same(value, "native");
}

static int bad_host(const char *value) {
  return invalid_choice(check_usage, "--host", value,
                        "'bun', 'node-worker', 'native'");
}

/* The check subparser walk, left to right. extra holds the unrecognized
   words of the top level; the walk appends its own. Returns -1 when the
   walk ends, else the exit code of a help or a usage error. */
typedef struct {
  const char *file; /* NULL: no file */
  int print;
  int erased;
  int kan; /* --kan: the internal pinfront front end */
  const char **extra;
  int extras;
} CheckArguments;

static int check_walk(int count, char **words, CheckArguments *a) {
  int positional = 0;
  int at;
  for (at = 0; at < count; at++) {
    const char *word = words[at];
    if (positional || !is_option(word)) {
      if (a->file == NULL)
        a->file = word;
      else
        a->extra[a->extras++] = word;
      continue;
    }
    if (same(word, "--")) {
      positional = 1;
      continue;
    }
    if (is_help(word)) return help(stdout, check_usage, check_help_body, 0);
    if (same(word, "--print")) {
      a->print = 1;
      continue;
    }
    if (same(word, "--erased")) {
      a->erased = 1;
      continue;
    }
    if (same(word, "--kan")) {
      a->kan = 1;
      continue;
    }
    if (same(word, "--host")) {
      if (at + 1 >= count || is_option(words[at + 1]))
        return usage_error(check_usage,
                           "argument --host: expected one argument");
      at++;
      if (!is_host(words[at])) return bad_host(words[at]);
      continue;
    }
    if (starts_with(word, "--host=")) {
      if (!is_host(word + 7)) return bad_host(word + 7);
      continue;
    }
    a->extra[a->extras++] = word;
  }
  return -1;
}

static int check_command(int count, char **words, const char **extra,
                         int extras) {
  CheckArguments a;
  Answer answer;
  int code;
  a.file = NULL;
  a.print = 0;
  a.erased = 0;
  a.kan = 0;
  a.extra = extra;
  a.extras = extras;
  code = check_walk(count, words, &a);
  if (code >= 0) return code;
  if (a.file == NULL)
    return usage_error(check_usage,
                       "the following arguments are required: file");
  if (a.extras > 0) return unrecognized(a.extra, a.extras);
  answer = answer_new();
  if (a.kan)
    code = kan_file(&answer, a.file, kan_mode(a.print, a.erased));
  else
    code = check_file(&answer, pathlib_form(a.file), a.print || a.erased,
                      a.erased);
  answer_flush(&answer);
  return code;
}

/* ---------------------------------------------------------------- test */

/* `test PATH...` (PORT-PLAN decision 3, S8 session 1): each PATH is a
   directory, walked in byte order, or a source file. For a source
   NAME.sole-comb (the check path) or NAME.kan (the kan path) and each MODE
   in check, print and erased with a golden NAME.MODE.out beside it, the row
   runs in this process; its record bytes
     "exit N\n--- stdout\n" STDOUT "--- stderr\n" STDERR
   (the sidecar format of the kit oracle, PORT-PLAN section e) are compared
   with the golden bytes. One line per row: `TEST PATH MODE ok`, or
   `TEST PATH MODE FAIL PART` with PART the part of the record that holds
   the first differing byte (exit, stdout or stderr; golden = the golden
   file did not read). The last line is `TEST n=N pass=P fail=F`; exit 0
   only when F is 0. The arena is released after each row: a kan row of
   test/agreement needs up to 823 MB (S8 session 1 measure). The TEST lines
   are stdio buffered: a crash in a later row loses them. */
static const char test_usage[] =
    "usage: sole-comb test [-h] [--expect N] path [path ...]\n";

static const char test_help_body[] =
    "\n"
    "positional arguments:\n"
    "  path\n"
    "\n"
    "options:\n"
    "  -h, --help  show this help message and exit\n"
    "  --expect N  require exactly N golden comparisons\n";

typedef struct {
  KanMode mode;
  const char *word; /* the MODE of the golden name */
} TestMode;

static const TestMode test_modes[] = {
    {KAN_CHECK, "check"}, {KAN_PRINT, "print"}, {KAN_ERASED, "erased"}};

typedef enum { SOURCE_NONE, SOURCE_PUBLIC, SOURCE_KAN } SourceKind;

typedef struct {
  unsigned long runs;
  unsigned long passes;
  unsigned long fails;
} TestCount;

static int ends_with(const char *word, const char *suffix) {
  size_t length = strlen(word);
  size_t count = strlen(suffix);
  return length >= count && same(word + length - count, suffix);
}

static SourceKind source_kind(const char *path) {
  if (ends_with(path, ".kan")) return SOURCE_KAN;
  if (ends_with(path, ".sole-comb")) return SOURCE_PUBLIC;
  return SOURCE_NONE;
}

/* The decimal digits of number. */
static void sink_decimal(Sink *sink, unsigned long number) {
  unsigned char digits[24];
  size_t count = 0;
  do {
    digits[sizeof digits - 1 - count] = (unsigned char)('0' + number % 10);
    count++;
    number /= 10;
  } while (number > 0);
  sink_bytes(sink, digits + sizeof digits - count, count);
}

/* The sink bytes as a C string (one NUL appended). */
static const char *sink_cstring(Sink *sink) {
  sink_bytes(sink, (const unsigned char *)"", 1);
  return (const char *)sink->items;
}

static const char *path_join(const char *dir, const char *name) {
  Sink path = sink_new();
  size_t length = strlen(dir);
  sink_puts(&path, dir);
  if (length > 0 && dir[length - 1] != '/') sink_puts(&path, "/");
  sink_puts(&path, name);
  return sink_cstring(&path);
}

/* The first index where the two byte runs differ; the shorter size when
   one is a prefix of the other (a_size when both are the same). */
static size_t first_difference(const unsigned char *a, size_t a_size,
                               const unsigned char *b, size_t b_size) {
  size_t least = a_size < b_size ? a_size : b_size;
  size_t at;
  for (at = 0; at < least; at++)
    if (a[at] != b[at]) return at;
  return least;
}

/* The record part that holds byte at: exit_end and out_end are the record
   sizes after the exit line and after the stderr marker. */
static const char *record_part(size_t at, size_t exit_end, size_t out_end) {
  if (at < exit_end) return "exit";
  if (at < out_end) return "stdout";
  return "stderr";
}

static int run_row(Answer *answer, const char *path, SourceKind kind,
                   KanMode mode) {
  if (kind == SOURCE_KAN) return kan_file(answer, path, mode);
  return check_file(answer, pathlib_form(path), mode != KAN_CHECK,
                    mode == KAN_ERASED);
}

/* Runs the row, builds its record and compares it with the golden bytes:
   the verdict goes to line; 1 = the same. */
static int row_verdict(Sink *line, const char *path, SourceKind kind,
                       KanMode mode, const unsigned char *golden,
                       size_t golden_size) {
  Answer answer = answer_new();
  int code = run_row(&answer, path, kind, mode);
  Sink record = sink_new();
  size_t exit_end;
  size_t out_end;
  size_t at;
  sink_puts(&record, "exit ");
  sink_decimal(&record, (unsigned long)code);
  sink_puts(&record, "\n");
  exit_end = record.size;
  sink_puts(&record, "--- stdout\n");
  sink_bytes(&record, answer.out.items, answer.out.size);
  sink_puts(&record, "--- stderr\n");
  out_end = record.size;
  sink_bytes(&record, answer.err.items, answer.err.size);
  at = first_difference(record.items, record.size, golden, golden_size);
  if (record.size == golden_size && at == record.size) {
    sink_puts(line, " ok\n");
    return 1;
  }
  sink_puts(line, " FAIL ");
  sink_puts(line, record_part(at, exit_end, out_end));
  sink_puts(line, "\n");
  return 0;
}

/* One row: the golden beside path for the mode, when there is one. */
static void test_row(const char *path, SourceKind kind, TestMode mode,
                     TestCount *count) {
  ArenaMark mark = arena_mark();
  Sink golden_path = sink_new();
  FILE *in;
  unsigned char *golden = NULL;
  size_t golden_size = 0;
  int got;
  int passed;
  Sink line;
  sink_puts(&golden_path, path);
  sink_puts(&golden_path, ".");
  sink_puts(&golden_path, mode.word);
  sink_puts(&golden_path, ".out");
  in = fopen(sink_cstring(&golden_path), "rb");
  if (in == NULL && errno == ENOENT) {
    arena_release(mark);
    return;
  }
  got = in != NULL && read_all(in, &golden, &golden_size);
  if (in != NULL) fclose(in);
  count->runs++;
  line = sink_new();
  sink_puts(&line, "TEST ");
  sink_puts(&line, path);
  sink_puts(&line, " ");
  sink_puts(&line, mode.word);
  if (got) {
    passed = row_verdict(&line, path, kind, mode.mode, golden, golden_size);
  } else {
    sink_puts(&line, " FAIL golden\n");
    passed = 0;
  }
  if (passed)
    count->passes++;
  else
    count->fails++;
  fwrite(line.items, 1, line.size, stdout);
  arena_release(mark);
}

static void test_source(const char *path, TestCount *count) {
  SourceKind kind = source_kind(path);
  unsigned long before = count->runs;
  size_t at;
  if (kind == SOURCE_NONE) return;
  for (at = 0; at < sizeof test_modes / sizeof test_modes[0]; at++)
    test_row(path, kind, test_modes[at], count);
  if (count->runs == before) {
    printf("TEST %s FAIL no goldens\n", path);
    count->runs++;
    count->fails++;
  }
}

/* The entry names of a directory in byte order (readdir gives no order),
   without "." and "..". Returns 0 when path does not open as a directory. */
static int directory_names(const char *path, const char ***out_names,
                           int *out_count) {
  DIR *dir = opendir(path);
  struct dirent *entry;
  const char **names = NULL;
  int count = 0;
  int room = 0;
  if (dir == NULL) return 0;
  for (entry = readdir(dir); entry != NULL; entry = readdir(dir)) {
    const char *name = entry->d_name;
    size_t length;
    char *copy;
    int at;
    if (same(name, ".") || same(name, "..")) continue;
    if (count == room) {
      const char **bigger;
      room = room == 0 ? 64 : room * 2;
      bigger = (const char **)arena_alloc(sizeof *bigger * (size_t)room);
      if (count > 0) memmove(bigger, names, sizeof *bigger * (size_t)count);
      names = bigger;
    }
    length = strlen(name);
    copy = (char *)arena_alloc(length + 1);
    memmove(copy, name, length + 1);
    for (at = count; at > 0 && strcmp(names[at - 1], copy) > 0; at--)
      names[at] = names[at - 1];
    names[at] = copy;
    count++;
  }
  closedir(dir);
  *out_names = names;
  *out_count = count;
  return 1;
}

/* A directory walks its entries in byte order; a file runs its goldens. */
static void test_walk(const char *path, TestCount *count) {
  const char **names;
  int n;
  int at;
  if (!directory_names(path, &names, &n)) {
    test_source(path, count);
    return;
  }
  for (at = 0; at < n; at++) test_walk(path_join(path, names[at]), count);
}

/* A test path: a directory that opens, or a readable file with a source
   suffix. */
static int is_test_path(const char *path) {
  DIR *dir = opendir(path);
  FILE *in;
  if (dir != NULL) {
    closedir(dir);
    return 1;
  }
  if (source_kind(path) == SOURCE_NONE) return 0;
  in = fopen(path, "rb");
  if (in == NULL) return 0;
  fclose(in);
  return 1;
}

static int bad_test_path(const char *path) {
  fputs("E-CLI: ", stderr);
  fputs(path, stderr);
  fputs(": not a directory or a readable .sole-comb or .kan source\n", stderr);
  return CLI_EXIT_USAGE;
}

/* Parse a positive row count without overflowing the host counter. */
static int test_expected(const char *word, unsigned long *out) {
  unsigned long n = 0;
  const unsigned char *at = (const unsigned char *)word;
  if (*at == 0) return 0;
  for (; *at != 0; at++) {
    unsigned long digit;
    if (*at < '0' || *at > '9') return 0;
    digit = (unsigned long)(*at - '0');
    if (n > (ULONG_MAX - digit) / 10) return 0;
    n = n * 10 + digit;
  }
  *out = n;
  return n != 0;
}

/* The test subparser walk (as check_walk), the path test, the rows, then
   the summary line. */
static int test_command(int count, char **words, const char **extra,
                        int extras) {
  const char **paths =
      (const char **)arena_alloc(sizeof *paths * (size_t)(count + 1));
  int path_count = 0;
  int positional = 0;
  int at;
  int have_expected = 0;
  unsigned long expected = 0;
  TestCount total;
  Sink line;
  for (at = 0; at < count; at++) {
    const char *word = words[at];
    if (positional || !is_option(word)) {
      paths[path_count++] = word;
      continue;
    }
    if (same(word, "--")) {
      positional = 1;
      continue;
    }
    if (is_help(word)) return help(stdout, test_usage, test_help_body, 0);
    if (same(word, "--expect")) {
      if (++at >= count || !test_expected(words[at], &expected))
        return usage_error(test_usage, "--expect requires a positive row count");
      have_expected = 1;
      continue;
    }
    extra[extras++] = word;
  }
  if (path_count == 0)
    return usage_error(test_usage,
                       "the following arguments are required: path");
  if (extras > 0) return unrecognized(extra, extras);
  for (at = 0; at < path_count; at++)
    if (!is_test_path(paths[at])) return bad_test_path(paths[at]);
  total.runs = 0;
  total.passes = 0;
  total.fails = 0;
  for (at = 0; at < path_count; at++) {
    unsigned long before = total.runs;
    test_walk(paths[at], &total);
    if (total.runs == before) {
      printf("TEST %s FAIL no goldens\n", paths[at]);
      total.runs++;
      total.fails++;
    }
  }
  line = sink_new();
  sink_puts(&line, "TEST n=");
  sink_decimal(&line, total.runs);
  sink_puts(&line, " pass=");
  sink_decimal(&line, total.passes);
  sink_puts(&line, " fail=");
  sink_decimal(&line, total.fails);
  sink_puts(&line, "\n");
  fwrite(line.items, 1, line.size, stdout);
  if (have_expected && total.runs != expected) {
    fprintf(stderr, "E-TEST-COUNT: expected %lu rows, got %lu\n", expected,
            total.runs);
    return CLI_EXIT_FAIL;
  }
  return total.fails == 0 ? 0 : CLI_EXIT_FAIL;
}

static int not_yet(const char *verb) {
  fputs("E-SOURCE-NOT-YET: sole-comb ", stderr);
  fputs(verb, stderr);
  fputs(" needs the erasure and WebAssembly backend; use check\n", stderr);
  return CLI_EXIT_SOURCE;
}

int main(int argc, char **argv) {
  const char **extra =
      (const char **)arena_alloc(sizeof *extra * (size_t)(argc + 1));
  int extras = 0;
  int at;
  const char *command;
  for (at = 1; at < argc && is_option(argv[at]); at++) {
    if (is_help(argv[at])) return help(stdout, top_usage, top_help_body, 0);
    extra[extras++] = argv[at];
  }
  if (at == argc && extras > 0) return unrecognized(extra, extras);
  if (at == argc)
    return help(stderr, top_usage, top_help_body, CLI_EXIT_USAGE);
  command = argv[at];
  if (same(command, "check"))
    return check_command(argc - at - 1, argv + at + 1, extra, extras);
  if (same(command, "test"))
    return test_command(argc - at - 1, argv + at + 1, extra, extras);
  if (!same(command, "build") && !same(command, "run"))
    return invalid_choice(top_usage, "command", command,
                          "'check', 'build', 'run', 'test'");
  if (extras > 0) return unrecognized(extra, extras);
  return not_yet(command);
}
