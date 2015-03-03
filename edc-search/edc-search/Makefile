VPATH = ./../src
.PATH = ${.CURDIR}/../src

CC=gcc
#CC=icc
CFLAGS = -Wall -g -O2 -fopenmp

ADJACENT_LIST        = edc-search-adjacent-list
COMMON_OBJ           = utils.o
ADJACENT_LIST_OBJ    = $(ADJACENT_LIST).o
OBJ = $(ADJACENT_LIST_OBJ) $(COMMON_OBJ)
BIN = $(ADJACENT_LIST)


all: $(BIN)

$(ADJACENT_LIST): $(ADJACENT_LIST_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $(ADJACENT_LIST) $(ADJACENT_LIST_OBJ) $(COMMON_OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -rf $(BIN) $(OBJ)

