CC = clang
AS = fasm

FLAGS = -I. -fno-stack-protector -DFREESTANDING -ffreestanding \
        -nostdlib -fno-pie -mno-stack-arg-probe -D_GNU_SOURCE

LIBS = -lOpenCL -lpthread

LDFLAGS = -static -Wl,-e,entry $(LIBS)

RLS_FLAGS = -O3 -mavx2 -flto -DNDEBUG -fno-unwind-tables -fno-asynchronous-unwind-tables
DBG_FLAGS = -g -D_DEBUG

SRCS = $(shell find . -name "*.c")
RLS_OBJS = $(addprefix obj/, $(subst /,_,$(SRCS:.c=.o)))
DBG_OBJS = $(addprefix obj/dbg_, $(subst /,_,$(SRCS:.c=.o)))

.PHONY: all clean

all: a.out d.out

obj/asm.o: runtime.asm
	@mkdir -p obj
	$(AS) $< $@

obj/asm_dbg.o: runtime_dbg.asm
	@mkdir -p obj
	$(AS) $< $@

obj/%.o:
	@mkdir -p obj
	$(CC) -c $(subst _,/,$(patsubst obj/%.o,%.c,$@)) -o $@ $(FLAGS) $(RLS_FLAGS)

obj/dbg_%.o:
	@mkdir -p obj
	$(CC) -c $(subst _,/,$(patsubst obj/dbg_%.o,%.c,$@)) -o $@ $(FLAGS) $(DBG_FLAGS)

a.out: $(RLS_OBJS) obj/asm.o
	$(CC) $^ -o $@ $(FLAGS) $(RLS_FLAGS) $(LDFLAGS)

d.out: $(DBG_OBJS) obj/asm_dbg.o
	$(CC) $^ -o $@ $(FLAGS) $(DBG_FLAGS) $(LDFLAGS)

clean:
	rm -rf obj a.out d.out
