PARSER_OBJS = buffer.o parser.o
LIBS = -lexpat
COMMANDER_DIR = ../clib/deps/commander
INCLUDES = -I $(COMMANDER_DIR) -I .
CFLAGS = -Wall -Wextra $(INCLUDES) $(MYCFLAGS)

ifdef debug
CFLAGS += -g
endif

# Change this to the path of the latest pages-meta-current.xml
# from the Wiktionary dump.
PAGES_XML = pages.xml

all-headers: $(PARSER_OBJS) all-headers.o get_header.o commander.o
	gcc $(CFLAGS) $(PARSER_OBJS) all-headers.o get_header.o commander.o \
		-o bin/all-headers $(LIBS) -lhat-trie

filter-headers: $(PARSER_OBJS) filter-headers.o get_header.o commander.o
	gcc $(CFLAGS) $(PARSER_OBJS) filter-headers.o get_header.o commander.o \
		-o bin/filter-headers $(LIBS) -lhat-trie

commander.o: $(COMMANDER_DIR)/commander.c
	gcc $(CFLAGS) -c $(COMMANDER_DIR)/commander.c -o commander.o

filter-headers.o: filter-headers/main.c
	gcc $(CFLAGS) -c filter-headers/main.c -o filter-headers.o

all-headers.o: all-headers/main.c
	gcc $(CFLAGS) -c all-headers/main.c -o all-headers.o

buffer.o: utils/buffer.c
	gcc $(CFLAGS) -c utils/buffer.c -o buffer.o

get_header.o: utils/get_header.c
	gcc $(CFLAGS) -c utils/get_header.c -o get_header.o

parser.o: utils/parser.c
	gcc $(CFLAGS) -c utils/parser.c -o parser.o