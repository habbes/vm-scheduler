CFLAGS =-g -Wall

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
TARGET = memory_coordinator

LDFALGS = -lvirt -lm

all: $(TARGET)

run: $(TARGET)
	./$< 5

memcheckv: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$<

memcheck: $(TARGET)
	valgrind --leak-check=summary ./$< 5

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFALGS)

.PHONY: clean
clean:
	rm -rf $(OBJ) $(TARGET)
