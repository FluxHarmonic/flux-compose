#include <GLFW/glfw3.h>
#include <flux.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>

uint8_t flux_graphics_initialized = 0;
pthread_t flux_graphics_thread_handle;

// TODO: Don't depend on this forever!
FluxWindow current_window = NULL;

struct _FluxWindow {
  int width, height;
  GLFWwindow *glfwWindow;
};

void glfw_error_callback(int error, const char *description) {
  flux_log("GLFW error %d: %s\n", error, description);
}

void flux_graphics_window_size_callback(GLFWwindow *window, int width, int height) {
  flux_log("Window size changed: %dx%d\n", width, height);

  if (current_window) {
    current_window->width = width;
    current_window->height = height;
  }
}

FluxWindow flux_graphics_window_create(int width, int height, const char *title) {
  FluxWindow window;
  GLFWwindow *glfwWindow;

  // Make the window float
  glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  // Create the window
  glfwWindow = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!glfwWindow) {
    flux_log("Could not create GLFW window!\n");
    return NULL;
  }

  window = malloc(sizeof(struct _FluxWindow));
  window->glfwWindow = glfwWindow;
  window->width = width;
  window->height = height;
  current_window = window;

  // Respond to window size changes
  glfwSetWindowSizeCallback(glfwWindow, flux_graphics_window_size_callback);

  return window;
}

void flux_graphics_window_show(FluxWindow window) {
  if (window && window->glfwWindow) {
    glfwShowWindow(window->glfwWindow);
  }
}

void flux_graphics_window_destroy(FluxWindow window) {
  if (window != NULL) {
    glfwDestroyWindow(window->glfwWindow);
    window->glfwWindow = NULL;
    free(window);
  }
}

void flux_graphics_render_loop_key_callback(GLFWwindow *window, int key, int scancode, int action,
                                            int mods) {
}

void flux_graphics_draw_color(FluxWindow window, float r, float g, float b, float a) {
  glColor4f(r, g, b, a);
}

void flux_graphics_draw_rect(FluxWindow window, float x, float y, float width, float height) {
  glRectf(x, y, x + width, y + height);
}

void *flux_graphics_render_loop(void *arg) {
  FluxWindow window = arg;
  GLFWwindow *glfwWindow = window->glfwWindow;
  float ratio;

  // TODO: Is this the best place for this?
  // Register a key callback for input handling
  /* glfwSetKeyCallback(window, key_callback); */

  // TODO: This may need to go somewhere else
  // Make the window's OpenGL context current
  glfwMakeContextCurrent(glfwWindow);

  // Set the swap interval to prevent tearing
  glfwSwapInterval(1);

  // Initialize the framebuffer and viewport
  glfwGetFramebufferSize(glfwWindow, &window->width, &window->height);
  glViewport(0, 0, window->width, window->height);

  // Set up the orthographic projection for 2D rendering
  ratio = window->width / (float)window->height;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
  glMatrixMode(GL_MODELVIEW);

  // TODO: Should I add scene flipping back here as an event to be handled?
  /* flux_log("Received set scene event!\n"); */
  /* flip_current_scene(&current_scene); */

  // Enable blending
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  while (!glfwWindowShouldClose(glfwWindow)) {
    float x;
    float y;

    // Poll for events for this frame
    glfwPollEvents();

    // TODO: Deduplicate viewport management!

    // Initialize the framebuffer and viewport
    glfwGetFramebufferSize(glfwWindow, &window->width, &window->height);
    glViewport(0, 0, window->width, window->height);

    // Set up the orthographic projection for 2D rendering
    ratio = 16 / (float)9; // window->width / (float)window->height;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.f, (float)window->width, (float)window->height, 0.f, -1.f, 1.f);
    glMatrixMode(GL_MODELVIEW);

    // Clear the screen to red
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Clear the model view matrix
    glLoadIdentity();

    x = 100 + (sin(glfwGetTime() * 7.f) * 100.f);
    y = 100 + (cos(glfwGetTime() * 7.f) * 100.f);

    // TODO: Render some stuff
    flux_graphics_draw_color(window, 1.0, 0.0, 0.0, 1.0);
    flux_graphics_draw_rect(window, x, y, 500, 400);
    flux_graphics_draw_color(window, 0.0, 1.0, 0.0, 0.5);
    flux_graphics_draw_rect(window, 300, 300, 500, 400);

    // Swap the render buffers
    glfwSwapBuffers(glfwWindow);
  }

  flux_log("Render loop is exiting...\n");

  return NULL;
}

void flux_graphics_loop_start(FluxWindow window) {
  pthread_create(&flux_graphics_thread_handle, NULL, flux_graphics_render_loop, window);
  flux_log("Graphics event loop started...\n");
}

void flux_graphics_loop_wait(void) {
  pthread_join(flux_graphics_thread_handle, NULL);
}

int flux_graphics_init() {
  if (flux_graphics_initialized == 0) {
    flux_log("Initializing graphics thread...\n");
    flux_graphics_initialized = 1;

    // Make sure we're notified about errors
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
      flux_log("GLFW failed to init!\n");
      return 1;
    }

    // Make sure all new windows are hidden by default
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  } else {
    flux_log("Graphics subsystem already initialized...\n");
  }

  return 0;
}

void flux_graphics_end(void) {
  glfwTerminate();
}
