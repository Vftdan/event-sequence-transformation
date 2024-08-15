DEPS =
DEPS += libevdev
DEPS += libconfig
CFLAGS = -Wall -Wextra
ifdef DEBUG
	CFLAGS += -Og -gdwarf-2
	CPPFLAGS += -D DEBUG
else
	CFLAGS += -O2
endif
CPPFLAGS += $(shell pkg-config --cflags $(DEPS))
LDLIBS += $(shell pkg-config --libs $(DEPS))
INTERP ?=
MAIN = main
OBJS = main.o events.o processing.o graph.o config.o hash_table.o module_registry.o nodes/getchar.o nodes/print.o nodes/evdev.o nodes/tee.o

all: $(MAIN)

run: $(MAIN)
	$(INTERP) ./$(MAIN)

.PHONY: all run

$(MAIN): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@
