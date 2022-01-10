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

extern void open_log(const char *file_path);
extern void flux_log(const char *format, ...);

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

extern unsigned int flux_script_eval(FILE *script_file);
extern unsigned int flux_script_eval_string(char *script_string);

#endif
