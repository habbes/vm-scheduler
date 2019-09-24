CFLAGS =-g -Wall

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

LDFALGS = -lvirt -lm

all: vcpu_scheduler

run: vcpu_scheduler
	./$< 5

memcheckv: vcpu_scheduler
	valgrind --leak-check=full --show-leak-kinds=all ./$<

memcheck: vcpu_scheduler
	valgrind --leak-check=summary ./$< 5

vcpu_scheduler: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFALGS)

.PHONY: clean
clean:
	rm -rf $(OBJ) vcpu_scheduler
