#include <stdio.h>

#include "op.h"
#include "value.h"
#include "chunk.h"

int mesche_disasm_simple_instr(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

int mesche_disasm_const_instr(const char *name, Chunk *chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d  '", name, constant);
  fflush(stdout);
  mesche_value_print(chunk->constants.values[constant]);
  printf("'\n");

  return offset + 2;
}

int mesche_disasm_byte_instr(const char *name, Chunk *chunk, int offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

int mesche_disasm_jump_instr(const char *name, int sign, Chunk *chunk, int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

int mesche_disasm_instr(Chunk *chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("  |   ");
  } else {
    printf("%4d  ", chunk->lines[offset]);
  }

  uint8_t instr = chunk->code[offset];
  switch (instr) {
  case OP_CONSTANT:
    return mesche_disasm_const_instr("OP_CONSTANT", chunk, offset);
  case OP_NIL:
    return mesche_disasm_simple_instr("OP_NIL", offset);
  case OP_T:
    return mesche_disasm_simple_instr("OP_T", offset);
  case OP_POP:
    return mesche_disasm_simple_instr("OP_POP", offset);
  case OP_ADD:
    return mesche_disasm_simple_instr("OP_ADD", offset);
  case OP_SUBTRACT:
    return mesche_disasm_simple_instr("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return mesche_disasm_simple_instr("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return mesche_disasm_simple_instr("OP_DIVIDE", offset);
  case OP_NEGATE:
    return mesche_disasm_simple_instr("OP_NEGATE", offset);
  case OP_AND:
    return mesche_disasm_simple_instr("OP_AND", offset);
  case OP_OR:
    return mesche_disasm_simple_instr("OP_OR", offset);
  case OP_NOT:
    return mesche_disasm_simple_instr("OP_NOT", offset);
  case OP_EQV:
    return mesche_disasm_simple_instr("OP_EQV", offset);
  case OP_EQUAL:
    return mesche_disasm_simple_instr("OP_EQUAL", offset);
  case OP_RETURN:
    return mesche_disasm_simple_instr("OP_RETURN", offset);
  case OP_DISPLAY:
    return mesche_disasm_simple_instr("OP_DISPLAY", offset);
  case OP_DEFINE_GLOBAL:
    return mesche_disasm_const_instr("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_READ_GLOBAL:
    return mesche_disasm_const_instr("OP_READ_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL:
    return mesche_disasm_const_instr("OP_SET_GLOBAL", chunk, offset);
  case OP_READ_LOCAL:
    return mesche_disasm_byte_instr("OP_READ_LOCAL", chunk, offset);
  case OP_SET_LOCAL:
    return mesche_disasm_byte_instr("OP_SET_LOCAL", chunk, offset);
  case OP_JUMP:
    return mesche_disasm_jump_instr("OP_JUMP", 1, chunk, offset);
  case OP_JUMP_IF_FALSE:
    return mesche_disasm_jump_instr("OP_JUMP_IF_FALSE", 1, chunk, offset);
  default:
    printf("Unknown opcode: %d\n", instr);
    return offset + 1;
  }
}

void mesche_disasm_chunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < chunk->count; /* Intentionally empty */) {
    offset = mesche_disasm_instr(chunk, offset);
  }
}
