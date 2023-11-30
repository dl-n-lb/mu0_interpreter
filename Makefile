RINCLUDE = raylib-5.0/src/
RLIB_PATH = raylib-5.0/src/libraylib.a

main: main.c
	gcc -o main main.c -I$(RINCLUDE) $(RLIB_PATH) -Wall -Wextra -lm -lpthread
