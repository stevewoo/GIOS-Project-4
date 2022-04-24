Rosenblum - Virtual Machine Monitors: Current Technology and Future Trends

# Virtual Machine Monitors: Current Technology and Future Trends

## Introduction
 - VMMs were important in the mainframe era due to the cost and scaricty of computers
 - However, in the 80s and 90s computers became commoditized and more affordable reducing the need for virtualization
	 - Also, these early personal computers lacked the architecture to run VMMs efficiently
	 - By they late 80s, these VMMs were viewed as historical curiosities!
 - By the 90s, researchers were looking for a way to improve massively parallel processing machines and found the solution in VMMs
	 - This led to VMware and VMMs for commodity computers

## Why the Revival?
 - Machines were underused and needed space and management (physically)
	 - Also, the operating systems were capable, but fragile
	 - To minimize issues, they were running one app per machine (expensive and inefficient)
 - Thus, VMs were once again seen as a solid cost/benefit
 - Originally used for multitasking, VMMs will be used for security and reliability
	 - Allows for easier migration and better security

## Decoupling Hardware and Software
 - Decoupling hardware and software lets VMM control Guest Operating Systems
 - Also VMs provide a consistent hardware interface for the Guest OS (can be different to the real underlying hardware as the VMM does the necessary translation)
 - VMM completely encapsulates a system state so it can map and remap and migrate the system at will
	 - Makes load balancing and scaling relatively easy
	 - Makes it easy to suspend and resume systems, plus checkpoint and backup and restore easily
 - VMM provides strong isolation as it provides *total mediation* of all system resources
 - Can consolidate a bunch of low resource VMs on one machine to save hardware cost and space
 - Allows apps to be isolated - an app that crashes only brings down its VM, not all the other VMs on the same machine
	 - If hackers compromise a single app, it only affects that VM

## VMM Implementation Issues
 - The VMM must share a hardware interface compatible with the underlying hardware
 - Design goals for a VMM are:
	 - Compatibility
	 - Performance
	 - Simplicity

## CPU Virtualization
 - A CPU is *virtualizable* if it supports *direct execution* fo the VM on the real machine
	 - When a GuestOS tries a priviliged operation, the VMM steps in and emulates it
		 - For example a VMM cannot allow a GuestOS to disable interrupts, rather it simulates it for the relevant VM

### Challenges
 - At the time, CPU architectures were not designed to be virtualizable
	 - EX: X86 POPF instruction does not trap in unprivileged mode
	 - ES: X86 allows unprivileged instructions to read privileged state so this is not trapped

### Techniques
 - Paravirtualization: replace parts of the original instruction set with more easily virtualized and efficient equivalents
	- EX: Instead of the POPF instruction accessing the CPU register, it accesses a special memory location set aside for the purpose
	- However, this requires the OS makers to alter their software to match
		- So, legacy systems cannot run
- On the Fly Binary Translation
	- Change the instructions as they come through using a binary translator which acts as a patch on the X86 instructions
	- Some binary translators convert between different CPU instruction sets for wider compatibility
		- This is similar to paravirtualization in that normal instructions are executed as is
	- This does not require the OS or Apps to be modified 
	- There is of course, some overhead, but it runs quickly once the translated routines are cached
	- Binary Translation also has the added benefit of avoiding traps which incur a performance hit especially on deep instruction pipeline architectures such as X86

### Future Support
 - AMD and Intel both announced hardware support for VMMs
	 - They do this by adding a new execution mode
		 - This should decrease the performance overhead

## Memory Virtualization
 - Uses a shadow page system to make sure each VM accesses the memory assigned
	 - Can use disk storage as extended memory (larger than physical)

### Challenges
 - The VMM doesn't have much information about what sections of memory the VM Guest OS is using so it may page out (to disk) necessary or often accessed memory
	 - One solution is to use a special process called a balloon process which asks for memory which the Guest OS grants - the VMM then swaps this memory out as it is clearly not needed by the rest of the system
 - Modern OSs are large - so if you are running 3 copies of an OS on one physical machine, you've loaded all the libraries etc 3 times which is a waste of space. 
	 - VMMs can track which pages are identical, delete the identical pages and point them all at the same copy. WOW!
	 - EX. An X86 computer might have 30 copies of Windows 2000 running, but only one physical copy of the kernel in the computer's memory.

### Future Support
 - Hardware-managed shadow page tables have been used in mainframes - this idea could be applied to X86 as well
 - Resource management cooperation between operating systems and VMMs are begin researched

## I/O Virtualization
 - In the 50's / 60's I/O was relatively simple and passed off to separate channel processors
 - However, modern systems have a wider range of I/O needs 

### Challenges
 - Wider variety of peripherals from different vendors with different programming interfaces
 - Some devices such as graphics and network devices have high performance requirements
 - The VMM will take an I/O call and translate it to the Host OS using its drivers
 - The VMM renders the VM display card in a window on the Host OS which lets the Host OS manage the display driver
 - Hosted Architecture has three advantages
	 - the VMM can be installed like any other app
	 - the VMM can accommodate any device that works on the Host OS
	 - the VMM can use the scheduling, resource management, and other services of the Host OS
 - Hosted Architecture also has disadvantages:
	 - Performance overhead for I/O device virtualization
	 - Windows and Linux do not have resource-managment support necessary for isolation and performance guarantees
 - ESX Server does not run on a Host OS
	 - It uses Linux drivers to talk directly to devices which lowers the virtualization overhead

### Future Support
 - Increased hardware support for I/O device virtualization
 - USB and SCSI are more channel-like compared to PS2 mice and keyboards (for example)
	 - These I/O interfaces ease implementation complexity and reducve virtualization overhead
 - In order to ensure isolation, VMM aware I/O devices will need to only have access to the memory found in the shadow tables for a particular VM
 - In systems with multiple VMs, there needs to be a mechanism to route I/O interrupts to the correct VM

# What's Ahead?

## Server Side
 - Centralized management of server farms
	 - provision, monitor, and manage
 - Hot migration from one physical server to another
 - Hardware failures dealt with by migration of VM
 - Virtualization avoids the need for maintenance downtime, end of equipment leases, and deploying hardware upgrades

## Beyond the Machine Room
 - For desktops, VMMs provide a consistent environment for software to run in no matter the changes to the underlying hardware
	 - No need to buy and maintain redundant infrastructure especially for older applications
 - Easier testing - a software developer can spin up a VM to test on a particular platform
 - Can take your own computing environment wherever you go

## Security Improvements
 - Security software can run alongside a virtual machine rather than in it - this provides an added layer of security should an application or Guest OS become compromised
	 - Can also limit a VM's access to the network and monitor it more closely through the VMM
 - Can specify the complete software stack improves security
	 - Different stacks can have different security levels (for example in government use)
 - The ability to quickly spin up new VMs will help when dealing with sudden resource needs or DDOS attacks

## Software Distribution
 - Instead of distributing software, companies can distribute full virtual machines
	 - Gives better control of the environment a piece of software runs on
	 - Currently used for demonstration purposes, but can be used in production as well
 - Virtual appliances are where apps are distributed as VMs that are treated as stand-alone apps
 - However, this change will require software vendors to update their license agreements
	 - For example, per CPU pricing will have to change to site or use-based licenses

## Summary
 - VMM resurgence is altering how software and hardware designers view, manage, and structure complex software environments
 - VMMs also allow for backward compatibility
 - The use of VMs will expand and will provide the fundamental building block for mobility, security, and usability on the desktop

# About these Notes

**Notes by [Christian Thompson](https://christianthompson.com/)**
My apologies for any errors or missing information.

Notes created using [Joplin](https://joplinapp.org/), a free and open source Evernote alternative.
