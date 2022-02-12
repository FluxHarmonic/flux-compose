#ifndef mesche_op_h
#define mesche_op_h

typedef enum {
  OP_CONSTANT,
  OP_NIL,
  OP_T,
  OP_POP,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NEGATE, // TODO: Remove in favor of SUBTRACT!
  OP_AND,
  OP_OR,
  OP_NOT,
  OP_EQV,
  OP_EQUAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_SET_LOCAL,
  OP_READ_GLOBAL,
  OP_READ_LOCAL,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_DISPLAY,
  OP_RETURN
} MescheOpCode;

#endif
