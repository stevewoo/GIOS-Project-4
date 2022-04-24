Birrell: Introduction to Programming with Threads

# Introduction to Programming with Threads
 - Birrel, Andrew 1989

## Introduction
 - Concurrent programming is more challenging than nonconcurrent programming
 - A thread is a single sequential flow of control. 
 - Multiple threads are viewed as being executed simultaneously
 - Each thread executes on a seperate call stack with its own local variables
 - The programmer has to synchronize the threads to the shared (global memory)
 - Threads can be added to reduce latency so low priority actions can be carried out when the program is less busy, such as when waiting for user input

## Why use concurrency?
 - Threads can take advantage of multiple processors
 - I/O is slow. Threads can take advantage of the wait time
 - Humans are multitaskers!
	 - If a user makes multiple clicks, multiple threads will be spawned to take care of them
 - Helps in distributed systems - different threads per request


## THE DESIGN OF A THREAD FACILITY
  - 4 Major Mechanisms
	  - thread creation
	  - mutual exclusion (mutex)
	  - waiting for events
	  - getting thread out of a long term wait
### Thread Creation
  - Threads are created by calling "fork"
  - When a procedure returns, the thread dies
  - The fork returns a caller handle which is sometimes joined (ie join means the calling thread waits for the thread to die before continuing)
  - Ex: (**NOTE:** Code is in Modula-2)

```
	TYPE Thread;
	TYPE Forkee = PROCEDURE ( REFANY ): REFANY ;
	PROCEDURE Fork(proc: Forkee; arg: REFANY ): Thread;
	PROCEDURE Join(thread: Thread): REFANY ;
```
 - Example Code
```
	VAR t: Thread;
	t := Fork(a, x);
	p := b(y);
	q := Join(t);
```
 - This runs a(x) and b(y) in parallel
 - The reult of a(x) is returned to q
### Mutual exclusion
  - Simple way for threads to interact is via access to shared memory
	  - AKA global variables
	  - But programmers have to be careful two threads don't try to access the same shared variable simultaneously
  - Mutex is a data type of the language's LOCK construct
		
```
		TYPE Mutex;
		LOCK mutex DO ... statements ... END;
```

 - Mutex has two states
	 - locked and unlocked
	 - initially unlocked
	 - The thread inside is said to be "holding" the mutex. 
	 - If another thread tries to access it is blocked and is enqueued on the mutex until it is unlocked.
 - To achive mutex on a set of variables a programmer must
	 - associate them with a mutex
	 - Only access the variables from a thread that holds the mutex
```
	TYPE List = REF RECORD ch: CHAR ; next: List END ;
	VAR m: Thread.Mutex;
	VAR head: List;
	
	LOCK m DO
	newElement^.next := head;
	head := newElement;
	END ;
```

 - Simplest mutex is a global variable
 - But could also be part of a data structure
	 - Can be used to block access to a record while allowing access to other records

### Condition variables
 - Mutex is a simple kind of resource scheduling mechanism
	 - The resource is the shared memory accessed inside the LOCK clause
	 - The scheduling policy is one thread at a time
 - We need a way to block a thread until some event happens - in this case, a condition variable

	```
	TYPE Condition;
	PROCEDURE Wait(m: Mutex; c: Condition);
	PROCEDURE Signal(c: Condition);
	PROCEDURE Broadcast(c: Condition);
	```

 - Condition variables are always associated with a particular mutex and the data protected by that mutex
	 - the cv is always associated with the **same** mutex and data
 - In the code above, the Wiat unlocks the mutex and blocks the thread
 - The Signal awakens at least one blocked thread (if one exists)
 - The Broadcast awakens all threads that are blocked
 - When a condition is awoken inside the Wiat after blocking it relocks the mutex and then returns
	 - If the mutex is not available the thread will block on the mutex until it is available

### Alerts
- A mechanism for interrupting a thread which causes it to exit a long-term wait or computation
- Example Code
```
EXCEPTION Alerted;
PROCEDURE Alert(t: Thread);
PROCEDURE AlertWait(m: Mutex, c: Condition);
PROCEDURE TestAlert(): BOOLEAN ;
```
- Thread state includes "alert-pending" boolean set to false initially
- Alert Wait is similar to Wait except if the alert-pending boolean is true, instead of blocking on c (condition) it sets alert-pending to false, relocks m and raises the exception Alerted.
- TestAlert tests and clears the alert-pending boolean
- Code Example:
```
TRY
	WHILE empty DO Thread.AlertWait(m, nonEmpty) END ;
	RETURN NextChar()
EXCEPT
	Thread.Alerted: RETURN EndOfFile
END ;
```

## USING A MUTEX: ACCESSING SHARED DATA
 - In a multithreaded program all shared mutable data must be protected by some mutex
 - Only access that data from a thread that is holding the mutex

### Unprotected Data
 - Simplest bug - no mutex is used and two threads call the same method or operate on the same data
 - To avoid this, use a LOCK
```
PROCEDURE Insert(r: REFANY );
BEGIN
IF r # NIL THEN
	LOCK m DO
		table[i] := r;
		i := i+1;
	END ;
END ;
END Insert;
```
 - One simple approach is to lock ALL global variables but this has costs (addressed below)

### Invariants
 - An invariant is an associated value (boolean) that is set when a value needs to be locked
 - The mutex is held on this value, rather than the actual value
	 - So, in your code, you have to remember to set and unset this boolean
	 - True when the mutex needs held
	 - False when the mutex is released

### Cheating
 - The rule is to protect *every* global variable with a mutex
 - However, it is tempting to not do this with simple variables that you know your code will not alter out of turn.
	 - But, this depends on the compiler and architecture
 - TL;DR: Protect *every* global variable with mutex
 - That said, you could do something like this:

```
IF NOT initDone THEN
LOCK m DO
	IF NOT initDone THEN
		Initialize();
		initDone := TRUE ;
	END ;	
END ;
END ;
```

 - This works if you can assume that initDone is initialized to FALSE
	 - Useful only for tasks that are run once, and once only
	 - **Warning:** You need to make sure your programming language guarantess that your assumptions are valid

### Deadlocks involving only mutexes
 - Simplest deadlock case is trying to lock a mutex that is already held
 - More complicated case:
```
Thread A locks mutex M1;
thread B locks mutex M2;
thread A blocks trying to lock M2;
thread B blocks trying to lock M1.
```
 - To avoid this, ensure that M1 and M2 are always locked in the same order.
 - Another way to avoid this is to partition the data into smaller pieces protected by separate mutexes (this will depend on the algorithm you are using)
 - **Note:** Having a deadlock is better than your program returning incorrect information

### Poor performance through lock conflicts
 - Assuming you've avoided all the possible mutex conflicts and have protected your data, you need to think about performance
 - When a mutex is held, it is potentially stopping another thread from running
	 - This penalizes the total program throughput
		 - For example, a processor could sit idle
 - The *best way* to prevent this is to lock at finer granularity even thought this introduces complexity
 - Typical example is when a module manages a group of objects such as a set of open buffered files.
	 - A simple strategy would mutex for all operations (open, close, read, write, etc)
		 -However, this would prevent multiple writes on separate files
	 - So, the better strategy is to use one lock on the global list of open files and one lock per open file for operations affecting that file
	 - Example Code:
```
TYPE File = REF RECORD m: Thread.Mutex; .... END ;
VAR globalLock: Thread.Mutex;
VAR globalTable: ....;

PROCEDURE Open(name: String): File;
BEGIN
LOCK globalLock DO
	.... (* access globalTable *)
END ;
END Open;

PROCEDURE Write(f: File, ....);
BEGIN
	LOCK f^.m DO
	.... (* access fields of f *)
END ;
END Write;
```

 - In more complicated scenarios, you may need to lock different parts of a record with different mutexes. 
	 - **Careful:** Mutexes can also cause problems with the scheduler in a case where a higher priority thread is waiting on a mutex to be released from a lower priority thread
	 - Example (single processor architecture):
```
C is running (e.g. because A and B are blocked somewhere);
C locks mutex M;
B wakes up and pre-empts C
(i.e. B runs instead of C since B has higher priority);
B embarks on some very long computation;
A wakes up and pre-empts B (since A has higher priority);
A tries to lock M;
A blocks, and so the processor is given back to B;
B continues its very long computation.
```
 - In this case, the high priority A is unable to make progress until the low priority thread completes its work and unlocks M

### Releasing the mutex within a LOCK clause
 - This is possible using Acquire(m) and Release(m) instead of LOCK.
 - However it is not a good idea and is discouraged

## USING A CONDITION VARIABLE: SCHEDULING SHARED RESOURCES
 - CV is used when the programmer needs to schedule how multiple threads access a shared resources
	 - The simple one-at-a-time mutex is not sufficient
 - Code Example: Waits until some data is nonEmpty
```
VAR m: Thread.Mutex;
VAR head: List;
PROCEDURE Consume(): List;
	VAR topElement: List;
BEGIN
	LOCK m DO
		WHILE head = NIL DO Thread.Wait(m, nonEmpty) END ;
			topElement := head;
			head := head^.next;
		END ;
		RETURN topElement
END Consume;

PROCEDURE Produce(newElement: List);
BEGIN
	LOCK m DO
		newElement^.next := head;
		head := newElement;
		Thread.Signal(nonEmpty);
	END ;
END Produce;
```
 - **KEY POINT:** Use this pattern for all uses of condition variables
	```
	WHILE NOT expression DO Thread.Wait(m,c) END ;
	```
	 - This makes the program for robust and correct b/c you can verify correctness by locl inspection
	 - Also, this simplifies calls to Signal or Broadcast - extra wakeups are bening.
### Using "Broadcast"
 - Signal is used to awaken a single thread
 - Broadcast wakes all threads that have called Wait
 - If you use the above recommended pattern, then replacing Signal with Broadcast will also work
 - This is used when you have multiple threads reading data, but only want one thread active when  a thread is writing data
 - Using Broadast is simpler at a small performance cost
 - Example Code:
```
VAR i: INTEGER ;
VAR m: Thread.Mutex;
VAR c: Thread.Condition;

PROCEDURE AcquireExclusive();
BEGIN
	LOCK m DO
		WHILE i # 0 DO Thread.Wait(m,c) END ;
		i := -1;
	END ;
END AcquireExclusive;

PROCEDURE AcquireShared();
BEGIN
	LOCK m DO
		WHILE i < 0 DO Thread.Wait(m,c) END ;
		i := i+1;
	END ;
END AcquireShared;

PROCEDURE ReleaseExclusive();
BEGIN
	LOCK m DO
		i := 0; Thread.Broadcast(c);
	END ;
END ReleaseExclusive;

PROCEDURE ReleaseShared();
BEGIN
	LOCK m DO
		i := i-1;
		IF i = 0 THEN Thread.Signal(c) END ;
	END ;
END ReleaseShared;
```
 - In this case Broadcast is convenient in ReleaseExclusive as it doesn't need to know how many readers there are that need to proceed.
 - Alternatively, if you keep track of how many readers are waiting  using a counter, you can just call signal that many times.
 - You don't need to use Broadcast in ReleaseShared b/c at most one blocked writer can make progress
### Spurious wake-ups
 - By using Broadcast and only one mutex, you run the chance of multiple reader threads blocking a writer thread.
 - To avoid this, you want to use two mutexes, one for readers and one for writers.
 - The code would look like this:
```
VAR i: INTEGER ;
VAR m: Thread.Mutex;
VAR cR, cW: Thread.Condition;
VAR readWaiters: INTEGER ;

PROCEDURE AcquireExclusive();
BEGIN
	LOCK m DO
		WHILE i # 0 DO Thread.Wait(m,cW) END ;
		i := -1;
	END ;
END AcquireExclusive;

PROCEDURE AcquireShared();
BEGIN
	LOCK m DO
		readWaiters := readWaiters+1;
		WHILE i < 0 DO Thread.Wait(m,cR) END ;
		readWaiters := readWaiters-1;
		i := i+1;
	END ;
END AcquireShared

PROCEDURE ReleaseExclusive();
BEGIN
	LOCK m DO
		i := 0;
		IF readWaiters > 0 THEN
			Thread.Broadcast(cR);
		ELSE
			Thread.Signal(cW);
		END ;
	END ;
END ReleaseExclusive;

PROCEDURE ReleaseShared();
BEGIN
	LOCK m DO
		i := i-1;
		IF i = 0 THEN Thread.Signal(cW) END ;
	END ;
END ReleaseShared;
```
### Suprious lock conflicts
 - The code above can lead to excessive scheduling when a terminating reader calling signal and still has the mutex locked.
	 - There is a delay during which the next thread cannot get access and goes back on the scheduler which is expensive time-wise
		 - This is not a problem in single processor systems, but is a problem in multiple processor systems
 - Solution Code:
```
PROCEDURE ReleaseShared();
	VAR doSignal: BOOLEAN ;
BEGIN
	LOCK m DO
		i := i-1;
		doSignal := (i=0);
	END ;
	IF doSignal THEN Thread.Signal(cW) END ;
END ReleaseShared;
```	 
 - There is a more complicated case where other awoken reader will block trying to lock the mutex (not likely on a single processor)
 - To fix this, we awaken just one reader by calling Signal instead of Broadcast and having each awaken the next in turn
 - Example Code:
```
PROCEDURE AcquireShared();
BEGIN
	LOCK m DO
		readWaiters := readWaiters+1;
		WHILE i < 0 DO Thread.Wait(m,cR) END ;
		readWaiters := readWaiters-1;
		i := i+1;
	END ;
	Thread.Signal(cR);
END AcquireShared;
```
### Starvation
 - When a thread basically never gets a chance to run
	 - Usually the threading mechanism takes care of this, but when using condition variables
	 - In our example there are too many readers and they starve the writers.
	 - Ex Pattern
```
Thread A calls “AcquireShared”; i := 1;
Thread B calls “AcquireShared”; i := 2;
Thread A calls “ReleaseShared”; i := 1;
Thread C calls “AcquireShared”; i := 2;
Thread B calls “ReleaseShared”; i := 1;
etc.
```
 - To avoid this, we'd need to add a counter for blocked Writers
 - Example Code:
```
VAR writeWaiters: INTEGER ;

PROCEDURE AcquireShared();
BEGIN
	LOCK m DO
		readWaiters := readWaiters+1;
		IF writeWaiters > 0 THEN
			Thread.Signal(cW);
			Thread.Wait(m,cR);
		END ;
		WHILE i < 0 DO Thread.Wait(m,cR) END ;
		readWaiters := readWaiters-1;
		i := i+1;
	END ;
	Thread.Signal(cR);
END AcquireShared;

PROCEDURE AcquireExclusive();
BEGIN
	LOCK m DO
		writeWaiters := writeWaiters+1;
		WHILE i # 0 DO Thread.Wait(m,cW) END ;
		writeWaiters := writeWaiters-1;
		i := -1;
	END ;
END AcquireExclusive;
``` 
 - There is no limit to how complicated this can become with more elaborate scheduling policies
	 - Only add this type of thing if necessary due to the load on the system

### Complexity
- Dealing with spurious wake-ups, lock conflicts, and starvation make a program more complicated.
- You'll need to weight whether the potential cost is worth the increased complexity
	- If a resource is not used often, the the performance hit will not be so great
- Usually moving the "Signal" to beyond the end of the LOCK clause is easy and worth the trouble and the other enhancements are not worth it.
	- But, of course, it depends on the situation
### Deadlock
- Can be caused by condition variables
- Example:
```
Thread A acquires resource (1);
Thread B acquires resource (2);
Thread A wants (2), so it waits on (2)’s condition variable;
Thread B wants (1), so it waits on (1)’s condition variable.
```
 - Similar to the situation with mutexes 
 - I see an analogy to logic errors in coding
 - Mutexes and Condition Variables can interact causing a deadlock:
 - Example Code:
```
VAR a, b: Thread.Mutex;
VAR c: Thread.Condition;
VAR ready: BOOLEAN ;

PROCEDURE Get();
BEGIN
	LOCK a DO
		LOCK b DO
			WHILE NOT ready DO Thread.Wait(b,c) END ;
		END ;
	END ;
END Get;

PROCEDURE Give();
BEGIN
	LOCK a DO
		LOCK b DO
			ready := TRUE ; Thread.Signal(c);
		END ;
	END ;
END Give;
```
 - "If “ready” is FALSE and thread A calls “Get”, it will block on a call of “Wait(b,c)”. This unlocks “b”, but leaves “a” locked. So if thread B calls “Give”, intending to cause a call of “Signal(c)”, it will instead block trying to lock “a”, and your program will have deadlocked." - p.20
 - It also occurs when you lock a mutex at one abstraction level and all a different level. 
	 - It is risky to call a lower level abstraction when holding a mutex
		 - One solution is to explicitly unlock the mutex before calling the lower level abstraction
		 - The better solution is to end the LOCK clause before calling down
		 - This problem is referred to as the *nexted monitor problem* in the literature

## USING FORK: WORKING IN PARALLEL
 - Reasons to fork a thread
	 - utilize multiple processors
	 - do work while waiting for I/O
	 - Satisfy multitasking humans
	 - provide a network service to multiple clients
	 - defer work until a less busy time
 - Applications use several threads, for example:
	 - Computation thread
	 - Writing output to file thread
	 - User input thread
	 - Background thread to clean up data structures
 - If you don't want to wait for a device, create a thread
 - If you want multiple device request, use multiple threads
 - If you are interacting with a user, you'll want a separate thread to deal with and respond
	 - For example, GUIs
		 - Most GUIs already take care of this for you
			 - But if not, you'll need to deal with it yourself
			 - Is it short enough to do synchronously?
			 - Or should you create a new thread?
 - Network servers will require separate threads
	 - RPC protocol requires separate threads already
	 - If you are writing a client program, create a separate thread to contact the server
 - Strategies
	 - Once a procedure has a usable result, fork the thread and complete the rest of the work and return to the original thread
		 - However this could create large numbers of threads and forking has a time cost
	 - Instead, keep a single housekeeping thread and feed requests to it
		 - Also, the housekeeper can merge similar requests and only run after a certain period of time
 - On a multiprocessor, you'll need to use "Fork" to utilize as many processors as you can.

### Pipelining
 - For example, when thread A initiates an action, all it does is enqueue a request in a buffer.
 - Thread B takes the action from the buffer, performs part of the work, then enqueues it in a
second buffer. 
 - Thread C takes it from there and does the rest of the work.
 - There is some long example code on page 22 that is probably worth looking over
 - Challenges with Pipelining
	 - Careful how much work gets done at each stage
	 - The number of stages determines the amount of concurrency
		 - Not as much of an issue if you know how many processors you have and where the realtime delays occur

### The impact of your environment
 - The use of threds is impacted by the OS and the libraries you are using
 - You need to know 
	 - the cost of creating a thead
	 - the cost of keeping a blocked thread in existence
	 - the cost of a context switch
	 - the cost of a LOCK when a mutex is not locked
		 
### Potential problems with addding threads
 - If you have many more threads than processors, performance will degrade
	 - Schedulers are slow at rescheduling
		 - An idle processor is not a problem but enqueuing and swapping into a processor is more expensive
 - Multiple threads means more chances for mutex or conditional variable conflicts
 - Threads for driving devices, responding to mouse clicks, and RPC invocation are usually not a problem
 - Threads for performance purposes you need to be careful as they can overload the system.
 - These problems are found with threads that are ready to run.
	 - Having threads blocked by condition variables is not so expensive
 - Thread creation and termination is not cheap
	 - "Fork" will cost two or three rescheduling sessions so don't fork small calculations
		 - Need to find the smallest calculation where the benefit exceeds the cost
 - Need to find the balance between too few and too many threads

## USING ALERT: DIVERTING THE FLOW OF CONTROL
 - Alerts are used to terminate a long running computation or long-term wait
	 - For example, with multiprocessor systems, you can have different algorithms working on the same problem - the first to finish stops the others **INTERESTING**
	 - Or you can abort if a user clicks "cancel"
 - To do this
	 - long computations should call "TestAlert" occasionally. 
	 - potentially long waits should be called with AlertWait, instead of Wait.
	 - *NOTE:* In this context long means long enough to annoy a human user.
 - Benefit is to allow users control
 - Demerit is that programs must be prepared to deal with the effects of ending a process early
 - **CONVENTION:** Only alert a thread if you forked it.
 - Using alerts makes progrmas less well-structured.
	 - A straightforward flow can be interrupted
	 - Can make your program
		 - Unreadable
		 - Unmaintainable
		 - and sometimes incorrect
 - Alerts are most useful when you don't know exactly what is going on
	 - Could be block by any one of a number of issues
	 

## ADDITIONAL TECHNIQUES
### Up-calls
 - Calling a higher level abstraction
 - Example given is about networking. See p. 26
 - Calling lower level abstractions has led to bad performance
	 - Upcalling fixes this but at the cost of more complicated programming and synchronization issues
		 - Can lead to deadlock of mutexes due to the potential to violate teh partial order rule for locking mutexes
		 - Avoid by not holding mutexes when making an up-call

### Version Stamps
 - To avoid cache issues, use a version stamp
	 - When data is shared with different abstraction levels, use the version stamp. Any changes cause an increment in the version stamp
		 - So, returned data with a different version stamp lets the calling process (or called process) know which version of data is the current active/up-to-date one
 - This is also useful in distributed systems

### Work Crews
 - When you have more concurrency (threads) than can be run on your hardware.
	 - One solution is to keep a set number of threads and use them to run the necessary tasks
		 - So you enque tasks and only allocate them when there is an available thread
	 - Another solution is to defer creating a new thread until a processor is available
		 - "Lazy Forking"

## BUILDING YOUR PROGRAM
 - A successful program must be:
	 - useful
	 - correct
	 - live
	 - efficient
 - Concurrency affects all of the above
	 - You need to design interfaces with the assumption your callers are using multiple threads
		 - Don't return results in globally shared variables or global statically allocated storage
		 - Calls should be synchronous only returning when ready
			 - Caller can run other tasks in other threads
	 - "Correct" means your program produces an answer according to a specification
		 - Each piece of data must be associated with one, and only one, mutex
	 - "Live" means your program will eventually produce an answer
		 - The alternatives are infinite loops or deadlocks
	 - "Efficient" means the program makes good use of the avialable resources and produces its answer quickly (relative to the complexity)
		 - A good thread aware debugger is helpful here as will a system that provides stats on lock conflicts and concurrency levels
		 - Dont' emphasize efficiency at the exponse of correctness
			 - It's easier to make a correct program efficient than the other way around

## CONCLUDING REMARKS
 - Writing concurrent programs is neither exotic nor difficult
 - By using a system with good primitives and suitable libraries and along with the use of the techniques in this paper (and more) plus awareness of common pitfalls you can do it!


# About these Notes

**Notes by [Christian Thompson](https://christianthompson.com/)**
My apologies for any errors or missing information. I especially got lazy towards the end...it took almost four hours to read, understand (mostly), and take all these notes.

Notes created using [Joplin](https://joplinapp.org/), a free and open source Evernote alternative.
 
 
 
 
 
 
 
 
 
 
 
 
 