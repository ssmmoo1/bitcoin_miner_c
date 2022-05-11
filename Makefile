CC=gcc
CFLAGS=-ljansson -lpthread -g
DEPS = network.h block.h sha-256.h pl_control.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

bitcoin_miner: bitcoin_miner.o block.o network.o sha-256.o pl_control.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm *.o bitcoin_miner


	