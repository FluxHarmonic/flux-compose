#include <SDL.h>
#include <libguile.h>
#include <pthread.h>
#include <stdbool.h>

void* init_sdl(void* arg)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *screen = SDL_CreateWindow("Flux Preview", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);

    bool quit = false;
    SDL_Event event;

    while (!quit)
    {
        SDL_WaitEvent(&event);

        switch (event.type)
        {
            case SDL_QUIT:
                quit = true;
                break;
        }
    }

    SDL_Quit();

    return 0;
}

pthread_t graphics_thread_handle;

int init_graphics_thread() {
  // Create the thread for SDL2
  int rc = pthread_create(&graphics_thread_handle, NULL, init_sdl, NULL);
}

SCM init_graphics (SCM width, SCM height)
{
  // TODO: Pass width and height through
  init_graphics_thread();
}

static void
inner_main (void *closure, int argc, char **argv)
{
  scm_c_primitive_load("init.scm");

  scm_c_define_gsubr("init-graphics", 2, 0, 0, init_graphics);

  scm_shell(argc, argv);
}

int
main (int argc, char **argv)
{
  scm_boot_guile (argc, argv, inner_main, 0);
  return 0; /* never reached, see inner_main */
}
