CC=gcc
CFLAGS=`pkg-config --cflags guile-3.0` -Wall
LIBS=`pkg-config --libs guile-3.0`

OUT=bin
DIRS=$(OUT)
OBJ=$(OUT)/main.o

.PHONY: all clean

all: $(OUT)/flux-compose $(OUT)/libflux.so

$(OUT)/%.o: src/core/%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OUT)/flux-compose: $(OBJ)
	${CC} $(LIBS) -o bin/flux-compose $(OBJ)

$(OUT)/libflux.so:
	$(MAKE) -C lib

clean:
	$(MAKE) -C lib clean
	rm -rf $(OUT)/*


$(shell mkdir -p $(DIRS) > /dev/null)
