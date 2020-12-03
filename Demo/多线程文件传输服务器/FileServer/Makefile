CC=gcc

OBJ=server
all:$(OBJ)

HERE=./
incs=-I$(HERE)  

files:=$(HERE)/main.c
files+=$(HERE)/SocketServer.c

OBJS_C := $(patsubst %.c,%.o,$(files))

CFLAGS=-O0 -g 
CFLAGS+=$(incs) 

LDFLAGS=-lpthread 
LDFLAGS+=-lm
LDFLAGS+=-ldl

$(OBJ):$(OBJS_C)
	$(CC)  $(LDFLAGS) -o $@ $^ 
			
$(OBJS_C):%.o:%.c
	$(CC) $(CFLAGS)  -c $< -o $@

clean:
	rm -rf $(HERE)*.o
	rm -rf $(HERE)/$(OBJ)
	0
	