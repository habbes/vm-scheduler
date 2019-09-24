# Memory Coordinator

## Code organisation

The project is organised in the following module files:
- `main.c`: main entrypoint of the application, connects to the hypervisor and starts the coordination while-loop
- `guestlist.h`, `guestlist.c`: structures and functions to get all the active domains on the host
- `memstats.h`, `memstats.c`: structures and functions for collecting and storing memory statistics for the host and each domain
- `allocplan.h`, `allocplan.c`: structures and functions used to keep track of how much memory is going to be allocated/deallocated from each domain in the current cycle
- `coordinator.h`, `coordinator.c`: functions that implement the memory coordination policy. This is the "engine" of the program.
- `check.h`: macro for assertions and error checking
- `util.h`, `util.c`: basic utility functions and macros

## How to run

Build the program using the command:

```
make
```

This will compile the program and generate a `memory_coordinator` binary in this directory.
You can the execute the binary, passing the cycle interval in seconds as an argument:

```
./memory_coordinator <INTERVAL_DURATION>
```
example: 
```
./memory_coordinator 12
```

Terminate the program using `Ctrl+C` keyboard command.

To observe the test case behaviours properly, it's advisable to use a host
with > 6GB memory, this is to ensure that the host has sufficient free memory
when the guests are consuming more and more memory. If the host does not
have sufficient free memory to begin with, it might run low on free memory
even before the memory coordinator starts to execute its policy. This is
especially the case for test cases 2 and 3.

## Memory allocation policy

Let's first by defining some terms that will be used in the policy description:

- MB: 1024 KB
- KB: 1024 bytes
- Starving domain: domain with <= 100 MB unused memory
- Active starving domain: starving domain that is actively consuming memory 
(i.e the amount of unused memory changes between consecutive two cycles)
- Wasteful domain: domain with >= 300MB unused memory
- Stable domain: domain that is not actively consuming memory
(i.e. the amount of unused memory is constant between 2 cycles),
and that is neither starving nor wasteful (i.e. unused memory between 100-300MB)

At a high level, in each cycle the policy aims to keep the memory utilization fair by allocating memory to domains
which are starving and release memory from domains which are wasteful. Before setting new memory sizes, the coordinator will iterate through each domains a couple of times to figure out how much
memory to allocate/de-allocate from each. It keeps tracks of these amounts in an allocation plan
structure (`AllocPlan`).

Each active starving domain will receive 3 times the the memory it used up since the last cycle, or 3 times the memory it needs to reach the 100mb threshold, whichever of the two is greater. This is to ensure the domain has enough memory available for continue consuming
while the coordinator is asleep. A starving domain that is not actively consuming memory will only
receive the memory it needs to reach the 100MB threshold.

Each wasteful domain will lose half of the memory it has above the 300MB threshold, up to 100MB (this cap ensures memory is de-allocated gradually even if a huge amount of memory is released at once).

If the memory freed from wasteful domains is not enough to cover the memory needed by starving domains,
then the difference memory is evenly freed from stable domains. If there are 3 stable domains
and 90MB is still needed for the starving domains, then 30MB will be de-allocated from each of the stable domains. However, the amount to de-allocate is limited to keep the domain from going below the 100MB threshold. For example, if the stable domain had 120MB unused memory, it would lose 20MB
instead of 30MB.

If memory freed wasteful domains, and stable domains is not enough to match the memory allocated to starving domains,
then the difference is allocated from the host memory.

Once this allocation plan has been computed based on the policy described above, the coordinator checks whether
the memory needed to fulfill the plan will lead to the host having low free memory (free host memory < 200MB). If
this is the case, then the allocation plan is re-adjusted, reducing allocations for each domain such that the
free host memory remains above 200MB.

Finally, the allocation plan is executed (virDomainSetMemory is called for each domain based on the values in
the allocation plan). This complete one cycle of the memory coordinator.

Other minor optimizations are done to ensure that:
- small noisy changes in memory (< 1MB change)
- virDomainSetMemory if new value is very similar to the previous
- memory de-allocations of less than 1MB are rounded up to 1MB

Here are some pros of this approach:
- Computing an allocation plan before the actual allocation/deallocation allows the coordinator
to fair distribution of the memory in relatively few cycles, usually only 1 cycle is enough
- Memory allocated to domains actively consuming memory adapts to the domain's usage rates
as well as the memory coordinator's cycle interval duration
- Memory is de-allocated gradually even when a domain frees a huge chunk of memory in one go

Cons:
- When free host memory is low due to external factors, domains may start giving up memory back
to the host even when there are starving domains which could use some memory
- In some cases some domains may stay below 100MB for a while because it maybe impossible
to strictly adhere to constraints of all domains as well as those of the host
- In the case of test case 2, if 3 VMs happen to release their memory one cycle before the 4th, then they will start losing memory which gets allocated to the 4th VM. This makes it hard to observe the expected behaviour of 4 vms gradually losing memory at the same time.
