ifdef D
   OPT= -g
else
   OPT= -g -flto -Ofast
endif


CC = clang
CPP = clang++
CFLAGS = $(OPT) -Wall -v -march=native -pthread
INCLUDE = -I ./include
SOURCES = src/lock.c
OBJECTS = $(subst src/,obj/,$(subst .c,.o,$(SOURCES)))
LIBS = -lssl -lcrypto -ltbb 

ifdef PMEM
INCLUDE += -I ./pmdk/src/PMDK/src/include
LIBS +=  -L ./pmdk/src/PMDK/src/nondebug -lpmem -lpmemobj
endif

all: BSkipList

obj/%.o: src/%.c
	@ mkdir -p obj
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

obj/BSkipList.o: BSkipList.cpp
	@ mkdir -p obj
	$(CPP) $(CFLAGS) $(INCLUDE) -c $< -o $@


BSkipList: $(OBJECTS) obj/BSkipList.o
	$(CPP) $(CFLAGS) $^ -o $@ $(LIBS)

.PHONY: clean directories

clean:
	rm -f BSkipList $(OBJECTS) obj/BSkipList.o
