#ifndef mesche_op_h
#define mesche_op_h

typedef enum {
  OP_CONSTANT,
  OP_NIL,
  OP_T,
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
  OP_RETURN
} MescheOpCode;

#endif
