# cache-info

## Description

Program to determine L1d cache characteristics: cache size, cache associativity, cache line size.

## Build

To build a program just run `make` in shell.

## Run

To run type and execute the following command:
```bash
./cache-info
```

It might be useful to run it with pinning to a single processor (e.g. using `taskset` on Linux):
```bash
taskset 1 ./cache-info
```
