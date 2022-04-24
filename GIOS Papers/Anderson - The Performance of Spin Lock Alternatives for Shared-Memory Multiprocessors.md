Anderson - The Performance of Spin Lock Alternatives for Shared-Memory Multiprocessors

# The Performance of Spin Lock Alternatives for Shared-Memory Multiprocessors

## Abstract

- This paper examines the following: Are there efficient algorithms for software spin-waiting given hardware support for atomic instructions, or are more complex kinds of hardware support needed for performance?

## Introduction
 - Mutex locks are supported in hardware. They are used when shared memory needs to be accessed.
 - When a lock is busy, the process can block, relinquish the processor, or spin ("busy-wait") until the lock is released.
	 - Spin locks are useful in 2 situations
		 - The critical section is small and the wait time is less than the cost of blocking and resuming the processs
		 - No other work is available
 - Simple approaches to spin-waiting have poor peformance due to consuming communication bandwidth
 - Some approaches to dealing with this problem include statically assigned slots, static delays, and dynamic backoff.
	 - This paper discusses all three in the context of spin-waiting
 - This paper proposes a novel method:
	 - Each processor arriving at a lock is given a unique sequence identifier in the order they can execute the critical section
	 - When the lock is released, control is passed directly to the next processor with no further synchronization necessary
 - The paper also examines several hardward methods

## Range of Multiprocessor Architectures Considered
 - Six types of architectures considered
	 - multistage interconnection network without coherent private caches
	 - multistage interconnection network with invalidation-based cache coherence using remote directories
	 - bus without coherent private caches
	 - bus with snoopy write-through invalidation-based cache coherence
	 - bus with snoopy write-back invalidation-based cache coherence
	 - bus with snoopy distributed-write cache coherence

### Common Hardware Support for Mutual Exclusions
 - Most architectures provide instructions to atomically read, modify, and write memory
 - They require four services:
	 - read
	 - write
	 - arbitration between simultaneous requests
	 - a state that prevents further access during execution
 - Most multiprocessors collapes these services into one or two bus or network transactions
 - Single bus multiprocessors use the bus to arbitrate between simultaneous atomic instructions by raising a line (*the atomic bus line*)
	 - This prevents further atomic requests from being started
 - In systems that don't cache shared data, the acquiring of the atomic bus line can overlap with the reques to read data
 - With invalidation-based coherence, even if a lock value is cached, acquiring the atomic bus line and be overlapped with the signal to invalidate other cache copies
 - Write-back invalidation-based coherence avoids the extra bus transaction to write the data.
 - With distributed-write write-back coherence the initial read is usually not needed as the copies an all caches are updated instead of invalidated when a processor changes a memory value
 - A bus cycle is still usually needed at the end of the atomic instruction to update copies in other caches

## The Performance of Simple Approaches to Spin-Waiting
 - Developing a spin-lock is straightforward
 - However, an efficient spin-lock is difficult to devise
	 - Have to balance performance when there is contention for the lock.
	 - Need to minimize communication used by spinning processors
	 - Minimize the delay between releasing and acquiring a lock
	 - There is a tradeoff:
		 - more frequent attempts to acquire a lock means it will be acquired faster, but other processors will be disrupted
 - When there is little contention (for example in parallel programs) it is better to redesign the program to avoid contention than to have to deal with it at the processor level
 - Spin lock performance is important when:
	 - there is a heavily utilized lock
	 - locks are attempted one right after the other (a queue will form)
	 - the user makes a lot of OS calls
		 - a high load reveals problems not seen under a low load
 - One user could ruin performance for all users by combining a bottleneck in a critical section, lots of processors, and an inefficient spin lock.
 
 ### Spin on Test and Set
 - Simplest algorithm
	 - Each processor repeatedly attempt test and set until it succeeds
	 - Of course, performance degrades as the number of spinning processors increases
		 - Partly due to the bus being saturated with memory checks

### Spin on Read (Test and Test and Set)
 - In theory, coherent caches should reduce the cost of spin-waiting
	 - First test, then attempt to test and set
	 - However, once the test and set goes through, all caches are invalidated and need updating
		 - So, the benefit may not be worth the gain as the other processors have to resume looping their caches
	 - Also, when the lock is released, all the processors waiting will simultaneously attempt a test and set
		 - These requests must compete for the bus or memory
		 - This will cause several test and sets, each of which results in cache invalidation thus slowing it all down

### Reasons for the Poor Performance of Spin on Read
 - The separate between detecting a released lock and the attempt to acquire it allows more than one processor to notice and attempt the lock more or less simultaneously. Of course, [THERE CAN BE ONLY ONE](https://youtu.be/ooN9xdAgi5w?t=28).
 - Cache copies are invalidated by the test and set instruction even if the value is not changed
 - Invalidation-based cache-coherence requires O(P) network cycles to broadcase a value to P waiting processors even though each request the same data
 - Solving any of these problems would help performance, but also require linearly increasing bus activity
 - Some solutions involve using a separte bus just for test and set variables; however, the problem here is that all processors receive the updated lock value at the same time which results in them all trying their next test and set operation.

### Measurement Results
 - Using a Sequent Symmetry Modle B with 20 80386 processors
	 - Acquiring and releasing a lock takes 5.6 microseconds.
 - See Fig. 1
	 - As the number of processors increases, there is an increase in performance until the critical section becomes the bottleneck
		 - Compared to the ideal, spin on read is faster than spin test and set
			 - However, it is far from ideal

## New Software Alternatives
 - Five software-based approaches

### Delay Alternatives
 - Delays can be inserted defined by two dimensions:
	 - Where the delay is inserted
		 - Can be inserted after the lock is released or after every separate access to the lock
	 - Whether the size of the delay is static or dynamic

#### Delay after Spinning Processors Notices Lock has been Released
 - Once a processor realizes a lock has been released there is delay before it tries a test and set
	 - If another processor gets the lock first, it can resume spinning
	 - If not, it can try the test and set
		 - This reduces the number of unsuccessful test and sets which reduces cache invalidations
	 - To do this, each processor is given a static slot (or wait time)
		 - Statically assigning times ensures that at most one processor times out at any instant
	 - This algorithm works well when there are many spinning processors as one will likely have a short delay; however, with only one processor spinning, the delay could be long and hurt performance
	 - The number of slots can be varied to tradeoff performance on these two cases
	 - Varying the slots gives good overall performance in both cases
	 - Backoff scheme for spinlocks
		 - This idea comes from Ethernet's exponential backoff 
			 - Each time a processor looks for a lock and doesn't get it, it doubles its wait time.
			 - However it is different for processors compared to network stacks
		 - Factors affecting performance
			 - Do not increase or decrease mean delay
			 - Need a maximum bound on mean delay
			 - The initial delay should be a fraction of the time at the last lock
			 - For this paper they set the initial delay to 50% the previous delay
		 - This performs well to static slots
	 - Both static and backoff slots have performance problems under burst conditions
		 - The first lock attempt all processors choose a delay and time out together
			 - Eventually the mean delays are increased enough to avoid congestion
			 - That said, there will be processors with long delays making it take longer to pass control of the lock
	 - Polling for lock release is only practical in systems with per-processor coherent caches
	 - Each spinning processor still requires two cache read misses per execution of the critical section
		 - Once when lock is released
		 - Once when lock is acquired
		 - If there are enought spinning processors this can saturate the bus
	 - Exponential backoff does provide scalable performance for multiprocessors with distributed-write cache coherence
		 - Each test and set requires a single bus cycle to broadcast the new value

#### Delay Between Each Memory Reference
 - This limits the communication bandwidth as test and set consumes more bandwidth than a simple read
 - The mean delay can be set statically or dynamically
	 - This is similar to what we saw above with the Ethernet protocol
	 - More frequent polling improves performance when there are few spinning processors but worsens it when there are many
	 - Exponential backoff can be used to dynamically adapt to conditions
 - However, performance of backoff is bad when there is s single spinning processor for a moderate-sized critical section
	 - When the lock is released the spinning processor will be in the middle of a long delay

### Queueing in Shared Memory
 - The idea is to use a shared counter to keep track of the number of spinning processors
	 - The cost would be two extra atomic instructions per critical section
	 - This would consume as much bandwidth as polling the lock directly on systems without distributed-write coherence
 - Another approach is to maintain an explicit queue of spinning processors
	 - The problem is that maintaining queues is expensive 
		 - The enqueue and dequeue operations must be locked
	 - This results in much worse performance for small critical sections
 - This paper developed the following method:
	 - Each processor does an atomic read and increment to optain a unique sequence number
	 - When a processor finishes a lock, it taps the processor with the next highest number
		 - That processor now owns the lock
	 - Since everything is sequenced, there is no need to do an atomic read-modify-write operation to pass control of the lock
	 - Depending on the architecture, the mechanism is slightly differnt
	 - This approach is not as useful in systems with a bus but no cache coherence as the bus can get swamped with polling
	 - In architectures without an atomic read-and-increment instruction, this operation can itself be locked
		 - Of course, when requests arrive in a burst there will be contention for the lock in which case one of the delay alternatives mentioned above can be used.
	 - This method is fast as the processor starts executing quickly after the previous is finished
	 - The negative aspect of queueing is lock latency
		 - When there is no contention, backoff or simple spin-waiting is better, but when there is contention, queueing is better
 - Processor preemption can yield bad spin-locking performance
	 - If a processor is preempted then all the waiting processors in the queue have to keep waiting unless there is a way to let the processor know ahead of time it is being preempted and can pass on control to the next processor

### Measurement Results
 - They tested the five software alternatives with 20 processors
	 - Static and Dynamic delays varied from 0 to 15 microseconds
	 - See Fig 3 for details
		 - As the number of processors increases, the overhead rises much slower than with a basic spin-on-read.
			 - The Queue method shows the least overhead when the number of processors is 7 or more as it parallelizes the lock handoff
		 - As the lock approaches saturation, static delay provides worse performance
	 - See Fig 4 for details
		 - As the max number of spinning processors approaches 64, thre is worse low-load performance but in a high load scenario this is not necessarily the case
		 - Backoff performs well in both situations
	 - Might want to reread this section - It was pretty dense.

## Hardware Solutions
 - There are always tradeoffs
	 - What might be best for spin locks may not be best for normal memory references
		 - Some systems use two different buses to get around this
			 - However, this is wasted on applications that do not spend a lot of time spin-waiting

### Multistage Interconnection Network Multiprocessors
 - Combining networks by providing parallel access to a single memory location
	 - Requests to the same location are combined and one one response is needed
		 - This has good performance for any number of spinning processors
		 - When there is little or no contention, there is little combining
		 - As more processors spin-wait, combining reduces congestion
		 - However, combining increases latency 
 - Hardware queuing at the memory module can eliminate polling across the network and speed passing control of the lock
	 - Processor would need an explicit enter and exit critical section instruction for the memory module
		 - When a processor's enter request returns, it has the lock so no polling is necessary
		 - By specially handling critical section requests, one network round trip to pass the lock control is avoided
		 - Lock latency would be lower with hardware queuing despite the increased hardware complexity
	 - Goodman et al. suggested using caches to hold queue links
		 - When a lock is released, the next processor can get notified without going through the original memory module

### Single Bus Multiprocessors
 - One solution is to invalidate only when the lock value changes
	 - But this only reduces the time to quiesce - it does not eliminate it
	 - And performance degrades as more processors spin
 - Intelligent snooping of bus activity can reduce the cost of spin-waiting
	 - Ex. If hardware keeps caches coherent, processors can spin on a cache copy instead of repeatedly going from memory.
	 - Two Methods Proposed
		 - Read broadcast can eliminate duplicate read miss requests
			 - Each processor monitors read requests and if the read corresponds to an invalid block in its cache, it reads the result as well and updates the cache
			 - This eliminates the cascade of read misses when spinning on a read
		 - Specially handle test and set requests in the cache and bus controller
			 - This reduces the amount of bus usage when the lock is busy and doing it in hardware eliminates some of the overhead of computing random delays and the complexity of queueing
			 - When a processor issues a test and set request, it first checks the cache and if it is not there, a read miss occurs. Once the value is in the cache, the test and set can return immediately if the lock is busy
			 - If the lock is free the controller can then try to acquire the bus to get the mutex needed for the atomic instruction
			 - While waiting, the controller must monitor to see if some other processor acquires the lock and all pending test and set request can be aborted and if the lock value is invalidated, the processor converts the test and request to a read request to see if the lock is now busy

## Conclusions
 - Simple methods of spin-waiting degrade overall performance as the number of processors increases
 - Software queueing and a variant of Ethernet backoff have good performance even for large numbers of spinning processors
	 - Backoff has better performance when there is no contention for the lock
	 - Queueing performs best when there are waiting processors (due to parallelization of the lock handoff)
 - Performance can be increased by specially handling spin lock requests in hardware
	 - Either at the memory module
	 - Or at the cache level
	 - Can also use intelligent snooping to avoid bus traffic
 - Open Question
	 - Do real workloads have enough spin-waiting to make the additional hardware support worthwhile?

# About these Notes

**Notes by [Christian Thompson](https://christianthompson.com/)**
My apologies for any errors or missing information.

Notes created using [Joplin](https://joplinapp.org/), a free and open source Evernote alternative.
	 

