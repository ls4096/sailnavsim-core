# SailNavSim core simulator

A global sailing navigation simulator, using real-world geographic information and weather/ocean data (as seen here: https://8bitbyte.ca/sailnavsim/).

## Dependencies

- POSIX threads (pthread) library, with headers
- SQLite3 library, with headers

### Tested build/run environments

- Ubuntu 18.04, x86-64
- Ubuntu 20.04, x86-64
- Debian 9 (Stretch), x86-64
- Alpine Linux 3.13, x86-64

## How to build

### libProteus

`git submodule update --init`

`cd libproteus`

`make libproteus`

`cd ..`

### SailNavSim

`make sailnavsim`

## How to run

`mkfifo cmds`

Basic run:
`LD_LIBRARY_PATH=./libproteus ./sailnavsim`

With optional TCP server listening on $PORT (for weather data, live boat info, etc.):
`LD_LIBRARY_PATH=./libproteus ./sailnavsim --netport $PORT`

Performance test run:
`LD_LIBRARY_PATH=./libproteus ./sailnavsim --perf`

### Add boat

`echo "TestBoat,add,44.0,-63.0,0,0" > cmds`

### Set course and start boat

`echo "TestBoat,course,90" > cmds`

`echo "TestBoat,start" > cmds`

### Stop and remove boat

`echo "TestBoat,stop" > cmds`

`echo "TestBoat,remove" > cmds`

## Build and run tests

`make tests`

`./run_tests.sh`
