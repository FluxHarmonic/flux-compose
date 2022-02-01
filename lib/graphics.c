#include <GLFW/glfw3.h>
#include <flux-internal.h>
#include <flux.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

uint8_t flux_graphics_initialized = 0;
pthread_t flux_graphics_thread_handle;

struct _FluxWindow {
  int width, height;
  GLFWwindow *glfwWindow;
};

// TODO: Eventually store this in the scripting layer memory
FluxWindow preview_window;

void glfw_error_callback(int error, const char *description) {
  flux_log("GLFW error %d: %s\n", error, description);
}

void flux_graphics_window_size_callback(GLFWwindow *glfwWindow, int width, int height) {
  FluxWindow window;

  flux_log("Window size changed: %dx%d\n", width, height);

  window = glfwGetWindowUserPointer(glfwWindow);
  if (!window) {
    flux_log("Cannot get FluxWindow from GLFWwindow!\n");
    return;
  }

  window->width = width;
  window->height = height;
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

  // Set the "user pointer" of the GLFW window to our window
  glfwSetWindowUserPointer(glfwWindow, window);

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
  glBegin(GL_LINE_LOOP);
  glVertex2f(x, y);
  glVertex2f(x + width, y);
  glVertex2f(x + width, y + height);
  glVertex2f(x, y + height);
  glEnd();
}

void flux_graphics_draw_rect_fill(FluxWindow window, float x, float y, float width, float height) {
  glRectf(x, y, x + width, y + height);
}

void flux_graphics_draw_args_scale(FluxDrawArgs *args, float scale_x, float scale_y) {
  if (scale_x != 0 || scale_y != 0) {
    args->scale_x = scale_x;
    args->scale_y = scale_y;
    args->flags |= FluxDrawScaled;
  }
}

void flux_graphics_draw_args_rotate(FluxDrawArgs *args, float rotation) {
  if (rotation != 0.0) {
    args->rotation = rotation;
    args->flags |= FluxDrawRotated;
  }
}

void flux_graphics_draw_args_center(FluxDrawArgs *args, bool centered) {
  if (centered) {
    args->flags |= FluxDrawCentered;
  }
}

void flux_graphics_draw_texture_ex(FluxWindow window, FluxTexture texture, float x, float y,
                                   FluxDrawArgs *args) {
  float bx, by;

  // Save current rendering state before transforms
  glPushMatrix();

  // Calculate the base x and y positions
  if (args && (args->flags & FluxDrawCentered) == FluxDrawCentered) {
    bx = -texture->width / 2.f;
    by = -texture->height / 2.f;
  } else {
    bx = by = 0.f;
  }

  // Translate and scale as requested
  glTranslated(x, y, 0);
  if (args && (args->flags & FluxDrawScaled) == FluxDrawScaled) {
    glScaled(args->scale_x, args->scale_y, 1);
  }
  if (args && (args->flags & FluxDrawRotated) == FluxDrawRotated) {
    glRotated(args->rotation, 0.0, 0.0, 1.0);
  }

  // Bind the texture
  glBindTexture(GL_TEXTURE_2D, texture->texture_id);

  // Render a quad with texture coords
  glBegin(GL_QUADS);

  glTexCoord2d(0, 0);
  glVertex2f(bx, by);

  glTexCoord2d(1.0, 0);
  glVertex2f(bx + texture->width, by);

  glTexCoord2d(1.0, 1.0);
  glVertex2f(bx + texture->width, by + texture->height);

  glTexCoord2d(0, 1.0);
  glVertex2f(bx, by + texture->height);

  glEnd();

  // Clear the transforms
  glPopMatrix();

  // Reset the texture
  glBindTexture(GL_TEXTURE_2D, 0);
}

void flux_graphics_draw_texture(FluxWindow window, FluxTexture texture, float x, float y) {
  flux_graphics_draw_texture_ex(window, texture, x, y, NULL);
}

void flux_graphics_save_to_png(FluxWindow window, const char *output_file_path) {
  int i = 0;
  unsigned char *screen_bytes = NULL;
  unsigned char *image_bytes = NULL;
  size_t image_row_length = 4 * window->width;
  size_t image_data_size = sizeof(*image_bytes) * image_row_length * window->height;

  flux_log("Rendering window of size %u / %u to file: %s\n", window->width, window->height,
           output_file_path);

  // Allocate storage for the screen bytes
  // TODO: reduce memory allocation requirements
  screen_bytes = malloc(image_data_size);
  image_bytes = malloc(image_data_size);

  // TODO: Switch context to this window

  // Store the screen contents to a byte array
  glReadPixels(0, 0, window->width, window->height, GL_RGBA, GL_UNSIGNED_BYTE, screen_bytes);

  // Flip the rows of the byte array because OpenGL's coordinate system is flipped
  for (i = 0; i < window->height; i++) {
    memcpy(&image_bytes[image_row_length * i],
           &screen_bytes[image_row_length * (window->height - (i + 1))],
           sizeof(*image_bytes) * image_row_length);
  }

  // Save image data to a PNG file
  flux_texture_png_save(output_file_path, image_bytes, window->width, window->height);

  // Clean up the memory
  free(image_bytes);
  free(screen_bytes);
}

void *flux_graphics_render_loop(void *arg) {
  int has_saved = 0; // TODO: Remove this hack!
  float amt, scale;
  FluxWindow window = arg;
  FluxTexture logo = NULL;
  FluxDrawArgs draw_args;
  FluxSceneView scene_view;
  FluxFont jost_font = NULL;
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

  // Load a font
  jost_font = flux_font_load_file("/gnu/store/1mba63xmanh974yag9g3fh5ilnf7y4jm-font-jost-3.5/share/"
                                  "fonts/truetype/Jost-500-Medium.ttf",
                                  200);

  // TODO: Should I add scene flipping back here as an event to be handled?
  /* flux_log("Received set scene event!\n"); */
  /* flip_current_scene(&current_scene); */

  // Enable blending
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Enable textures
  glEnable(GL_TEXTURE_2D);

  // TODO: REMOVE THIS
  logo = flux_texture_png_load("assets/flux_icon.png");

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

    // Clear the screen
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Clear the model view matrix
    glLoadIdentity();

    // Set up the scene view
    scene_view.center_x = window->width / 2.f;
    scene_view.center_y = window->height / 2.f;
    scene_view.scale = 1.5f;

    // Translate the scene preview to the appropriate position, factoring in the
    // scaled size of the scene
    glPushMatrix();
    glTranslatef(scene_view.center_x - ((1280 * scene_view.scale) / 2.f),
                 scene_view.center_y - ((720 * scene_view.scale) / 2.f), 0.f);
    glScalef(scene_view.scale, scene_view.scale, 1.f);

    // Draw the preview area rect
    flux_graphics_draw_color(window, 1.f, 1.f, 0.f, 1.f);
    flux_graphics_draw_rect(window, -1.f, -1.f, 1280 + 1.f, 720 + 1.f);

    x = 100 + (sin(glfwGetTime() * 7.f) * 100.f);
    y = 100 + (cos(glfwGetTime() * 7.f) * 100.f);

    // TODO: Render some stuff
    flux_graphics_draw_color(window, 1.0, 0.0, 0.0, 1.0);
    flux_graphics_draw_rect_fill(window, x, y, 500, 400);
    flux_graphics_draw_color(window, 0.0, 1.0, 0.0, 0.5);
    flux_graphics_draw_rect_fill(window, 300, 300, 500, 400);

    // Apply transforms before rendering
    amt = sin(glfwGetTime() * 5.f);
    scale = 1.0 + amt * 0.3;
    flux_graphics_draw_args_scale(&draw_args, scale, scale);
    flux_graphics_draw_args_rotate(&draw_args, amt * 0.1 * 180);
    flux_graphics_draw_args_center(&draw_args, true);

    // Render a texture
    flux_graphics_draw_color(window, 1.0, 1.0, 1.0, 1.0);
    flux_graphics_draw_texture_ex(window, logo, 950, 350, &draw_args);

    // Draw some text if the font got loaded
    if (jost_font) {
      flux_font_draw_text(jost_font, "Flux Harmonic", 20, 20);
    }

    // Finish scene rendering
    glPopMatrix();

    // Render the screen to a file once
    // TODO: Remove this!
    if (has_saved == 0) {
      flux_graphics_save_to_png(window, "output.png");
      has_saved = 1;
    }

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
  if (flux_graphics_thread_handle) {
    pthread_join(flux_graphics_thread_handle, NULL);
  }
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

ValueHeader *flux_graphics_func_show_preview_window(VectorCursor *list_cursor,
                                                    ValueCursor *value_cursor) {
  // TODO: Extract width and height parmeters

  if (preview_window == NULL) {
    // Start the loop and wait until it finishes
    flux_graphics_init();
    preview_window = flux_graphics_window_create(1280, 720, "Flux Compose");
    flux_graphics_window_show(preview_window);
    flux_graphics_loop_start(preview_window);
  }

  // TODO: Return preview_window as a pointer
  return NULL;
}
