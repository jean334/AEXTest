# This program was made to perform some testing on AEX Notify

## Compilation/Execution
- `make`
- `./app <sleep_time> <core_id> <set_affinty> <sleep_inside_enclave>`

## Parameters
- **sleep_time** : integers, for how many seconds the program should count the number of AEX
- **core_id** : integers, ranging from 0 to the number of available core
- **set_affinity** : 0 or 1, 0 to set manually - with taskset for instance - which core is associated to the program, 1 to set the affinity directly in the code
-  **sleep_inside_enclave** : 0 or 1, the program count AEX for `sleep_time` seconds, either inside the enclave (with a loop doing nothing for approximately `sleep_time_seconds`) or an ocall that wait with the sleep() function

## Remarks and observations
- the ocall to wait outside the enclave is performed switchlessly, thus, AEX should still be counted
- the set_affinity function doesn't seem to work (nothing changes if it is used or not)
- if set_affinity = 0, core_id won't be taken into account


## System specs
- SGX1 (AEX Notify is enabled, which means that the EDECCSSA User Leaf Function is available on our system)
- custom kernel based on the 6.9.3, the following paramaters were changed
    - CONFIG_HZ=100 Hz, which means that only 100 temporal interrupts (leading to scheduling, context switching, preemption, etc.) This modification seems to effectively reduce AEX, as the number of AEX was reduced by about half after we change this parameter from 256 to 100.
    - CONFIG_NO_HZ_FULL=y, Omits scheduling-clock ticks on cores that are either idle or that have only one runnable task

## Results
- **Command :** `./app 2 0 0 0`, 209 AEX counted. Coherent considering 100 scheduling-clock ticks happend every second. Considering the CountAdd_intervals array, this value increments linearly (1 every 10 ms)
- **Command :** `./app 2 0 0 1` 226 AEX counted. Coherent as waiting inside the enclave is done by performing a certain amount of add operations that approximately take `sleep_time` seconds.

- **Command :** `taskset 1 ./app 2 0 0 0`, 3 AEX counted, they didn't appear in the arrays, they may come from the registration and unregistration aex_handler
- **Command :** `taskset 1 ./app 120 0 0 0`, 3 AEX counted
- **Command :** `taskset 1 ./app 2 0 0 1`, 21000 AEX counted, when we wait inside the enclave and specify on which core we execute the program, AEX happend 100 times more often. The result didn't depends on taskset's argument
- **Command :** `taskset 3 ./app 2 0 0 1`, either 20 000 or 200 AEX counted

- **Command :** `./app 2 0 1 0`, 200 AEX counted. It seems that set_affinity didn't change anything 
- **Command :** `taskset 1 ./app 2 0 1 1`, 21000 AEX counted


## Observations
- tendency to count more AEX when the program is run for the first time or when parameters change. e.g:
    - `./app 2 0 0 0` for the first time : 800 AEX
    - `./app 2 0 0 0` next times : 200 AEX
- the 4 last AEX are much closer to each other than other ones

- Sometimes, especially when using taskset, the number of AEX varies when changing taskset's argument, or the ocall never return
- Taskset's argument is a mask : 
    - 1 --> CPU0
    - 2 --> CPU1
    - 3 (0011 in binary) --> CPU0 and CPU1
    but the program is never running or switching threads, despite multiple threads are running. This command `watch -n 1 'ps -e -o pid,psr,comm | awk '\''$2 < 4  && $1 > 9000 '\'''` shows that the app is only running on one threads 

## Conclusion
The number of AEX is completely different when using taskset. Depending on whether `sleep_time` is expected inside or outside the enclave, the number of AEX explodes or remains close to 0, with AEX occurring at the very end of the program. In any case, the number of AEX is not plausible (either too high or too low). 


## Second Test, attaching the handler to the ADD counting threads

### Results
- **Command :** `./app 2 0 0 0`, 200 AEX counted (as expected)
- **Command :** `./app 2 0 0 1`, 200 AEX counted (as expected)

- **Command :** `taskset 1 ./app 2 0 0 0` 20 000 AEX counted
- **Command :** ` taskset 1 ./app 2 0 0 1` 20 000 AEX counted

- **Command :** `taskset 3 ./app 2 0 0 0` 200 AEX counted
- **Command :** `taskset 3 ./app 2 0 0 1` 200 AEX counted

### Conclusion 
This time, by assinging 2 cores to the program (taskset 3 --> CPU0 and CPU1), the number of AEX is as expected. 