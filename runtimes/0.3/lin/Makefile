CC = clang
AS = nasm

FLAGS = -flto -I. -fno-stack-protector -fno-pie -mno-stack-arg-probe -D_GNU_SOURCE -std=gnu2y -fms-extensions -Wno-microsoft
LIBS = -lOpenCL -lpthread
LDFLAGS = -Wl,-e,entry $(LIBS)

RLS_FLAGS = -O3 -mavx2 -flto -DNDEBUG -fno-unwind-tables -fno-asynchronous-unwind-tables
DBG_FLAGS = -g -D_DEBUG

SRCS = $(shell find . -name "*.c") $(shell find ../common -name "*.c")
RLS_OBJS = $(addprefix obj/, $(subst /,-,$(SRCS:.c=.o)))
DBG_OBJS = $(addprefix obj/dbg-, $(subst /,-,$(SRCS:.c=.o)))

.PHONY: all clean

all: a.out d.out

obj/asm.o: runtime.asm
	@mkdir -p obj
	$(AS) -f elf64 $< -o $@

obj/asm-dbg.o: runtime_dbg.asm
	@mkdir -p obj
	$(AS) -f elf64 $< -o $@

obj/%.o:
	@mkdir -p obj
	$(CC) -c $(subst -,/,$(patsubst obj/%.o,%.c,$@)) -o $@ $(FLAGS) $(RLS_FLAGS)

obj/dbg-%.o:
	@mkdir -p obj
	$(CC) -c $(subst -,/,$(patsubst obj/dbg-%.o,%.c,$@)) -o $@ $(FLAGS) $(DBG_FLAGS)

a.out: $(RLS_OBJS) obj/asm.o
	$(CC) $^ -o $@ $(FLAGS) $(RLS_FLAGS) $(LDFLAGS)

d.out: $(DBG_OBJS) obj/asm-dbg.o
	$(CC) $^ -o $@ $(FLAGS) $(DBG_FLAGS) $(LDFLAGS)

clean:
	rm -rf obj a.out d.out
