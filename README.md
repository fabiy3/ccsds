# CCSDS Parser & Simulator

A compact CCSDS packet parser and UDP simulator written in C.

This repository contains a simple telemetry receiver (`ccsds_parser`) that listens on UDP port `7000`, parses CCSDS primary headers, detects sequence gaps, logs packet metadata to `telemetry.csv`, and dispatches packets to subsystem handlers. A companion sender (`ccsds_simulator`) generates valid CCSDS telemetry packets and sends them to the parser for testing.

## Features

- CCSDS primary header parsing with proper big-endian field extraction
- Telemetry packet validation (version checks, packet length)
- Sequence gap detection per APID with wrap-around support
- Dispatch table for subsystem-specific packet handling
- CSV logging of packet metadata and anomalies
- UDP-based simulator for generating telemetry traffic

## Repository structure

- `Makefile` - build targets for `ccsds_parser` and `ccsds_simulator`
- `include/ccsds.h` - shared CCSDS type definitions, constants, and function declarations
- `src/main.c` - UDP receiver and main loop for the parser application
- `src/parser.c` - CCSDS packet parsing and dispatch initialization
- `src/anomaly.c` - sequence tracking and anomaly detection logic
- `src/logger.c` - CSV logging and terminal output formatting
- `src/simulator.c` - UDP packet generator for testing the parser
- `telemetry.csv` - generated log file output from parser runs

## Build

From the repository root, run:

```sh
make
```

This builds two executables:

- `ccsds_parser`
- `ccsds_simulator`

## Run

1. Start the parser in one terminal:

```sh
./ccsds_parser
```

2. Start the simulator in another terminal:

```sh
./ccsds_simulator
```

The parser listens on UDP port `7000` and prints packet status to the console. The simulator sends telemetry packets to `127.0.0.1:7000` at a fixed rate and injects deliberate sequence gaps for anomaly detection.

## Logging

The parser writes packet logs to `telemetry.csv` with the following columns:

- `timestamp_ms`
- `apid`
- `seq_count`
- `payload_len`
- `anomaly`

If the log file cannot be opened, the parser still runs and prints warnings to `stderr`.

## CCSDS behavior

The parser supports:

- 6-byte CCSDS primary header
- version field validation (must be `0`)
- APID values up to `0x7FF`
- packet sequence flags and counters
- payload extraction after the primary header

## Subsystems

The following APIDs are handled explicitly in the parser:

- `0x001` — attitude control
- `0x002` — power management
- `0x003` — thermal

Unknown APIDs are handled by a default fallback routine.

## Notes

- `ccsds_parser` is designed as a teaching/demo implementation and is not a full flight software stack.
- The simulator uses `usleep()` to control packet rate and intentionally skips a sequence number every 15 packets for one subsystem.

## Clean

To remove executables and generated log output:

```sh
make clean
```
