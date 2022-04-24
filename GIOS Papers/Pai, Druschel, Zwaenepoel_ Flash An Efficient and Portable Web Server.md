Pai, Druschel, Zwaenepoel: Flash An Efficient and Portable Web Server

# Flash: An Efficient and Portable Web Server
 - by Pai, Druschel, and Zwaenepoel

## Abstract
 - asymmetric multi-process event-driven (AMPED) architecture
 - Comines high performance of single process event driven servers with the performance of multi-process and multi-threaded servers on disk-bound workloads

## Introduction
 - Web servers cache frequently request content in main memory
	 - avoids disk latency inssues
 - Approaches
	 - single-process event-driven (SPED) architecture can provide excellent performance for cached workloads, where most requested content can be kept in main memory.
		 - Ex: Zeus / Squid Proxy
	 - multi-process (MP) or multi-threaded (MT) architecture
		 - Ex: Apache
 - Flash Server matches the performance of SPED and matches or exceeds MP/MT approaches

## Background
 - Serving a request
	 - Accept Client Connection
		 - ```accept``` on ```listen``` socket
	 - Read request
		 - read the HTTP request header and parse the header
	 - Find file
		 - Check if the file exists and the client has permissions
		 - File size and last modification time are obtained for inclusion in the response header
	 - Send response header
		 - transmit HTTP response header
	 - Read file
		 - read the file data (or chunks)
	 - Send data
		 - transmit the requested content
 - All steps of serving a request can potentially block

## Server Architectures

### Multi-process (MP)
 - Each request is assigned to one process
	 - The OS takes care of switching processes (20-200 at a time)
	 - No synchronization is necessary
	 - But hard to optimize

### Multi-threaded (MT)
 - Multiple threads in a shared address space
 - Each thread manages a request (like processes are used in MP)
 - Big difference is that the threads share global variables
 - But, must use some form of synchronization
 - The OS must support kernel threads. 
	 - FreeBSD does not as it only has user-level threads w/o kernel support

### Single-process event-driven (SPED)
 - Single processs processes HTTP requests
 - Use non-blocking system calls to perform asynchronous I/O operations
	 - Ex ```select``` on UNIX or ```poll``` on System V to check for I/O operations that have completed
 - A SPED Server can be thought of as a state machine that performs one step of an HTTP request at a time
	 - ```select``` is performed to check for completed I/O events
		 - When one is ready, the corresponding step is carried out and the next step is initiated
 - The single thread controls CPU, Disk, and Network operations
	 - This avoids the overhead of context switching and thread synchronization (in the MP and MT architectures)
 - The big challenge is lack of support for asynchronous disk operations
	 - non-blocking ```read``` and ```write``` operations work on network sockets and pipes, but block when used on disk files
 - Some UNIX systems have APIs that implment asynchronous disk I/O but they are not integrated with ```select```.  Also ```open``` and ```stat``` on file descriptors may still block

### Asymmetric Multi-Process Event-Drive (AMPED)
 - Combines event-driven SPED architecture with multiple *helper* processes (or threads) to handle blocking disk I/O operations
 - The main process handles all HTTP request processing.
 - Disk I/O is handled via IPC (pipe) to helper threads that handle the disk stuff
	 - The helper returns notification via IPC
	 - The main process learns of this vai ```select```
 - In a UNIX system, AMPED uses the standard non-blocking ```read```, ```write```, and ```accept``` system calls on sockets and pipes, and the ```select``` system call to test for I/O completion. The ```mmap``` operation is used to access data from the filesystem and the ```mincore``` operation is used to check if a file is in main memory.

## Design Comparison
 - Disk Operations
	 - In APMED The helper process is blocked, due to I/O, but not the main process
		 - But, the IPC is an extra cost
	 - In SPED all processing is blocked when there is disk activity
 - Memory Effects
	 - SPED has small memory requirements
	 - MT has additional memory consumption proportional to the number of concurrent HTTP requests
	 - AMPED helpers have overhead, but they are relatively small
	 - MP has the cost of separate processes (memory and kernel overhead)
 - Disk Utilization
	 - Since the AMPED model has one request per helper, it benefits from multiple disks and disk head scheduling

### Cost/Benefits of optimization & features
 - Information Gathering
	 - Web servers using info about recent requests to improve performance
		 - This has overhead
		 - In the MP model, IPC must be used to consolidate the data
		 - The MT model requires per-thread stats and periodic consolidation, or fine-grained synchronization on global variables
		 - SPED and AMPED models simplify info gathering since all requests are processed in a centralized fashion which eliminates the need for synchronization or IPC
 - Appplication-level Caching
	 - Use memory to store previous results such as response headers and file mappings for frequently used content
		 - But the cache competes with the filesystem cache for memory
	 - In the MP Model, each process may have its own cache to reduce IPC and synchronization
		 - Multiple caches lead to less efficient memory usage
	 - MT model uses a single cache, but data access/update must be synchronized to avoid race conditions
	 - AMPED and SPED can use a single cache without synchronization
 - Long-lived connections
	 - Server sized resources are committed for the duration of the connection
		 - In AMPED and SPED this cost is a file descriptor, application level connection info, and kernel state
		 - In MT and MP models, you need an extra thread or process for each connection

## Flash Implementation
 - Uses caching and other techniques to improve performance

### Overview
 - Single non-blocking server process assisted by helper processes
	 - The server pocess does all interaction with clients and cgi applications and controls the helper processes
	 - The helper processes perform all operations that result in synchronous disk activity
		 - These are separate processes (not threads) to support OSs that don't support kernel threads
 - The server is divided into modules that perform various steps and cacheing
	 - Three types of caches:
		 - filename translations, response headers, and file mappings

### Pathname Translation Caching
 - Maintains list of mappings from filenames to actual disk files
	 - Ex: /~bob -> /home/users/bob/public html/index.html
 - Reduces processing needed for pathname translations and the number of helpers 
 - Memory spent on cache is recovered in reduced memory used by helper processes

### Response Header Caching
 - HTTP servers prepend file data with a response header - this header can be cached and reused when files are repeatedly served
	 - If the file is changed, the header is regenerated

### Mapped Files
 - Cache of previously used files in memory
	 - Mappings of frequently requested files are kept and reused
 - All mapped file pages are tested for memory residency via ```mincore``` before use

### Byte Position Alignment
 - ```writev()``` alows applications to send multiple discontiguous memry regions in one operation
 - Web servers use this to send response headers followed by data
	 - This can cause misaligned data copying
		 - Caused by lengths of data not being multiples of the word length. To get around this, data is padded to 32 bytes

### Dynamic Content Generation
 - Dynamic content is served using forked processes and pipes
 - Data is transmitted to the server like static content, but it is read from a descriptor associated with the CGI process pipe instead of a file
	 - Since the CGI apps run in separate processes, they can block the disk w/o affecting the server

### Memroy Residency Testing
 - ```mincore()``` is used to determine if mapped file pages are memory resident
	 - Some OS systems don't have ```mincore()``` so other methods such as a timer are used

## Performance Evaluation
 - Apache vs Zeus vs Flash
 - TONS OF DATA HERE (SEE ORIGINAL PAPER FOR CHARTS AND DISCUSSION)

## Related Work
 - There is some related work. Didn't seem super important

## Conclusion
 - Flash nearly matches the performance of SPED servers on cached workloads while simultaneously matching or exceeding the performance of MP and MT servers on disk-intensive workloads. Moreover, Flash uses only standard APIs available in modern operating systems and is therefore easily portable
 - Results also show that the Flash server’s performance exceeds that of the Zeus Web server by up to 30%, and it exceeds the performance of Apache by up to 50% on real workloads.

# About these Notes
**Notes by [Christian Thompson](https://christianthompson.com/)**
My apologies for any errors or missing information.

Notes created using [Joplin](https://joplinapp.org/), a free and open source Evernote alternative.





