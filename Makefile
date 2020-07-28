all: sailnavsim

OBJS = \
	Boat.o \
	BoatInitParser.o \
	BoatRegistry.o \
	BoatWindResponse.o \
	Command.o \
	ErrLog.o \
	Logger.o \
	main.o

%.o: %.c
	gcc -c -Wall -Werror -O2 -Ilibproteus/include/ -o $@ $<

sailnavsim: $(OBJS)
	gcc -O2 -o sailnavsim *.o -lm -lpthread -lsqlite3 -Llibproteus -lproteus

clean:
	rm -rf *.o sailnavsim
