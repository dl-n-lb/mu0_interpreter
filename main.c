#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#pragma GCC diagnostic pop

typedef struct {
  const char *data;
  int len;
} sv;

typedef struct {
  char *data;
  int len;
  size_t cap;
} sb;

sb read_entire_file(const char *filepath) {
  FILE *f = fopen(filepath, "r");
  if (!f) {
    return (sb){0};
  }
  fseek(f, 0, SEEK_END);
  size_t file_sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  sb res;
  res.data = calloc(file_sz, sizeof(char));
  res.cap = file_sz;
  if (fread(res.data, file_sz, 1, f) == 0) {
    return (sb) {0};
  }
  res.len = file_sz;
  return res;
}

typedef enum {
  OP_ADD,
  OP_SUB,
  OP_LDA,
  OP_STO,
  OP_JMP,
  OP_JGE,
  OP_JNE,
  OP_STP
} op_type;

typedef struct {
  bool is_data;
  op_type op; // maybe unused
  sv label; // some instructions have a label
  sv val;
} tok_t;

void chop_whitespace(sv *s) {
  while (isspace(*s->data)) {
    s->len--;
    s->data++;
  }
}

sv chop_word(sv *s) {
  sv res = *s;
  res.len = 0;
  while (!isspace(*s->data)) {
    s->len--;
    s->data++;
    res.len++;
  }
  return res;
}

tok_t get_next_token(sv *prog) {
  chop_whitespace(prog);
  if (prog->len == 0) {
    return (tok_t) {0};
  }
  // the next token is (at least) one full word
  // since there are no commas in this dialect to worry about
  sv tok = chop_word(prog);
  // is it an operation ?
  if (tok.len == 3) {
    chop_whitespace(prog);
    // stp doesnt take any data (all other instructions do)
    if (strncmp(tok.data, "STP", 3) == 0) {
      return (tok_t) {.op = OP_STP};
    } 
    sv val = chop_word(prog);
    if        (strncmp(tok.data, "ADD", 3) == 0) {
      return (tok_t) {.op = OP_ADD, .val=val};
    } else if (strncmp(tok.data, "SUB", 3) == 0) {
      return (tok_t) {.op = OP_SUB, .val=val};
    } else if (strncmp(tok.data, "LDA", 3) == 0) {
      return (tok_t) {.op = OP_LDA, .val=val};
    } else if (strncmp(tok.data, "STO", 3) == 0) {
      return (tok_t) {.op = OP_STO, .val=val};
    } else if (strncmp(tok.data, "JMP", 3) == 0) {
      return (tok_t) {.op = OP_JMP, .val=val};
    } else if (strncmp(tok.data, "JGE", 3) == 0) {
      return (tok_t) {.op = OP_JGE, .val=val};
    } else if (strncmp(tok.data, "JNE", 3) == 0) {
      return (tok_t) {.op = OP_JNE, .val=val};
    }  else {
      printf("Invalid OPERATION\n");
      exit(1);
    }
  }
  if (tok.len == 4 && strncmp(tok.data, "DEFW", 4) == 0) {
    chop_whitespace(prog);
    sv val = chop_word(prog);
    return (tok_t) {
      .is_data = true,
      .val = val,
    };
  }
  // otherwise its a label
  tok_t res = get_next_token(prog);
  res.label = tok;
  return res;
}

void print_tok(tok_t tok) {
  if (tok.is_data) {
    printf("%.*s: Word(%.*s)\n", tok.label.len, tok.label.data, tok.val.len, tok.val.data);
    return;
  }
  switch(tok.op) {
  case OP_ADD: {
    printf("%.*s: ADD(%.*s)\n", tok.label.len, tok.label.data, tok.val.len, tok.val.data);
    break;
  }
  case OP_SUB: {
    printf("%.*s: SUB(%.*s)\n", tok.label.len, tok.label.data, tok.val.len, tok.val.data);
    break;
  }
  case OP_LDA: {
    printf("%.*s: LDA(%.*s)\n", tok.label.len, tok.label.data, tok.val.len, tok.val.data);
    break;
  }
  case OP_STO: {
    printf("%.*s: STO(%.*s)\n", tok.label.len, tok.label.data, tok.val.len, tok.val.data);
    break;
  }
  case OP_JMP: {
    printf("%.*s: JMP(%.*s)\n", tok.label.len, tok.label.data, tok.val.len, tok.val.data);
    break;
  }
  case OP_JGE: {
    printf("%.*s: JGE(%.*s)\n", tok.label.len, tok.label.data, tok.val.len, tok.val.data);
    break;
  }
  case OP_JNE: {
    printf("%.*s: JNE(%.*s)\n", tok.label.len, tok.label.data, tok.val.len, tok.val.data);
    break;
  }
  case OP_STP: {
    printf("%.*s: STP\n", tok.label.len, tok.label.data);
    break;
  }
  }
}

typedef struct {
  tok_t *toks;
  size_t cap;
  size_t len;
} tokens;

void append_token(tokens *toks, tok_t tok) {
  if (toks->len + 1 >= toks->cap) {
    toks->toks = realloc(toks->toks, (toks->cap *= 2) * sizeof(tok_t));
  }
  toks->toks[toks->len++] = tok;
}

// this should technically be 16 bits
// to be "proper" but oh well
typedef struct {
  op_type op;
  short data; // for data values this is the data, for operations its an address
} instr_t;

typedef struct {
  size_t   count;
  instr_t *instr;

  short acc;
  short pc;
} prog;


#define MIN(x, y) ((x) < (y)) ? x : y

// returns -1 for not found
int get_index_of_label(tokens toks, sv label) {
  for (size_t i = 0; i < toks.len; ++i) {
    if (toks.toks[i].label.len == label.len) {
      if (strncmp(toks.toks[i].label.data, label.data, label.len) == 0) {
	return i;
      }
    }
  }
  return -1;
}

// currently only handles decimals.. we will see
int sv_to_dec(sv s) {
  int res = 0;
  for (size_t i = 0; i < s.len; ++i) {
    res = res * 10 + s.data[i] - '0';
  }
  return res;
}

prog create_program(tokens toks) {
  prog res = {
    .count = toks.len,
    .instr = malloc(toks.len * sizeof(instr_t)),
  };
  instr_t instr;
  tok_t tok;
  for (size_t i = 0; i < toks.len; ++i) {
    instr = (instr_t){0};
    tok = toks.toks[i];
    instr.op = tok.op;
    // try label lookup
    if (get_index_of_label(toks, tok.val) > 0) {
      instr.data = get_index_of_label(toks, tok.val);
    } else {
      // otherwise store the value directly
      instr.data = sv_to_dec(tok.val);
    }
    res.instr[i] = instr;
  }
  return res;
}

void run_program(prog p) {
  instr_t ins;
  bool done = false;
  for (;!done;) {
    ins = p.instr[p.pc++];
    switch(ins.op) {
    case OP_STP: { done = true; break; }
    case OP_ADD: { p.acc += p.instr[ins.data].data; break; }
    case OP_SUB: { p.acc -= p.instr[ins.data].data; break; }
    case OP_LDA: { p.acc = p.instr[ins.data].data;  break; }
    case OP_STO: { p.instr[ins.data].data = p.acc;  break; }
    case OP_JMP: { p.pc = ins.data; break; }
    case OP_JGE: { p.pc = (p.acc > 0) ? ins.data : p.pc; break; }
    case OP_JNE: { p.pc = (p.acc != 0) ? ins.data : p.pc; break; }
    }
    printf("%d\n", p.acc);
  }
}

// returns true if execution should continue
bool step_program(prog *p) {
  instr_t ins = p->instr[p->pc++];
  switch(ins.op) {
  case OP_STP: { return false; }
  case OP_ADD: { p->acc += p->instr[ins.data].data; break; }
  case OP_SUB: { p->acc -= p->instr[ins.data].data; break; }
  case OP_LDA: { p->acc = p->instr[ins.data].data;  break; }
  case OP_STO: { p->instr[ins.data].data = p->acc;  break; }
  case OP_JMP: { p->pc = ins.data; break; }
  case OP_JGE: { p->pc = (p->acc > 0) ? ins.data : p->pc; break; }
  case OP_JNE: { p->pc = (p->acc != 0) ? ins.data : p->pc; break; }
  }
  printf("%d\n", p->acc);
  return true;
}

int main(void) {
  sb eg = read_entire_file("example.s");

  sv program = {.data = eg.data, .len = eg.len};
  tokens toks = {
    .toks = malloc(sizeof(tok_t)),
    .cap = 1,
  };
  tok_t tok;
  do {
    tok = get_next_token(&program);
    append_token(&toks, tok);
  } while(program.len > 0);

  for (size_t i = 0; i < toks.len; ++i) {
    print_tok(toks.toks[i]);
  }

  prog p = create_program(toks);

  InitWindow(800, 600, "MU0 Interpreter");
  SetTargetFPS(60);
  GuiLoadStyleDefault();

  bool show_ui = true;
  
  while(!WindowShouldClose()) {
    if(!step_program(&p)) break;
    BeginDrawing();
    {
      ClearBackground(RAYWHITE);
      if (show_ui) {
	show_ui = !GuiWindowBox((Rectangle) {10, 10, 50, 100},"A");
      }
    }
    EndDrawing();
  }
  CloseWindow();
  
  return 0;  
}
