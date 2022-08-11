CC:= gcc
HOSTNAME := $(shell hostname|awk '{print toupper($$0)'})
CFLAGS:= -O3 -D_POSIX_SOURCE -D_GNU_SOURCE -m64 -D$(HOSTNAME)
CFLAGSO1:= -O1 -D_POSIX_SOURCE -D_GNU_SOURCE -m64 -D$(HOSTNAME)
LIBS:= -lpthread -lrt

all: obj bin out plot transmitter transmitter-no-loads receiver setup-sem cleanup-sem

transmitter: obj/transmitter.o ../util/util.o ../util/pmon_utils.o ../util/machine_const.o ../util/skx_hash_utils.o ../util/pfn_util.o
	$(CC) -o bin/$@ $^ $(LIBS)

transmitter-no-loads: obj/transmitter-no-loads.o ../util/util.o ../util/pmon_utils.o ../util/machine_const.o ../util/skx_hash_utils.o ../util/pfn_util.o
	$(CC) -o bin/$@ $^ $(LIBS)

receiver: obj/receiver.o ../util/util.o ../util/pmon_utils.o ../util/machine_const.o ../util/skx_hash_utils.o ../util/pfn_util.o
	$(CC) -o bin/$@ $^ $(LIBS)

setup-sem: obj/setup-sem.o
	$(CC) -o bin/$@ $^ $(LIBS)

cleanup-sem: obj/cleanup-sem.o
	$(CC) -o bin/$@ $^ $(LIBS)

# pmon_utils needs to be compiled with -O1 for the get_corresponding_cha function to work
../util/pmon_utils.o: ../util/pmon_utils.c
	$(CC) -c $(CFLAGSO1) -o $@  $^

obj/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

obj:
	mkdir -p $@

bin:
	mkdir -p $@

out:
	mkdir -p $@

plot:
	mkdir -p $@

clean:
	rm -rf bin obj

.PHONY: all clean
