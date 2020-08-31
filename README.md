# SailNavSim core simulator

A global sailing navigation simulator, using real-world geographic information and weather and ocean data (as seen here: https://8bitbyte.ca/sailnavsim/).

## Dependencies
- libProteus library (https://github.com/ls4096/libproteus)
- pthread library (with headers)
- SQLite3 library (with headers)

## How to build

### libproteus
`cd libproteus`

`make libproteus`

`cd ..`

### SailNavSim

`make sailnavsim`

## How to run
`mkfifo cmds`

`LD_LIBRARY_PATH=./libproteus ./sailnavsim`

### Add boat

`echo "TestBoat,add,44.0,-63.0,0,0" > cmds`

### Set course and start boat

`echo "TestBoat,course,90" > cmds`

`echo "TestBoat,start" > cmds`

### Stop and remove boat

`echo "TestBoat,stop" > cmds`

`echo "TestBoat,remove" > cmds`
