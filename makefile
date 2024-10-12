CC=gcc
CFLAGS=-Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2 -g
LDFLAGS_LIB=-shared -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup -Wl,--wrap=strndup
LDFLAGS_EXE=
RM=rm -f

SOURCES_LIB=nand.c memory_tests.c
OBJECTS_LIB=$(SOURCES_LIB:.c=.o)
SOURCES_EXE=nand_example.c
OBJECTS_EXE=$(SOURCES_EXE:.c=.o)
TARGET_LIB=libnand.so
TARGET_EXE=main_program

.PHONY: all clean

all: $(TARGET_LIB) $(TARGET_EXE)

$(TARGET_LIB): $(OBJECTS_LIB)
	$(CC) $(LDFLAGS_LIB) -o $@ $^

$(TARGET_EXE): $(OBJECTS_EXE) $(OBJECTS_LIB)
	$(CC) $(LDFLAGS_EXE) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS_LIB) $(OBJECTS_EXE) $(TARGET_LIB) $(TARGET_EXE)