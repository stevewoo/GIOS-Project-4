Popek - Formal Requirements for Virtualizable Third Generation Architectures

# Formal Requirements for Virtualizable Third Generation Architectures

## Virtual Machine Concepts
 - DEF: A virtual machine is an *efficient, isolated duplicate* of the real machine.
 - A VMM (software) has three characteristics:
	 - provides an identical environment to the original system
	 - only show minor decreases in speed
		 - Most instructions should run directly on the CPU (no emulation)
	 - has complete control of system resources
		 - Programs running under the VMM only have access to resources explicitly allocated
		 - The VMM can get back control of resources when it wants
 - A virtual machine is the environment created by the virtual machine monitor (VMM)
 - A VMM can be, but is not necessarily a time-sharing system

## A Model of Third Generation Machines
 - Processor has two modes: Supervisor and User (AKA Privileged and Non-Privileged)
 - Memory addressing is done relative to a relocation register
 - Has the ability to lookup memory of arbitrary size and move it elsewhere (*table look-up and copy* property)
 - Finite number of states where each state has four components:
	 - E: Executable Storage
		 - E[i] for 0 <= i <= q where q is the max memory size
	 - M: Processor Mode
		 - Either s (supervisor) or u (user)
	 - P: Program Counter
		 - Address is relative to R
	 - R: Relocation-bounds Register
		 - R = (l, b) where l is the relocation offset and b is the bounds (max size of the virtual memory)
	 - Therefore: S = <E, M P, R>
 - <M, P, R> are referred to as *program status word* or *PSW*

### Traps
 - A trap activates if i(E _1_, M _1_, P _1_, R _1_) == (E _2_, M _2_, P _2_, R _2_)
	 - I think this is a fancy mathematical way of saying if one VM tries to access the memory of another VM
 - Memory Trap
	 - Happens when an instruction attempts to access memory outside its bound or outside the physical memory

## Instruction Behaviour
 - A trap occurs when a non-privileged user attempts a particular instruction that would not be trapped by a privileged user
	 - By this definition, privileged instruction are *required* to trap
		 - Can't NOP - you must deal with the instruction via a trap
 - Sensitive Instructions
	 - Control Sensitive: An instruction which either attempts to change the amount of memory or affects the processor mode without going through the trap sequence
	 - Behaviour Sensitive: An instruction whose behaviour depends on its location in real memory or on the mode.

## Virtual Machine Monitor
 - The VMM is a software implementation referred to here as a *control program*
 - Three groups
	 - Dispatcher: Its intial instruction is where the hardware trap points to
		 - This is the top-level control module of the control program
	 - Allocator: Decides which system resources are provided
		 - Keeps VMM and VM resources separate and keeps multiple VM resources separate from each other as well (isolation)
		 - Invoked by the dispatcher when there is an attempt to execute a priviliged instruction, for example, when attempting to reset the R register
	 - Interpreters: One interpreter routine per instruction that traps
		 - These simualate the effect of the instruction which trapped
 - A control program is specified by its three parts:
	 - CP = <D, A, {v _i_}>
		 - v _i_ refers to the set of instructions which can be trapped
 - We assume the control program runs in supervisor mode
 - We also assume that all other programs run in user mode

## Virtual Machine Properties
 - Efficiency Property
	 - All innocuous instructions are executed by the hardware directly
 - Resource Control Property
	 - Any attempt by a program to affect system resources is trapped by the allocator
 - Equivalence Property
	 - Program execution is indistinguishable from the case where it was running directly on the bare metal
	 - Of course the VMM takes space (as would other VMs) so we assume equivalence of a system with the same subset of resources given the VM
 - The only exceptions are timing issues and resource availability
	 - Programs cannot assume a consistent execution length
 - Therefore, a VMM is defined as any control program which satisfies the three properties of efficiency, resource control, and equivalence.
 - The environment a program sees is called the virtual machine
 - **Theorem 1:** For any conventional third generation computer, a virtual machine monitor may be constructed if the set of sensitive instructions for that computer is a subset of the privileged instruction set.

## Discussion of Theorem
 - Most of what has been discussed is already present in most systems - for example, privileged vs non-privileged instructions. The only really new postulate is Theorem 1 above.  The rest of this section goes on to discuss why this is true mathematically. Honestly, I can't follow it all, but I get the gist and doubt it is of any real importance to GIOS.

## Recursive Virtualization
 - Question: Is it possible for a VMM to run under itself?
 - **Theorem 2**: A conventional third generation computer is recursively virtualizable if it is: (a) virtualizable, and (b) a VMM without any timing dependencies can be constructed for it.
	 - The only excluding factors are programs which are resource bound or have timing dependencies

## Hybrid Virtual Machines
 - At the time of the publication (1974), few third generation architectures were virtualizable
 - A Hybrid Virtual Machine (HVM) is simiar to a VMM but has more interpreted instructions than directly executed instructions.
	 - This makes HVMs less efficient, but allows more third generation architectures to qualify
		 - Ex: PDP-10 can host an HVM monitor but not a VM monitor.
 - **Theorem 3**: A hybrid virtual machine monitor may be constructed for any conventional third generation machine in which the set of user sensitive instructions are a subset of the set of privileged instructions.
	 - The difference between HVM and VMM is that *all* instructions in virtual supervisor mode will be interpreted. 
		 - Equivalence and control are maintained b/c the HVM always has control or gains control via a trap and there are interpretive routines for all necessary instructions, hence all sensitive instructions are caught by the HVM and simulated

## Conclusion
 - This paper developed a formal model of a third generation computer system and derived the necessary and sufficient conditions to determine whether such a machine can support a VMM.

# About these Notes

**Notes by [Christian Thompson](https://christianthompson.com/)**
My apologies for any errors or missing information.

Notes created using [Joplin](https://joplinapp.org/), a free and open source Evernote alternative.