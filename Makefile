# Compiler settings
CC = cc
CFLAGS = -g -Wall -IBackend -IFrontend -I. -I../ $(shell sdl2-config --cflags) -I/usr/local/include
CFLAGS += -DMAIN_DRIVER
LDFLAGS = -lm $(shell sdl2-config --libs) -L/usr/local/lib -lSDL2 -lSDL2_ttf -ljson-c

# Source files
SRC = $(wildcard Backend/*.c Frontend/*.c)
OBJ = $(SRC:.c=.o)

# Output executable
TARGET = Ray_anim

# Default rule: Compile everything
all: $(TARGET)

# Compile the final executable from object files
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile each `.o` file only if the corresponding `.c` file changed
Backend/%.o: Backend/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

Frontend/%.o: Frontend/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Cleanup rule
clean:
	rm -f $(TARGET) $(OBJ)

