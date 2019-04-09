SHARED_OBJS = utils/buffer.o utils/parser.o utils/commander.o
LIBS = -lexpat
COMMANDER_DIR = ../clib/deps/commander
INCLUDES = -I $(COMMANDER_DIR) -I .
CFLAGS = -Wall -Wextra $(INCLUDES) $(MYCFLAGS)

ifdef debug
CFLAGS += -g -fsanitize=address -fno-omit-frame-pointer
else
CFLAGS += -O3
endif

# Change this to the path of the latest pages-meta-current.xml
# from the Wiktionary dump.
PAGES_XML = pages.xml

.SECONDARY:
.SECONDEXPANSION:

# all-headers, filter-headers
%-headers: $(SHARED_OBJS) src/$$@.o utils/get_header.o
	$(CC) $(CFLAGS) $(SHARED_OBJS) src/$@.o utils/get_header.o \
		-o bin/$@ $(LIBS) -lhat-trie

find-templates: $(SHARED_OBJS) src/$$@.o
	$(CC) $(CFLAGS) $(SHARED_OBJS) src/$@.o \
		-o bin/$@ $(LIBS)

find-multiple-templates: $(SHARED_OBJS) src/$$@.o
	$(CC) $(CFLAGS) $(SHARED_OBJS) src/$@.o \
		-o bin/$@ $(LIBS) -lhat-trie

utils/commander.o: $(COMMANDER_DIR)/commander.c
	$(CC) $(CFLAGS) -c $(COMMANDER_DIR)/commander.c -o $@