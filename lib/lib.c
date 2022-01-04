#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>

#define INITIAL_MEMBER_SIZE 100

#define TYPE_CIRCLE 2

Uint32 scene_event_id = -1;

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
  Color* color;
} Circle;

typedef struct SceneMemberType {
  Uint32 type;
  void* props;
} SceneMember;

typedef struct SceneType {
  Uint32 member_count;
  SceneMember* members;
} Scene;

SceneMember test_member = {
  .type = TYPE_CIRCLE,
  .props = &(Circle) {
    .x = 500,
    .y = 500,
    .radius = 400,
    .color = &(Color) {
      .r = 255,
      .g = 0,
      .b = 0,
      .a = 255
    }
  }
};

Scene test_scene = (Scene) {
  .member_count = 1,
  .members = &test_member
};

/* Scene* current_scene = &test_scene; */
Scene* staging_scene = NULL;
Scene* current_scene = NULL;

// For each thing in the scene, we need to know
// - Type of thing to draw
// - Specific struct of properties for the thing to be drawn

Color* make_color_struct(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  Color* color = malloc(sizeof(Color));
  color->r = r;
  color->g = g;
  color->b = b;
  color->a = a;

  return color;
}

Circle* make_circle_struct(Sint16 x, Sint16 y, Sint16 radius, Color* color) {
  Circle* circle = malloc(sizeof(Circle));
  circle->x = x;
  circle->y = y;
  circle->radius = radius;
  circle->color = color;

  return circle;
}

void add_scene_member(Uint32 type, void* props) {
  // Add the member in the member array
  // TODO: Check if we've reached the size limit!
  SceneMember* new_member = &staging_scene->members[staging_scene->member_count++];
  new_member->type = type;
  new_member->props = props;
}

void init_scene() {
  Scene *new_scene = malloc(sizeof(Scene));
  new_scene->member_count = 0;
  new_scene->members = malloc(sizeof(SceneMember) * INITIAL_MEMBER_SIZE);

  // TODO: Synchronize this!
  staging_scene = new_scene;
}

void promote_staging_scene() {
  SDL_Event event;
  SDL_memset(&event, 0, sizeof(event));
  event.type = scene_event_id;
  SDL_PushEvent(&event);

  // TODO: THIS IS BAD!
  current_scene = staging_scene;
  init_scene();
}

void render_filled_circle(SDL_Renderer* renderer, Circle* circle) {
  // Draw a circle
  Color* color = circle->color;
  filledCircleRGBA(renderer, circle->x, circle->y, circle->radius, color->r, color->g, color->b, color->a);
}

void render_scene(SDL_Renderer* renderer, Scene* scene) {
  // Draw the scene
  for (int i = 0; i < scene->member_count; i++) {
    switch(scene->members[i].type) {
      case TYPE_CIRCLE:
        render_filled_circle(renderer, scene->members[i].props);
        break;
    }
  }
}

void* init_sdl(void* arg)
{
    SDL_Init(SDL_INIT_VIDEO);

    // Register custom event for scene flipping
    scene_event_id = SDL_RegisterEvents(1);

    // Create the preview window
    SDL_Window *window = SDL_CreateWindow("Flux Preview", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_OPENGL);

    // Create the renderer for the window
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

    bool quit = false;
    SDL_Event event;

    while (!quit) {
        SDL_WaitEvent(&event);

        if (event.type == scene_event_id) {
          // TODO: Free the old current_scene!
          current_scene = staging_scene;
          init_scene();
        } else {
          switch (event.type) {
            case SDL_QUIT:
              // TODO: Find a better way to tell the Scheme side to exit gracefully
              exit(0);
              quit = true;
              break;
          }
        }

        // Set the fill color to black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        // Clear the screen before rendering
        SDL_RenderClear(renderer);

        // Render the scene
        if (current_scene != NULL) {
          render_scene(renderer, current_scene);
        }

        // Flip the back buffer
        SDL_RenderPresent(renderer);
    }

    SDL_Quit();

    return 0;
}

pthread_t graphics_thread_handle;

int init_graphics_thread() {
  // Create the thread for SDL2
  int rc = pthread_create(&graphics_thread_handle, NULL, init_sdl, NULL);
}

Uint8 graphics_initialized = 0;

void init_graphics (int width, int height)
{
  if (graphics_initialized == 0) {
    graphics_initialized = 1;

    // Create a blank scene
    init_scene();

    // TODO: Pass width and height through
    init_graphics_thread();
  }
}