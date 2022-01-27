# add_compile_definitions(SPNG_USE_MINIZ)

target_sources(flux PRIVATE file.c log.c mem.c scene.c script.c vector.c
  graphics.c texture.c vendor/spng/spng.c vendor/miniz/miniz.c)
