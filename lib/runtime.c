/* runtime.c: host runtime of the sole-comb C host (arena, Base Nat and U32
   helpers, Text, TextBuilder, big stack, UTF-8 output). It has no Bend
   twin: it stands in for Bend Base and the Bend runtime. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sole.h"

/* ---------------------------------------------------------------- arena */

#define ARENA_BLOCK ((size_t)64 << 20)

/* A block: this header, then size bytes (the header is 16 bytes, so the
   bytes keep the malloc alignment). The blocks form a chain in the order
   of allocation; the current block is the last one. arena_release frees
   the blocks after the mark's block (malloc gives big blocks back to the
   system, so the resident size falls to the mark: a chain that keeps the
   blocks for reuse fragments, S8 session 1 measure 1.78 GB for natAdd
   then natMul against 823 MB with free). */
struct ArenaBlock {
  ArenaBlock *next;
  size_t size;
};

static ArenaBlock *arena_first = NULL;
static ArenaBlock *arena_block = NULL; /* the last block, of arena_next */
static unsigned char *arena_next = NULL;
static size_t arena_left = 0;

void *arena_alloc(size_t size) {
  size_t rounded = (size + 15) & ~(size_t)15;
  void *cell;
  if (rounded == 0) rounded = 16;
  if (rounded > arena_left) {
    size_t block = rounded > ARENA_BLOCK ? rounded : ARENA_BLOCK;
    ArenaBlock *fresh = malloc(sizeof *fresh + block);
    if (fresh == NULL) {
      fputs("sole-comb: out of memory\n", stderr);
      exit(70);
    }
    fresh->next = NULL;
    fresh->size = block;
    if (arena_block == NULL)
      arena_first = fresh;
    else
      arena_block->next = fresh;
    arena_block = fresh;
    arena_next = (unsigned char *)(fresh + 1);
    arena_left = block;
  }
  cell = arena_next;
  arena_next += rounded;
  arena_left -= rounded;
  return cell;
}

ArenaMark arena_mark(void) {
  ArenaMark mark;
  mark.block = arena_block;
  mark.next = arena_next;
  mark.left = arena_left;
  return mark;
}

void arena_release(ArenaMark mark) {
  ArenaBlock *block = mark.block == NULL ? arena_first : mark.block->next;
  while (block != NULL) {
    ArenaBlock *following = block->next;
    free(block);
    block = following;
  }
  if (mark.block == NULL)
    arena_first = NULL;
  else
    mark.block->next = NULL;
  arena_block = mark.block;
  arena_next = mark.next;
  arena_left = mark.left;
}

/* ---------------------------------------------------------------- base */

Cmp u32_cmp(U32 a, U32 b) {
  if (a < b) return CMP_LT;
  return a == b ? CMP_EQ : CMP_GT;
}

int nat_add(Nat a, Nat b, Nat *out) {
  if (a > UINT64_MAX - b) return 0;
  *out = a + b;
  return 1;
}

Nat nat_sub(Nat a, Nat b) { return a > b ? a - b : 0; }

/* ---------------------------------------------------------------- text */

static U32 *text_cells(U32 size) {
  return arena_alloc((size_t)size * sizeof(U32));
}

Text text_end(void) {
  Text text;
  text.items = NULL;
  text.size = 0;
  return text;
}

static Text text_slice(const U32 *items, U32 size) {
  Text text;
  text.items = items;
  text.size = size;
  return text;
}

Text text_of_bytes(const unsigned char *bytes, size_t size) {
  U32 *items = text_cells((U32)size);
  size_t at;
  for (at = 0; at < size; at++) items[at] = bytes[at];
  return text_slice(items, (U32)size);
}

Text text_of_cstring(const char *ascii) {
  return text_of_bytes((const unsigned char *)ascii, strlen(ascii));
}

Bool text_is_end(Text text) { return text.size == 0 ? YES : NO; }

U32 text_head(Text text) { return text.items[0]; }

Text text_tail(Text text) { return text_slice(text.items + 1, text.size - 1); }

Text text_take(Text text, U32 count) {
  return text_slice(text.items, count < text.size ? count : text.size);
}

Text text_drop(Text text, U32 count) {
  U32 skip = count < text.size ? count : text.size;
  return text_slice(text.items + skip, text.size - skip);
}

Text text_append(Text a, Text b) {
  U32 *items = text_cells(a.size + b.size);
  U32 at;
  for (at = 0; at < a.size; at++) items[at] = a.items[at];
  for (at = 0; at < b.size; at++) items[a.size + at] = b.items[at];
  return text_slice(items, a.size + b.size);
}

Text text_reverse(Text text) {
  U32 *items = text_cells(text.size);
  U32 at;
  for (at = 0; at < text.size; at++) items[at] = text.items[text.size - 1 - at];
  return text_slice(items, text.size);
}

Bool text_same(Text a, Text b) {
  U32 at;
  if (a.size != b.size) return NO;
  for (at = 0; at < a.size; at++)
    if (a.items[at] != b.items[at]) return NO;
  return YES;
}

Text text_concat(const Texts *texts) {
  TextBuilder builder = builder_new();
  for (; texts != NULL; texts = texts->tail) builder_append(&builder, texts->head);
  return builder_text(&builder);
}

Text text_join(const Texts *texts, Text separator) {
  TextBuilder builder = builder_new();
  Bool first = YES;
  for (; texts != NULL; texts = texts->tail) {
    if (!first) builder_append(&builder, separator);
    builder_append(&builder, texts->head);
    first = NO;
  }
  return builder_text(&builder);
}

Text text_of_nat(Nat number) {
  U32 digits[24];
  U32 count = 0;
  U32 *items;
  U32 at;
  do {
    digits[count++] = (U32)(48 + number % 10);
    number /= 10;
  } while (number > 0);
  items = text_cells(count);
  for (at = 0; at < count; at++) items[at] = digits[count - 1 - at];
  return text_slice(items, count);
}

Text text_of_u32(U32 number) { return text_of_nat(number); }

/* ---------------------------------------------------------------- builder */

TextBuilder builder_new(void) {
  TextBuilder builder;
  builder.items = NULL;
  builder.size = 0;
  builder.capacity = 0;
  return builder;
}

void builder_push(TextBuilder *builder, U32 item) {
  if (builder->size == builder->capacity) {
    U32 capacity = builder->capacity < 16 ? 16 : builder->capacity * 2;
    U32 *items = text_cells(capacity);
    U32 at;
    for (at = 0; at < builder->size; at++) items[at] = builder->items[at];
    builder->items = items;
    builder->capacity = capacity;
  }
  builder->items[builder->size] = item;
  builder->size++;
}

void builder_append(TextBuilder *builder, Text text) {
  U32 at;
  for (at = 0; at < text.size; at++) builder_push(builder, text.items[at]);
}

void builder_append_cstring(TextBuilder *builder, const char *ascii) {
  const unsigned char *bytes = (const unsigned char *)ascii;
  for (; *bytes != 0; bytes++) builder_push(builder, *bytes);
}

Text builder_text(const TextBuilder *builder) {
  return text_slice(builder->items, builder->size);
}

/* ---------------------------------------------------------------- host */

int run_on_big_stack(void *(*job)(void *), void *argument) {
  pthread_attr_t attributes;
  pthread_t thread;
  if (pthread_attr_init(&attributes) != 0) return 0;
  if (pthread_attr_setstacksize(&attributes, SOLE_STACK_SIZE) != 0) return 0;
  if (pthread_create(&thread, &attributes, job, argument) != 0) return 0;
  pthread_attr_destroy(&attributes);
  return pthread_join(thread, NULL) == 0;
}

size_t utf8_encode(U32 c, unsigned char *bytes) {
  if (c < 0x80) {
    bytes[0] = (unsigned char)c;
    return 1;
  }
  if (c < 0x800) {
    bytes[0] = (unsigned char)(0xC0 | (c >> 6));
    bytes[1] = (unsigned char)(0x80 | (c & 0x3F));
    return 2;
  }
  if (c < 0x10000) {
    bytes[0] = (unsigned char)(0xE0 | (c >> 12));
    bytes[1] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
    bytes[2] = (unsigned char)(0x80 | (c & 0x3F));
    return 3;
  }
  bytes[0] = (unsigned char)(0xF0 | ((c >> 18) & 0x07));
  bytes[1] = (unsigned char)(0x80 | ((c >> 12) & 0x3F));
  bytes[2] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
  bytes[3] = (unsigned char)(0x80 | (c & 0x3F));
  return 4;
}

int write_utf8(void *stream, Text text) {
  FILE *out = stream;
  U32 at;
  for (at = 0; at < text.size; at++) {
    unsigned char bytes[4];
    size_t count = utf8_encode(text.items[at], bytes);
    if (fwrite(bytes, 1, count, out) != count) return 0;
  }
  return 1;
}
