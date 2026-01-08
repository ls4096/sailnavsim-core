# SailNavSim core simulator

A global sailing navigation simulator, using real-world geographic information and weather/ocean data (as seen here: https://8bitbyte.ca/sailnavsim/).

## Dependencies

### Libraries

- POSIX threads (pthread) library, with headers
- SQLite3 library, with headers
- [libproteus](https://github.com/ls4096/libproteus) library, with headers (referenced as git submodule)
    - Can be cloned/initialized with `git submodule update --init`

### Build tools

- Make
- C compiler (gcc or clang, etc.)
- Rust toolchain (rustc and cargo, etc.)

### Tested build/run environments

- GNU/Linux (various distros), x86-64

## How to build

`make sailnavsim`

## How to run

Create the named pipe to be able to send the simulator commands:

`mkfifo cmds`

Basic run:

`./sailnavsim`

With optional TCP server listening on localhost:$PORT (for weather data, live boat info, etc.):

`./sailnavsim --netport $PORT`

Performance test run:

`./sailnavsim --perf`

### Add a boat

`echo "TestBoat,add,44.0,-63.0,0,0" > cmds`

### Set a course and start the boat

`echo "TestBoat,course,90" > cmds`

`echo "TestBoat,start" > cmds`

### Stop and remove the boat

`echo "TestBoat,stop" > cmds`

`echo "TestBoat,remove" > cmds`

## Build and run tests

`make tests`

`./sailnavsim_tests`
