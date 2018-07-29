# set up complier
CC = clang
WARNINGS = -Wall -Wextra -Werror -Wno-error=unused-parameter -Wmissing-declarations -Wmissing-variable-declarations
CFLAGS_RELEASE = -O2 $(WARNINGS) -std=c99 -c -MMD -MP -D_GNU_SOURCE -pthread

# set up linker
LD = clang
LDFLAGS = -pthread -lncurses

all: client server

client: client.o
	$(LD) $^ $(LDFLAGS) -o $@

server: server.o
	$(LD) $^ $(LDFLAGS) -o $@

client.o: client.c
	$(CC) $(CFLAGS_RELEASE) $^ -o $@

server.o: server.c
	$(CC) $(CFLAGS_RELEASE) $^ -o $@
