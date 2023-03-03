all: sailnavsim tests

tests: sailnavsim_tests


OBJS = \
	src/Boat.o \
	src/BoatInitParser.o \
	src/BoatRegistry.o \
	src/BoatWindResponse.o \
	src/CelestialSight.o \
	src/Command.o \
	src/ErrLog.o \
	src/GeoUtils.o \
	src/Logger.o \
	src/NetServer.o \
	src/Perf.o \
	src/WxUtils.o

TESTS_OBJS = \
	tests/test_BoatRegistry.o \
	tests/test_WxUtils.o

LIBPROTEUS_SO = libproteus/libproteus.so

RUSTLIB_A = rustlib/target/release/libsailnavsim_rustlib.a

SRC_INCLUDES = \
	-Ilibproteus/include \
	-Irustlib/include

SOLIB_DEPS = \
	-lm \
	-ldl \
	-lpthread \
	-lsqlite3 \
	-Llibproteus \
	-lproteus


$(LIBPROTEUS_SO):
	make -C libproteus libproteus

$(RUSTLIB_A):
	cd rustlib; \
	cargo build --release; \
	cd ..;


src/%.o: src/%.c
	$(CC) -c -Wall -Wextra -O2 -D_GNU_SOURCE $(SRC_INCLUDES) -o $@ $<

sailnavsim: $(OBJS) src/main.o $(LIBPROTEUS_SO) $(RUSTLIB_A)
	$(CC) -O2 -D_GNU_SOURCE -o sailnavsim src/main.o $(OBJS) $(RUSTLIB_A) $(SOLIB_DEPS)


tests/%.o: tests/%.c
	$(CC) -c -Wall -Wextra -O2 -D_GNU_SOURCE -Isrc $(SRC_INCLUDES) -o $@ $<

sailnavsim_tests: $(TESTS_OBJS) $(OBJS) tests/tests_main.o $(LIBPROTEUS_SO) $(RUSTLIB_A)
	$(CC) -O2 -D_GNU_SOURCE -o sailnavsim_tests tests/tests_main.o $(TESTS_OBJS) $(OBJS) $(RUSTLIB_A) $(SOLIB_DEPS)


clean:
	rm -rf src/*.o tests/*.o sailnavsim sailnavsim_tests; \
	make -C libproteus clean; \
	cd rustlib; \
	cargo clean; \
	cd ..;
