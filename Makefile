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

COMPILE = $(CC) $(CFLAGS)
LUA_LIBS = -llua -ldl -lm

# Change this to the path of the latest pages-meta-current.xml
# from the Wiktionary dump.
PAGES_XML = pages.xml

.SECONDARY:
.SECONDEXPANSION:

# all-headers, filter-headers
%-headers: $(SHARED_OBJS) src/$$@.o utils/get_header.o
	$(COMPILE) $(SHARED_OBJS) src/$@.o utils/get_header.o \
		-o bin/$@ $(LIBS) -lhat-trie

find-templates: $(SHARED_OBJS) src/$$@.o
	$(COMPILE) $(SHARED_OBJS) src/$@.o \
		-o bin/$@ $(LIBS)

find-multiple-templates: $(SHARED_OBJS) src/$$@.o
	$(COMPILE) $(SHARED_OBJS) src/$@.o \
		-o bin/$@ $(LIBS) -lhat-trie

process-with-lua: utils/buffer.o utils/parser.o src/$$@.o
	$(COMPILE) utils/buffer.o utils/parser.o src/$@.o \
		-o bin/$@ $(LIBS) $(LUA_LIBS) -export-dynamic

utils/commander.o: $(COMMANDER_DIR)/commander.c
	$(COMPILE) -c $(COMMANDER_DIR)/commander.c -o $@