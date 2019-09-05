CFLAGS=-g -Wall
all: vcpu_scheduler

run: vcpu_scheduler
	./$<

vcpu_scheduler: vcpu_scheduler.c
	$(CC) $(CFLAGS) $< -o $@ -lvirt

clean:
	rm -rf vcpu_scheduler
