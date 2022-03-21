CC=gcc
CFLAGS=-ljansson -lpthread
DEPS = network.h block.h sha-256.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

bitcoin_miner: bitcoin_miner.o block.o network.o sha-256.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm *.o bitcoin_miner


	