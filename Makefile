all: sailnavsim tests

tests: sailnavsim_tests


OBJS = \
	src/Boat.o \
	src/BoatInitParser.o \
	src/BoatRegistry.o \
	src/BoatWindResponse.o \
	src/Command.o \
	src/ErrLog.o \
	src/Logger.o \
	src/NetServer.o \
	src/PerfUtils.o

TESTS_OBJS = \
	tests/test_BoatRegistry.o


src/%.o: src/%.c
	gcc -c -Wall -Werror -O2 -D_GNU_SOURCE -Ilibproteus/include/ -o $@ $<

sailnavsim: $(OBJS) src/main.o
	gcc -O2 -D_GNU_SOURCE -o sailnavsim $(OBJS) src/main.o -lm -lpthread -lsqlite3 -Llibproteus -lproteus


tests/%.o: tests/%.c
	gcc -c -Wall -Werror -O2 -D_GNU_SOURCE -Ilibproteus/include/ -Isrc/ -o $@ $<

sailnavsim_tests: $(TESTS_OBJS) $(OBJS) tests/tests_main.o
	gcc -O2 -D_GNU_SOURCE -o sailnavsim_tests $(TESTS_OBJS) $(OBJS) tests/tests_main.o -lm -lpthread -lsqlite3 -Llibproteus -lproteus


clean:
	rm -rf src/*.o tests/*.o sailnavsim sailnavsim_tests
