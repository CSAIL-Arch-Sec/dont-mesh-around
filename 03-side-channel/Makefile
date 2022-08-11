CC:= gcc
HOSTNAME := $(shell hostname|awk '{print toupper($$0)'})
CFLAGS:= -O3 -D_POSIX_SOURCE -D_GNU_SOURCE -m64 -D$(HOSTNAME)
CFLAGSO1:= -O1 -D_POSIX_SOURCE -D_GNU_SOURCE -m64 -D$(HOSTNAME)
LIBS:= -lpthread -lrt

all: obj bin out mesh-monitor mesh-monitor-full-key-per-iteration

mesh-monitor: obj/mesh-monitor.o ../util/util.o ../util/pmon_utils.o ../util/machine_const.o ../util/skx_hash_utils.o ../util/pfn_util.o
	$(CC) -o bin/$@ $^ $(LIBS)

mesh-monitor-full-key-per-iteration: obj/mesh-monitor-full-key-per-iteration.o ../util/util.o ../util/pmon_utils.o ../util/machine_const.o ../util/skx_hash_utils.o ../util/pfn_util.o
	$(CC) -o bin/$@ $^ $(LIBS)
	
obj/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

# pmon_utils needs to be compiled with -O1 for the get_corresponding_cha function to work
../util/pmon_utils.o: ../util/pmon_utils.c
	$(CC) -c $(CFLAGSO1) -o $@  $^

obj:
	mkdir -p $@

bin:
	mkdir -p $@

out:
	mkdir -p $@

clean:
	rm -rf bin obj
	rm -rf ../util/*.o

.PHONY: all clean
