# Use GCC, if you have it installed.
CC=gcc

# Tell the C compiler where to find <libguile.h>
CFLAGS=`pkg-config --cflags guile-3.0`

# Tell the linker what libraries to use and where to find them.
LIBS=`pkg-config --libs guile-3.0`

all: bin/flux-compose bin/libflux.so

bin/flux-compose:
	${CC} src/core/main.c ${CFLAGS} ${LIBS} -o bin/flux-compose

bin/libflux.so: src/core/lib.c
	${CC} src/core/lib.c `pkg-config --libs --cflags sdl2 SDL2_gfx` -lpthread -shared -fPIC -o bin/libflux.so
