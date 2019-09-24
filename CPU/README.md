# CPU Scheduler

## Code organisation

The project is organised in the following module files:

- `main.c`: entry-point of the program, connects to the hypervisor and starts the scheduler loop
- `guestlist.h`, `guestlist.c`: structures and functions to get all the active domains on the host (`GuestList` struct and `GuestList*` functions)
- `cpustats.h`, `cpustats.c`: structures and functions for collecting and updating overall and domain-specific CPU statistics as well as current vCPU->pCPU mappings(`CpuStats` struct and `CpuStats*` functions)
- `scheduler.h`, `scheduler.c`: implements the CPU scheduler policy that decide which CPU to pin each domain to at each cycle (`allocateCpus` function). This is the heart of the program.
- `check.h`: assertions and error-checking macros
- `util.h`, `util.c`: basic utility functions

## How to run

Build the project by running:

```
make
```
This will compile and build the executable binary `cpu_scheduler`

Run this executable passing the cycle interval duration in seconds as argument:
```
./cpu_scheduler <INTERVAL_DURATION>
```
Example:
```
./cpu_scheduler 12
```
Terminate the program using `Ctrl-C` keyboard command.

## CPU Scheduler Policy

The CPU scheduler aims to distribute the total usages of the domains
evenly across all the pCPUs while trying to prevent changes in pin mapping of
vCPUs as much as possible.

At each cycle, the scheduler computes usage statistics for each vCPU. Then it
iterates through the pCPUs a couple of times to find the optimal mappings of vCPUs
that will lead to an evenly distributed usage. Once this process is complete,
the vCPUs are re-pinned based on the newly-computed mappings. This completes
on cycle of the scheduler. CPU usage is computed as `(cpuTime(t) - cpuTime(t - 1))/ timeInterval`.

Now let's discuss the process in a bit more detail:

Let `totalWeight = sum of all the usages of each vCPU`, then the purpose is for each pCPU to achieve a `targetWeight = totalWeight/num of pCPUs`. That is, `targetWeight` represents the target utilization for each pCPU.

On the other hand, the `currentWeight` of a pCPU is the sum of the usages of all
the vCPUs pinned to that pCPU (even if that vCPUs were not using that pCPU in the previous period).

Once the scheduler computes `targetWeight` and `currentWeight` of each pCPU, it checks
whether the pCPUs are currently balanced. The system is considered to be in balance if
for each pCPU the `targetWeight` is close to `currentWeight`. The threshold to determine whether they are close is currently set at `0.01`, i.e. 0.79 and 0.80 are considered close.

If the pCPUs are balanced, no remapping is performed, and this completes the scheduler cycle.

If the pCPUs are not balanced, then the scheduler tries to compute new mappings of vCPUs for each pCPU assuming that each pCPU has an initial weight of 0 and aims for its `targetWeight`. For each pCPU, it finds the most ideal vCPU to pin to that pCPU. The ideal vCPU is the one whose usage that gets pCPU closest to its `targetWeight` without exceeding it. If a vCPU was already pinned to the pCPU in the previous cycle, then its prioritized over another vCPU with the same usage. This discourages unnecessary pin changes. Furthermore, a vCPU that is mapped to fewer other pCPUs is prioritized over another vCPU with the same usage. This encourages pinning a vCPU to as few pCPU as possible.

In one pass of the remapping algorithm, each pCPU will have been allocated at most one vCPU, and the N vCPUs with the leading usage will have been pinned to a pCPU, where N is the number of pCPUs. The process will be repeated until no pCPU can be allocated any more vCPUs. This occurs when the pCPU has reached its `targetWeight`, when all vCPUs have already been pinned to that pCPU, or when any remaining vCPU will cause the pCPU to exceed its `targetWeight`.

Example, assuming there are 8 vCPUs an 4 pCPUs. Half of the vCPUs have a usage of 0.75 and the other half a usage of 0.25. The `targetWeight` in this case will be `(0.75*4 + 0.25*4)/4` = `1`.
Then in the first pass of the remapping algorithm,
the 4 vCPUs with 0.75 usage will be pinned to the 4 pCPUs, one each. And the pCPUs weights will be 0.75 each. In the second pass each of the remaining 0.25 vCPUs will be mapped to the 4 pCPUs, one each. At these point the pCPU weights weill be `1` each. In the third pass, no vCPU will be mapped to pCPU because the `targetWeight` for each pCPU has been reached. Because no pCPU has been allocated an additional vCPU, the remapping algorithm terminates.

Once the new mappings have been calculated, each vCPU is repinned based on these mappings. This completes the scheduler cycle.

Here are some pros of these approach:
- Computing the mappings iteratively before repinning the vCPU allows the scheduler to achieve a balanced state in very few cycles. In the provided test cases, only one cycle is enough to achieve a balanced state most of the time.
- The algorithm leads to relatively few pin changes even when the utilizations have to rebalanced
- In situations that where the scheduler cannot balance utilization evenly, it still prevents one pCPU from getting all of the load


Here are some cons:
- The pCPUs utilization will not be balanced if vCPU usages cannot be evenly distributed across all pCPUs.
- This scheduler does not support systems with more than 8 pCPUs (this is because it only uses a single `unsiged char` for cpu maps, for the case of simplicity)