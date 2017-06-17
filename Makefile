RPM_OPT_FLAGS ?= -O2 -g -Wall
all: zstdwriter
zstdwriter: main.o libzstdwriter.a
	$(CC) $(RPM_OPT_FLAGS) -o $@ $^ -lzstd
libzstdwriter.a: zstdwriter.o
	$(AR) r $@ $^
main.o: main.c zstdwriter.h
	$(CC) $(RPM_OPT_FLAGS) -c $<
zstdwriter.o: zstdwriter.c zstdwriter.h
	$(CC) $(RPM_OPT_FLAGS) -c $<
clean:
	rm -f zstdwriter.o libzstdwriter.a main.o zstdwriter
