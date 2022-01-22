#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <SDL_image.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <flux-internal.h>
#include <flux.h>

Uint32 set_scene_event_id = -1;

Scene *current_scene = NULL;

SDL_Renderer *renderer = NULL;

Uint8 save_requested = 0;

void request_render_to_file(void) {
  save_requested = 1;
}

void render_to_file(SDL_Renderer *renderer) {
  const char *file_name = "output.png";

  // Used temporary variables
  SDL_Rect viewport;
  SDL_Surface *surface = NULL;

  // Get viewport size
  SDL_RenderGetViewport(renderer, &viewport);

  // Create SDL_Surface with depth of 32 bits
  // TODO: Alpha mask?
  surface = SDL_CreateRGBSurface(0, viewport.w, viewport.h, 32, 0, 0, 0, 0);

  // Check if the surface is created properly
  if (surface == NULL) {
    /* std::cout << "Cannot create SDL_Surface: " << SDL_GetError() << std::endl; */
    return;
  }

  // Get data from SDL_Renderer and save them into surface
  if (SDL_RenderReadPixels(renderer, NULL, surface->format->format, surface->pixels,
                           surface->pitch) != 0) {
    /* std::cout << "Cannot read data from SDL_Renderer: " << SDL_GetError() << std::endl; */

    // Don't forget to free memory
    SDL_FreeSurface(surface);
    return;
  }

  // Save screenshot as PNG file
  if (IMG_SavePNG(surface, file_name) != 0) {
    /* std::cout << "Cannot save PNG file: " << SDL_GetError() << std::endl; */

    // Free memory
    SDL_FreeSurface(surface);
    return;
  }

  // Free memory
  SDL_FreeSurface(surface);
}

void *render_loop(void *arg) {
  bool quit = false;
  SDL_Event event;

  // Create the preview window
  SDL_Window *window = SDL_CreateWindow("Flux Preview", SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_OPENGL);

  // Create the renderer for the window
  renderer = SDL_CreateRenderer(window, -1, 0);

  while (!quit) {
    SDL_WaitEvent(&event);

    /* flux_log("SDL_Event: %d [%d]\n", event.type, set_scene_event_id); */

    if (event.type == set_scene_event_id) {
      flux_log("Received set scene event!\n");
      flip_current_scene(&current_scene);
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

    // Save the scene if requested
    if (save_requested == 1) {
      save_requested = 0;
      render_to_file(renderer);
    }

    // Flip the back buffer
    SDL_RenderPresent(renderer);
  }

  SDL_Quit();

  return 0;
}

pthread_t graphics_thread_handle;
Uint8 graphics_initialized = 0;

void init_graphics(int width, int height) {
  if (graphics_initialized == 0) {
    flux_log("Initializing graphics thread...\n");

    graphics_initialized = 1;

    SDL_Init(SDL_INIT_VIDEO);

    // Register custom event for scene flipping
    set_scene_event_id = register_set_scene_event();

    // Create a blank staging scene
    init_staging_scene();

    // TODO: Pass width and height through
    // Create the thread for SDL2
    pthread_create(&graphics_thread_handle, NULL, render_loop, NULL);
  } else {
    flux_log("Graphics thread already initialized...\n");
  }
}

ValueHeader *flux_graphics_func_show_preview_window(VectorCursor *list_cursor,
                                                    ValueCursor *value_cursor) {
  flux_log("ATTEMPTING TO LOAD WINDOW\n");

  // TODO: Pass actual width and height
  init_graphics(1280, 720);

  // TODO: Is this appropriate
  return NULL;
}
