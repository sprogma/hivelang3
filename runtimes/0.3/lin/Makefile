CC = clang
AS = nasm

CFLAGS = -fPIC -I../common -I. -fno-stack-protector -mno-stack-arg-probe -D_GNU_SOURCE -std=gnu2y -fms-extensions -Wno-microsoft -Wno-format
LIBS = -lOpenCL -lpthread
LDFLAGS = -fPIC $(LIBS)

ifeq ($(SANITIZE),1)
    # CFLAGS += -fsanitize=address -g
    # LDFLAGS += -fsanitize=address -g
    CFLAGS += -fsanitize=leak -g
    LDFLAGS += -fsanitize=leak -g
endif

RLS_FLAGS = -O3 -mavx2 -flto -DNDEBUG -fno-unwind-tables -fno-asynchronous-unwind-tables
DBG_FLAGS = -g -D_DEBUG

SRCS = $(shell find . -name "*.c") $(shell find ../common -name "*.c")
RLS_OBJS = $(addprefix obj/, $(subst /,-,$(SRCS:.c=.o)))
DBG_OBJS = $(addprefix obj/dbg-, $(subst /,-,$(SRCS:.c=.o)))

.PHONY: all clean

all: a.out d.out

obj/asm.o: runtime.asm
	@mkdir -p obj
	$(AS) -g -f elf64 $< -o $@

obj/asm-dbg.o: runtime_dbg.asm runtime.asm
	@mkdir -p obj
	$(AS) -g -f elf64 $< -o $@

obj/%.o:
	@mkdir -p obj
	$(CC) -c $(subst -,/,$(patsubst obj/%.o,%.c,$@)) -o $@ $(CFLAGS) $(RLS_FLAGS)

obj/dbg-%.o:
	@mkdir -p obj
	$(CC) -c $(subst -,/,$(patsubst obj/dbg-%.o,%.c,$@)) -o $@ $(CFLAGS) $(DBG_FLAGS)

a.out: $(RLS_OBJS) obj/asm.o
	$(CC) $^ -o $@ $(CFLAGS) $(RLS_FLAGS) $(LDFLAGS)

d.out: $(DBG_OBJS) obj/asm-dbg.o
	$(CC) $^ -o $@ $(CFLAGS) $(DBG_FLAGS) $(LDFLAGS) $(DBG_FLAGS)

clean:
	rm -rf obj a.out d.out
