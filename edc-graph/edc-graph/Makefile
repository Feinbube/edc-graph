VPATH = ./../src
.PATH = ${.CURDIR}/../src

CC=icc
CFLAGS = -O3 -Wall

MLC_NAND_2012       = edc-graph-cai-haratsch-mutlu-mai-2012
MLC_NAND_2012_OBJ   = $(MLC_NAND_2012).o
OBJ = $(MLC_NAND_2012_OBJ)
BIN = $(MLC_NAND_2012)


all: $(BIN)

$(MLC_NAND_2012): $(MLC_NAND_2012_OBJ)
	$(CC) $(CFLAGS) -o $(MLC_NAND_2012) -lm $(MLC_NAND_2012_OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -rf $(BIN) $(OBJ)

