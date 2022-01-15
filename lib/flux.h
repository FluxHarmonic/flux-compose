#ifndef __FLUX_H
#define __FLUX_H

#include <SDL.h>
#include <stdio.h>

// Memory -----------------------------------------

extern void *flux_memory_alloc(size_t size);
extern void flux_memory_dealloc(void *mem_ptr);

// File -------------------------------------------

extern FILE *flux_file_open(char *file_name, char *mode_string);
extern FILE *flux_file_from_string(char *file_contents);

// Logging ----------------------------------------

extern void flux_log(const char *format, ...);
extern void flux_log_mem(void *ptr, const char *format, ...);
extern void flux_log_file_set(const char *file_path);

// Scene ------------------------------------------

// Member types
#define TYPE_CIRCLE 2

typedef struct ColorType {
  Uint8 r;
  Uint8 g;
  Uint8 b;
  Uint8 a;
} Color;

typedef struct CircleType {
  Sint16 x;
  Sint16 y;
  Sint16 radius;
  Color *color;
} Circle;

typedef struct SceneMemberType {
  Uint32 type;
  void *props;
} SceneMember;

typedef struct SceneType {
  Uint32 member_count;
  SceneMember *members;
} Scene;

extern Uint32 register_set_scene_event();
extern void init_staging_scene();
extern void promote_staging_scene();
extern void flip_current_scene();
extern void render_scene(SDL_Renderer *, Scene *);

// Scripting ----------------------------------------

typedef enum {
  ValueKindNone,
  ValueKindInteger,
  ValueKindFloat,
  ValueKindString,
  ValueKindEntity,
} ValueKind;

typedef struct {
  ValueKind kind;
} ValueHeader;

typedef struct {
  ValueHeader header;
  int value;
} ValueInteger;

typedef struct {
  ValueHeader header;
  float value;
} ValueFloat;

typedef struct {
  ValueHeader header;
  unsigned int length;
  char string[];
} ValueString;

typedef struct {
  ValueHeader header;
  void* entity;
} ValueEntity;

typedef struct {
  ValueHeader header;
} ValueFunctionPtr;

extern ValueHeader *flux_script_eval(FILE *script_file);
extern ValueHeader *flux_script_eval_string(char *script_string);

// Utils --------------------------------------------

#define PANIC(message, ...)                                        \
  flux_log("\n\e[1;31mPANIC\e[0m in \e[0;93m%s\e[0m: ", __func__); \
  flux_log(message __VA_OPT__(,) __VA_ARGS__);                     \
  exit(1);

#endif
