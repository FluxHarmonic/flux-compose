#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <flux-internal.h>
#include <flux.h>

// Constants
#define INITIAL_MEMBER_SIZE 100

static void scene_render_image(FluxRenderContext context, Scene *scene, SceneImage *image) {
  FluxDrawArgs draw_args;
  draw_args.flags = 0;
  draw_args.shader_program = 0;

  flux_graphics_draw_args_scale(&draw_args, image->scale, image->scale);
  flux_graphics_draw_args_center(&draw_args, image->centered);
  flux_graphics_draw_texture_ex(context, image->texture, image->position[0], image->position[1],
                                &draw_args);
}

static void scene_render_rect(FluxRenderContext context, Scene *scene, SceneRect *rect) {
  flux_graphics_draw_rect_fill(context, rect->rect[0], rect->rect[1], rect->rect[2], rect->rect[3],
                               rect->color->color);
}

static void scene_render_text(FluxRenderContext context, Scene *scene, SceneText *text) {
  flux_font_draw_text(context, text->font, text->string, text->position[0], text->position[1]);
}

void flux_scene_render(FluxRenderContext context, Scene *scene) {
  // Draw the scene
  for (int i = 0; i < scene->member_count; i++) {
    switch (scene->members[i]->kind) {
    case TYPE_IMAGE:
      scene_render_image(context, scene, (SceneImage *)scene->members[i]);
      break;
    case TYPE_RECT:
      scene_render_rect(context, scene, (SceneRect *)scene->members[i]);
      break;
    case TYPE_TEXT:
      scene_render_text(context, scene, (SceneText *)scene->members[i]);
      break;
    }
  }
}

SceneImage *flux_scene_make_image(FluxTexture *texture, double x, double y, double scale,
                                  bool centered) {
  SceneImage *image = flux_memory_alloc(sizeof(SceneImage));
  image->member.kind = TYPE_IMAGE;
  image->texture = texture;
  image->position[0] = x;
  image->position[1] = y;
  image->scale = scale;
  image->centered = centered;

  return image;
}

Value flux_scene_func_scene_image_make(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 5) {
    flux_log("Function requires 5 parameters, received %d.\n", arg_count);
  }

  ObjectPointer *texture_ptr = AS_POINTER(args[0]);
  FluxTexture *texture = texture_ptr->ptr;
  double pos_x = AS_NUMBER(args[1]);
  double pos_y = AS_NUMBER(args[2]);
  double scale = AS_NUMBER(args[3]);
  bool centered = AS_BOOL(args[4]);

  SceneImage *image = flux_scene_make_image(texture, pos_x, pos_y, scale, centered);
  return OBJECT_VAL(mesche_object_make_pointer((VM *)mem, image, true));
}

SceneRect *flux_scene_make_rect(double x, double y, double width, double height,
                                SceneColor *color) {
  SceneRect *rect = flux_memory_alloc(sizeof(SceneRect));
  rect->member.kind = TYPE_RECT;
  rect->rect[0] = x;
  rect->rect[1] = y;
  rect->rect[2] = width;
  rect->rect[3] = height;
  rect->color = color;

  return rect;
}

Value flux_scene_func_scene_rect_make(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 5) {
    flux_log("Function 'scene-rect-make' requires 5 parameters, received %d.\n", arg_count);
  }

  double pos_x = AS_NUMBER(args[0]);
  double pos_y = AS_NUMBER(args[1]);
  double width = AS_NUMBER(args[2]);
  double height = AS_NUMBER(args[3]);
  ObjectPointer *color_ptr = AS_POINTER(args[4]);

  SceneRect *rect = flux_scene_make_rect(pos_x, pos_y, width, height, (SceneColor *)color_ptr->ptr);
  return OBJECT_VAL(mesche_object_make_pointer((VM *)mem, rect, true));
}

SceneColor *flux_scene_make_color(double r, double g, double b, double a) {
  SceneColor *color = flux_memory_alloc(sizeof(SceneColor));
  color->color[0] = r / 255.f;
  color->color[1] = g / 255.f;
  color->color[2] = b / 255.f;
  color->color[3] = a;

  return color;
}

Value flux_scene_func_scene_color_make(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 4) {
    flux_log("Function 'scene-color-make' requires 4 parameters, received %d.\n", arg_count);
  }

  double r = AS_NUMBER(args[0]);
  double g = AS_NUMBER(args[1]);
  double b = AS_NUMBER(args[2]);
  double a = AS_NUMBER(args[3]);

  SceneColor *color = flux_scene_make_color(r, g, b, a);
  return OBJECT_VAL(mesche_object_make_pointer((VM *)mem, color, true));
}

SceneText *flux_scene_make_text(double x, double y, const char *string, FluxFont font,
                                SceneColor *color) {
  SceneText *text = flux_memory_alloc(sizeof(SceneText));
  text->member.kind = TYPE_TEXT;
  text->position[0] = x;
  text->position[1] = y;
  text->string = string;
  text->font = font;
  text->color = color;

  return text;
}

Value flux_scene_func_scene_text_make(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 5) {
    flux_log("Function 'scene-text-make' requires 5 parameters, received %d.\n", arg_count);
  }

  double pos_x = AS_NUMBER(args[0]);
  double pos_y = AS_NUMBER(args[1]);
  ObjectString *string = AS_STRING(args[2]);
  ObjectPointer *font_ptr = AS_POINTER(args[3]);
  ObjectPointer *color_ptr = AS_POINTER(args[4]);

  SceneText *text = flux_scene_make_text(pos_x, pos_y, string->chars, (FluxFont)font_ptr->ptr,
                                         (SceneColor *)color_ptr->ptr);
  return OBJECT_VAL(mesche_object_make_pointer((VM *)mem, text, true));
}

Scene *flux_scene_make_scene(double width, double height) {
  Scene *scene = malloc(sizeof(Scene));
  scene->width = width;
  scene->height = height;
  scene->member_count = 0;
  scene->members = malloc(sizeof(SceneMember *) * INITIAL_MEMBER_SIZE);

  return scene;
}

void flux_scene_member_add(Scene *scene, SceneMember *member) {
  scene->members[scene->member_count] = member;
  scene->member_count++;
}

Value flux_scene_func_scene_make(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 3) {
    flux_log("Function requires 3 parameters.");
  }

  double width = AS_NUMBER(args[0]);
  double height = AS_NUMBER(args[1]);
  Value *members = &args[2];

  // Create the new scene
  Scene *scene = flux_scene_make_scene(width, height);

  // Add the member pointers to the scene
  Value *current_member = members;
  while (IS_CONS(*current_member)) {
    ObjectCons *cons = AS_CONS(*current_member);
    ObjectPointer *member_ptr = AS_POINTER(cons->car);
    flux_scene_member_add(scene, (SceneMember *)member_ptr->ptr);
    current_member = &cons->cdr;
  }

  return OBJECT_VAL(mesche_object_make_pointer((VM *)mem, scene, true));
}
