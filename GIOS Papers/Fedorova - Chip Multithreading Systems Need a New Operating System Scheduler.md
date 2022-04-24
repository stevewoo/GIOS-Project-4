Fedorova - Chip Multithreading Systems Need a New Operating System Scheduler

# Chip Multithreading Systems Need a New Operating System Scheduler

## Abstract

- CMT (Chip Multithreading Systems) will require a new scheduler. This studies show that a 2x increase in performance is possible.

## Introduction
 - Depending on the workload, processor pipeline utilization can be as low as 19%
 - Scheduling is very important
	 - Tailored schedulers can have a 17% performance increase while naive schedulers can actually hurt performance
 - Ideal Scheduler
	 - Assign threads to processors to minmize resource contention and maximize system throughput

## Related Work
 - Single processor MT scheduling algorithms have studied usage patterns and use this info to make further scheduling decisions resulting in a 17% performance increase
	 - However, it does not scale well
	 - The scheduler will try some combinations that do not work well resulting in poor performances
 - This paper proposes modeling relative resource contention and using this as a basis for decisions

## Experimental Platform
 - They built a CMT system simulator toolkit using the Simics simulation toolkit
	 - Models systems with multithreaded CPU cores
		 - The number of cores and degree of hardware multithreading is configurable
 - An MT core has multiple *hardware contexts*, usually 1, 2, 4, or 8 each with their own set of registers and thread state.
	 - By switching to other threads when one thread becomes blocked due to cache miss (etc) another thread can run. This hides latences and is "at the heart of hardware multithreading."
 - The simulated CPU has a simple RISC pipeline with shared TLB (Translation Lookaside Buffer) and L1 data and instruction caches. The L2 cache is shared by all the CPU cores on the chip.

## Studying Resource Contention
 - Threads that share a processor compete for resources. There are many resources, but this paper focuses on the processor pipeline as this is the difference between single-threaded and multi-threaded processors
 - The scheduler needs to decide which threads to run on the same processor, and which threads to run on different processors.
 - To efficiently schedule, we need to understand the causes (and effects) of contention
 - The key is *instruction delay latency*
 - When a thread is blocked, subsequent instructions are delayed until the operation completes
	 - ALU instuctions have a 0 delay latency
	 - Hitting the L1 cache has a latency of 4 cycles
	 - A branch has a delay of 2 cycles
 - Processor pipeline contention depends on the latencies of the instructions the workload executes
	 - In a thread with lots of memory access, contention is low and other threads can jump in
	 - However, if a thread does mostly ALU operations, it keeps the pipeline busy at all times.
		 - Thus the other threads on the same processor will suffer.
 - In order to assess the pipeline contention, the paper compares a MT core to a single thread core.
	 - The conventional processor provides the lower bound
	 - The mutliprocessor provides the upper bound
 - When contention is low, a MT core should approach the performance of a MP system
 - When contention is high, an MT core will be no better than a single core processor where a single thread uses all resources
 - See Figure 1 for CPU Bound workload
 - See Figure 2 for Memory Bound workload
 - See Figure 3 for a performance summary
 - The results show that the instuction mix and more precisely, the average instruction delay latency can be used as a **heuristic** to approximate pipeline requirements and use this to schedule
	 - It is logical to co-schedule long latency with short latency to fully utilize the functional units
	 - Note, this does not take into account cache contention
	 - Also note that the working set size of memory affects the throughput due to L1 cache contention
 
 ## Scheduling
  - Based on the information above, mean Cycles Per Instruction (CPI), easily determined using hardware counters, can be used to get an idea of the instruction mix of a workload. 
	  - High CPI threads have low pipeline requirements as they are often blocked in memory or executing long latency instructions
	  - Threads with low CPI have high resource requirements as they are rarely stalled
  - Based on modeling, mixing low and high CPI can result in a 2x improvement in throughput. See Figure 5 for details
  - So, what should a scheduler do in practice?
	  - They found that
		  - Measured CPI can vary by an order of magnitude among applications
		  - Given recent behaviour, they can predict near-term CPI
	  - Given this range and predictability, a scheduler that balances loads across CPU cores on a CMT processor should be a simple engineering matter

## Summary
 - Using CPI works well for simple workloads
 - However there are limitations as CPI does not give precise information about the types of instructions the workload is executing
	 - i.e. It doesn't tell what other resources a thread might be competing for such as the cache or FPU (Floating Point Unit)
	 - Also, the CPI measurement may be itself affected by resource contention on an already busy system.
 - This paper has demonstrated that CMT systems need new schedulers b/c a naive schduler can waste up to 50% of application performance and existing SMT scheduling algorigthms do not scale to dozensof threads.
 - A CPI-based scheduler can expect a 2x improvement over a naive scheduler.

# About these Notes

**Notes by [Christian Thompson](https://christianthompson.com/)**
My apologies for any errors or missing information.

Notes created using [Joplin](https://joplinapp.org/), a free and open source Evernote alternative.