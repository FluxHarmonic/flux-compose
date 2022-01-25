#include <GLFW/glfw3.h>
#include <pthread.h>
#include <flux.h>

void glfw_error_callback(int error, const char* description)
{
  flux_log("GLFW error %d: %s\n", error, description);
}

struct _FluxWindow {
  GLFWwindow* glfwWindow;
};

FluxWindow flux_graphics_window_create(int width, int height, const char *title) {
  GLFWwindow* glfwWindow = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!glfwWindow) {
    flux_log("Could not create GLFW window!\n");
    return NULL;
  }

  FluxWindow window = malloc(sizeof(struct _FluxWindow));
  window->glfwWindow = glfwWindow;

  return window;
}

void flux_graphics_window_destroy(FluxWindow window) {
  if (window != NULL) {
    glfwDestroyWindow(window);
    window->glfwWindow = NULL;
    free(window);
  }
}

Uint8 flux_graphics_initialized = 0;

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
  } else {
    flux_log("Graphics thread already initialized...\n");
  }

  return 0;
}
