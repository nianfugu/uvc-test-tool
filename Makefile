EXE=uvc-test-arm

COMMON_LIB=/home/gunianfu/work/test
COMMON_BIN=/home/gunianfu/work/test/bin

#CC=/toolchain/arm-none-linux-gnueabi-4.8.3/bin/arm-none-linux-gnueabi-gcc
#CC=~/bin/gcc-linaro-aarch64-linux-gnu-4.9-2014.07_linux
#CC=/home/gunianfu/bin/gcc-linaro-aarch64-linux-gnu-4.9-2014.07_linux/bin/aarch64-linux-gnu-gcc
CC=/home/gunianfu/bin/arm-2014.05/bin/arm-none-linux-gnueabi-gcc
#CC=/home/gunianfu/work/pro/cross_compile/cross_compile_app/bin/aarch64-linux-gnu-gcc

SRC=$(wildcard *.c)
#OBJ=$(SRC:.c=.o)
OBJ=$(patsubst %.c,%.o,$(SRC))
DEP=$(patsubst %.c,.%.d,$(SRC))

CFLAGS=-g  -static -O0 -I$(COMMON_LIB)/libv4l/arm_static/include
#V4L2LIBS = $(shell pkg-config --libs libv4l2)
V4L2LIBS = -L$(COMMON_LIB)/libv4l/arm_static/lib -lv4l2 \
		   -L$(COMMON_LIB)/libv4l/arm_static/lib -lv4lconvert \
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
