CC     = gcc
CFLAGS = -Wall -Wextra -Iinclude

# 'all' is the default target — runs when you just type 'make'
all: ccsds_parser ccsds_simulator

ccsds_parser: src/main.c src/parser.c src/anomaly.c src/logger.c
	$(CC) $(CFLAGS) -o ccsds_parser $^

ccsds_simulator: src/simulator.c
	$(CC) $(CFLAGS) -o ccsds_simulator $^

clean:
	rm -f ccsds_parser ccsds_simulator telemetry.csv