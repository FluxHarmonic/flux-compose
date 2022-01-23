#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <stdio.h>

#include <flux-internal.h>
#include <flux.h>

// Constants
#define INITIAL_MEMBER_SIZE 100

/* Scene* current_scene = &test_scene; */
Scene *staging_scene = NULL;

// For each thing in the scene, we need to know
// - Type of thing to draw
// - Specific struct of properties for the thing to be drawn

Color *make_color_struct(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  Color *color = malloc(sizeof(Color));
  color->r = r;
  color->g = g;
  color->b = b;
  color->a = a;

  return color;
}

Circle *make_circle_struct(Sint16 x, Sint16 y, Sint16 radius, Color *color) {
  Circle *circle = malloc(sizeof(Circle));
  circle->x = x;
  circle->y = y;
  circle->radius = radius;
  circle->color = color;

  return circle;
}

ValueHeader *flux_graphics_func_circle(VectorCursor *list_cursor, ValueCursor *value_cursor) {
  Sint16 x = 0, y = 0, radius = 0;
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

Uint32 scene_event_id = -1;
Uint32 register_set_scene_event(void) {
  scene_event_id = SDL_RegisterEvents(1);
  return scene_event_id;
}

void add_scene_member(Uint32 type, void *props) {
  // Add the member in the member array
  // TODO: Check if we've reached the size limit!
  if (staging_scene != NULL) {
    SceneMember *new_member = &staging_scene->members[staging_scene->member_count++];
    new_member->type = type;
    new_member->props = props;

    flux_log("Added scene member: %d\n", type);
  } else {
    // TODO PANIC
    flux_log("Attempting to add member to uninitialized scene!\n");
  }
}

void init_staging_scene(void) {
  Scene *new_scene = malloc(sizeof(Scene));
  new_scene->member_count = 0;
  new_scene->members = malloc(sizeof(SceneMember) * INITIAL_MEMBER_SIZE);
  staging_scene = new_scene;

  flux_log("New scene initialized!\n");
}

void promote_staging_scene(void) {
  SDL_Event event;
  SDL_memset(&event, 0, sizeof(event));
  event.type = scene_event_id;
  SDL_PushEvent(&event);

  flux_log("Promote staging scene!\n");
}

void flip_current_scene(Scene **current_scene_ptr) {
  flux_log("Flipping the staging scene!\n");
  *current_scene_ptr = staging_scene;
  init_staging_scene();
}

void render_filled_circle(SDL_Renderer *renderer, Circle *circle) {
  // Draw a circle
  Color *color = circle->color;
  filledCircleRGBA(renderer, circle->x, circle->y, circle->radius, color->r, color->g, color->b,
                   color->a);
}

void render_scene(SDL_Renderer *renderer, Scene *scene) {
  // Draw the scene
  for (int i = 0; i < scene->member_count; i++) {
    switch (scene->members[i].type) {
    case TYPE_CIRCLE:
      render_filled_circle(renderer, scene->members[i].props);
      break;
    }
  }
}
