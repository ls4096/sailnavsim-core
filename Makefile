all: sailnavsim

OBJS = \
	src/Boat.o \
	src/BoatInitParser.o \
	src/BoatRegistry.o \
	src/BoatWindResponse.o \
	src/Command.o \
	src/ErrLog.o \
	src/Logger.o \
	src/main.o

src/%.o: src/%.c
	gcc -c -Wall -Werror -O2 -Ilibproteus/include/ -o $@ $<

sailnavsim: $(OBJS)
	gcc -O2 -o sailnavsim src/*.o -lm -lpthread -lsqlite3 -Llibproteus -lproteus

clean:
	rm -rf src/*.o sailnavsim
