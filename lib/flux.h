#ifndef __FLUX_H
#define __FLUX_H

#define GLFW_INCLUDE_NONE
#include <cglm/cglm.h>
#include <glad/glad.h>
#include <inttypes.h>
#include <mesche.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Memory -----------------------------------------

extern void *flux_memory_alloc(size_t size);
extern void flux_memory_dealloc(void *mem_ptr);
extern void *flux_memory_realloc(void *mem_ptr, size_t old_size, size_t new_size);

// Vector -----------------------------------------

// A function pointer which can return the size of a vector item
typedef size_t (*VectorItemSizeFunc)(void *);

typedef struct _Vector *Vector;

typedef struct {
  int32_t index;
  Vector vector;
  void *current_item;
} VectorCursor;

extern Vector flux_vector_init(Vector vector, VectorItemSizeFunc item_size_func);
extern Vector flux_vector_create(size_t initial_size, VectorItemSizeFunc item_size_func);
extern void *flux_vector_cursor_next(VectorCursor *cursor);
extern char flux_vector_cursor_has_next(VectorCursor *cursor);
extern void *flux_vector_push(VectorCursor *cursor, void *item);
extern void flux_vector_reset(Vector vector);
extern void flux_vector_cursor_init(Vector vector, VectorCursor *cursor);

// File -------------------------------------------

extern FILE *flux_file_open(const char *file_name, const char *mode_string);
extern FILE *flux_file_from_string(const char *file_contents);
extern char *flux_file_read_all(const char *file_path);

// Logging ----------------------------------------

extern void flux_log(const char *format, ...);
extern void flux_log_mem(void *ptr, const char *format, ...);
extern void flux_log_file_set(const char *file_path);

// Texture ---------------------------------------

typedef struct _FluxTexture *FluxTexture;

extern FluxTexture flux_texture_png_load(char *file_path);
extern void flux_texture_png_save(const char *file_path, const unsigned char *image_data,
                                  const uint32_t width, const uint32_t height);

Value flux_texture_func_image_load_internal(MescheMemory *mem, int arg_count, Value *args);

// Graphics ---------------------------------------

typedef struct _FluxWindow *FluxWindow;
typedef struct _FluxRenderContext *FluxRenderContext;

typedef enum {
  FluxDrawNone,
  FluxDrawScaled = 1,
  FluxDrawRotated = 2,
  FluxDrawCentered = 4
} FluxDrawFlags;

typedef struct FluxDrawArgs {
  float scale_x, scale_y;
  float rotation;
  uint8_t flags;
  GLuint shader_program;
} FluxDrawArgs;

typedef struct {
  GLenum shader_type;
  const char *shader_text;
} FluxShaderFile;

extern int flux_graphics_init(void);
extern void flux_graphics_end(void);

extern FluxWindow flux_graphics_window_create(int width, int height, const char *title);
extern void flux_graphics_window_show(FluxWindow window);
extern void flux_graphics_window_destroy(FluxWindow window);

extern void flux_graphics_loop_start(FluxWindow window);
extern void flux_graphics_loop_wait(void);

extern void flux_graphics_draw_args_scale(FluxDrawArgs *args, float scale_x, float scale_y);
extern void flux_graphics_draw_args_rotate(FluxDrawArgs *args, float rotation);
extern void flux_graphics_draw_args_center(FluxDrawArgs *args, bool centered);

extern void flux_graphics_draw_texture(FluxRenderContext context, FluxTexture texture, float x,
                                       float y);
extern void flux_graphics_draw_texture_ex(FluxRenderContext context, FluxTexture texture, float x,
                                          float y, FluxDrawArgs *args);

extern GLuint flux_graphics_shader_compile(const FluxShaderFile *shader_files,
                                           uint32_t shader_count);

Value flux_graphics_func_show_preview_window(MescheMemory *mem, int arg_count, Value *args);
Value flux_graphics_func_render_to_file(MescheMemory *mem, int arg_count, Value *args);
Value flux_graphics_func_flux_harmonic_thumbnail(MescheMemory *mem, int arg_count, Value *args);
Value flux_graphics_func_graphics_scene_set(MescheMemory *mem, int arg_count, Value *args);

// Shaders ---------------------------------------

#define GLSL(src) "#version 330 core\n" #src

// Fonts -----------------------------------------

typedef struct _FluxFont *FluxFont;

extern FluxFont flux_font_load_file(const char *font_path, int font_size);
extern void flux_font_draw_text(FluxRenderContext context, FluxFont font, const char *text,
                                float pos_x, float pos_y);

// The returned string must be freed!
extern char *flux_font_resolve_path(const char *font_name);

Value flux_graphics_func_load_font_internal(MescheMemory *mem, int arg_count, Value *args);

// Scene ------------------------------------------

// Member types
typedef enum { TYPE_CIRCLE, TYPE_IMAGE } SceneMemberKind;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} Color;

typedef struct {
  int16_t x;
  int16_t y;
  int16_t radius;
  Color *color;
} Circle;

typedef struct {
  SceneMemberKind kind;
} SceneMember;

typedef struct {
  SceneMember member;
  FluxTexture texture;
  vec2 position;
} SceneImage;

typedef struct {
  double width;
  double height;
  uint32_t member_count;
  SceneMember **members;
} Scene;

SceneImage *flux_scene_make_image(FluxTexture *texture, double x, double y);
Scene *flux_scene_make_scene(double width, double height);
void flux_scene_render(FluxRenderContext context, Scene *scene);

// Mesche API wrappers
Value flux_scene_func_scene_image_make(MescheMemory *mem, int arg_count, Value *args);
Value flux_scene_func_scene_make(MescheMemory *mem, int arg_count, Value *args);

uint32_t register_set_scene_event(void);
void init_staging_scene(void);
void promote_staging_scene(void);
void flip_current_scene(Scene **);

// Scripting ----------------------------------------

typedef enum {
  ValueKindNone,
  ValueKindInteger,
  ValueKindFloat,
  ValueKindString,
  ValueKindEntity
} ValueKindOld;

typedef struct {
  ValueKindOld kind;
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
  void *entity;
} ValueEntity;

typedef struct {
  ValueHeader header;
} ValueFunctionPtr;

extern ValueHeader *flux_script_eval(FILE *script_file);
extern ValueHeader *flux_script_eval_string(char *script_string);

// Utils --------------------------------------------

#define PANIC(message, ...)                                                                        \
  flux_log("\n\e[1;31mPANIC\e[0m in \e[0;93m%s\e[0m: ", __func__);                                 \
  flux_log(message, ##__VA_ARGS__);                                                                \
  exit(1);

#endif
