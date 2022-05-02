CC=gcc
CFLAGS=-ljansson -lpthread -march=armv8-a+crypto
DEPS = network.h block.h sha256-arm.h pl_control.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

bitcoin_miner: bitcoin_miner.o block.o network.o sha256-arm.o pl_control.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm *.o bitcoin_miner


	