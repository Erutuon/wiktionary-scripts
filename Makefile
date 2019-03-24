PARSER_OBJS = utils/buffer.o utils/parser.o
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

all-headers: $(PARSER_OBJS) src/all-headers.o utils/get_header.o utils/commander.o
	gcc $(CFLAGS) $(PARSER_OBJS) src/all-headers.o utils/get_header.o utils/commander.o \
		-o bin/all-headers $(LIBS) -lhat-trie

filter-headers: $(PARSER_OBJS) src/filter-headers.o utils/get_header.o utils/commander.o
	gcc $(CFLAGS) $(PARSER_OBJS) src/filter-headers.o utils/get_header.o utils/commander.o \
		-o bin/filter-headers $(LIBS) -lhat-trie

find-templates: $(PARSER_OBJS) src/find-templates.o utils/commander.o
	gcc $(CFLAGS) $(PARSER_OBJS) src/find-templates.o utils/commander.o \
		-o bin/find-templates $(LIBS)

utils/commander.o: $(COMMANDER_DIR)/commander.c
	gcc $(CFLAGS) -c $(COMMANDER_DIR)/commander.c -o utils/commander.o

src/%.o: src/%.c
	gcc $(CFLAGS) -c $< -o $@

utils/%.o: utils/%.c
	gcc $(CFLAGS) -c $< -o $@