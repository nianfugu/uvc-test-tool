EXE=uvc-test-x86_64

COMMON_LIB=/data2/ngu/work/test
COMMON_BIN=/data2/ngu/work/test/bin

CC=gcc

SRC=$(wildcard *.c)
#OBJ=$(SRC:.c=.o)
OBJ=$(patsubst %.c,%.o,$(SRC))
DEP=$(patsubst %.c,.%.d,$(SRC))

CFLAGS=-g -O0 -I$(COMMON_LIB)/libv4l/x86_64/include
#V4L2LIBS = $(shell pkg-config --libs libv4l2)
V4L2LIBS = -L$(COMMON_LIB)/libv4l/x86_64/lib -lv4l2 \
		   -L$(COMMON_LIB)/libv4l/x86_64/lib -lv4lconvert \
			-lrt -lm -lpthread
$(EXE):$(OBJ)
	$(CC) $(CFLAGS) $^ -o $(COMMON_BIN)/$@ $(V4L2LIBS) 

$(DEP):.%.d:%.c
	@set -e; rm -f $@; \
	$(CC) -MM  $< > $@.$$$$; \
	sed 's,/($*/)/.o[ :]*,/1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEP)
clean:
	@rm $(EXE) $(OBJ) $(DEP) -f
