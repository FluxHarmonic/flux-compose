#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <flux-internal.h>
#include <flux.h>

// Constants
#define INITIAL_MEMBER_SIZE 100

/* Scene* current_scene = &test_scene; */
Scene *staging_scene = NULL;

// For each thing in the scene, we need to know
// - Type of thing to draw
// - Specific struct of properties for the thing to be drawn

Color *make_color_struct(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  Color *color = malloc(sizeof(Color));
  color->r = r;
  color->g = g;
  color->b = b;
  color->a = a;

  return color;
}

Circle *make_circle_struct(int16_t x, int16_t y, int16_t radius, Color *color) {
  Circle *circle = malloc(sizeof(Circle));
  circle->x = x;
  circle->y = y;
  circle->radius = radius;
  circle->color = color;

  return circle;
}

ValueHeader *flux_graphics_func_circle(VectorCursor *list_cursor, ValueCursor *value_cursor) {
  int16_t x = 0, y = 0, radius = 0;
  Color *color = NULL;

  // Read values from K/V pairs
  while (flux_vector_cursor_has_next(list_cursor)) {
    ExprHeader *expr = flux_vector_cursor_next(list_cursor);
    if (expr->kind == ExprKindKeyword) {
      // TODO: Check for none
      ExprHeader *value = flux_vector_cursor_next(list_cursor);

      // Which keyword name is it?
      char *key_name = ((ExprKeyword *)expr)->name;
      if (strcmp(key_name, "name") == 0) {
        flux_log("Found a name keyword! %s\n", ((ExprSymbol *)value)->name);
      }
    }
  }

  // TODO: Wrap this in a value type
  make_circle_struct(x, y, radius, color);

  return NULL;
}

/* void render_filled_circle(SDL_Renderer *renderer, Circle *circle) { */
/*   // Draw a circle */
/*   Color *color = circle->color; */
/*   filledCircleRGBA(renderer, circle->x, circle->y, circle->radius, color->r, color->g, color->b,
 */
/*                    color->a); */
/* } */

static void scene_render_image(FluxRenderContext context, Scene *scene, SceneImage *image) {
  FluxDrawArgs draw_args;
  draw_args.shader_program = 0;

  flux_graphics_draw_args_scale(&draw_args, 1.5f, 1.5f);
  flux_graphics_draw_args_center(&draw_args, false);
  flux_graphics_draw_texture_ex(context, image->texture, image->position[0], image->position[1],
                                &draw_args);
}

void flux_scene_render(FluxRenderContext context, Scene *scene) {
  // Draw the scene
  for (int i = 0; i < scene->member_count; i++) {
    switch (scene->members[i]->kind) {
    /* case TYPE_CIRCLE: */
    /*   render_filled_circle(renderer, scene->members[i].props); */
    /*   break; */
    case TYPE_IMAGE:
      scene_render_image(context, scene, (SceneImage *)scene->members[i]);
      break;
    }
  }
}

SceneImage *flux_scene_make_image(FluxTexture *texture, double x, double y) {
  SceneImage *image = flux_memory_alloc(sizeof(SceneImage));
  image->member.kind = TYPE_IMAGE;
  image->texture = texture;
  image->position[0] = x;
  image->position[1] = y;

  return image;
}

Value flux_scene_func_scene_image_make(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 4) {
    flux_log("Function requires 4 parameters.");
  }

  ObjectPointer *texture_ptr = AS_POINTER(args[0]);
  FluxTexture *texture = texture_ptr->ptr;
  double pos_x = AS_NUMBER(args[1]);
  double pos_y = AS_NUMBER(args[2]);

  SceneImage *image = flux_scene_make_image(texture, pos_x, pos_y);
  return OBJECT_VAL(mesche_object_make_pointer((VM *)mem, image, true));
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
