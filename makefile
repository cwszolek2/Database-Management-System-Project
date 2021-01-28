CC=gcc
CFLAGS=-I.
DEPS = dberror.h storage_mgr.h test_helper.h buffer_mgr.h buffer_mgr_stat.h dt.h
OBJ = dberror.o storage_mgr.o test_assign2_1.o buffer_mgr.o buffer_mgr_stat.o
OBJ2 = dberror.o storage_mgr.o test_assign2_2.o buffer_mgr.o buffer_mgr_stat.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

test_buffer1: $(OBJ) 
	cc -g -o $@ $^ $(CFLAGS)

test_buffer2: $(OBJ2)
	cc -o $@ $^ $(CFLAGS)

clean: 
	rm -f $(OBJ)
