CFLAGS=-g -Wall
all: vcpu_scheduler

run: vcpu_scheduler
	./$< 5

memcheckv: vcpu_scheduler
	valgrind --leak-check=full --show-leak-kinds=all ./$<

memcheck: vcpu_scheduler
	valgrind --leak-check=summary ./$<

vcpu_scheduler: vcpu_scheduler.c
	$(CC) $(CFLAGS) $< -o $@ -lvirt -lm

clean:
	rm -rf vcpu_scheduler
