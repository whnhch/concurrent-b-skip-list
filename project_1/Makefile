ifdef D
   OPT= -g
else
   OPT= -g -flto -Ofast
endif


CC = clang
CPP = clang++
CFLAGS = $(OPT) -Wall -march=native -pthread
INCLUDE = -I ./include
SOURCES = src/lock.c
OBJECTS = $(subst src/,obj/,$(subst .c,.o,$(SOURCES)))
LIBS = -lssl -lcrypto -ltbb 

ifdef PMEM
INCLUDE += -I ./pmdk/src/PMDK/src/include
LIBS +=  -L ./pmdk/src/PMDK/src/nondebug -lpmem -lpmemobj
endif

all: benchmark

obj/%.o: src/%.c
	@ mkdir -p obj
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

obj/benchmark.o: benchmark.c
	@ mkdir -p obj
	$(CPP) $(CFLAGS) $(INCLUDE) -c $< -o $@


benchmark: $(OBJECTS) obj/benchmark.o
	$(CPP) $(CFLAGS) $^ -o $@ $(LIBS)

.PHONY: clean directories

clean:
	rm -f benchmark $(OBJECTS) obj/benchmark.o
