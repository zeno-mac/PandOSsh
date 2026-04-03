# PandOSsh Project

<b>PandOSsh</b> operative system which is the 2025/2026 Operatig System project. The implementation containd in this directory was made by Luca Argentino, Milo Disalvatore, Zeno Maccari and Matteo Mazzetti.

# PandOSsh Phase 1

The <b> Phase 1 </b> implementation is located in the directory named <b>phase1/</b>. The following text is a brief explanation of this part of the project.

## Process Control Blocks (PCBs)

In the <b>pcb.c</b> file you can find the implementations regarding PCBs queues and trees.
For both trees and queues the allocation/deallocation functions are essential for the maintainment of PCBs.
<br>The module includes also three global data structuses which are:
<ol>
<li> <b>pcbFree_table[MAXPROC]</b>: a static array containing the PCBs not used from the OS;
<li> <b>pcbFree_h</b>: head of the free PCBs array;
<li> <b>next_pid</b>: a int counter used to identify uniquely each PCB;
</ol>

### Functions in pcb.c

<ul>
<li>void initPcbs()
<li>void freePcb(pcb_t *p)
<li>pcb_t *allocPcb()
<li>int emptyProcQ(struct list_head *head)
<li>void insertProcQ(struct list_head *head, pcb_t *p)
<li>pcb_t *headProcQ(struct list_head *head)
<li>pcb_t *removeProcQ(struct list_head *head)
<li>pcb_t *outProcQ(struct list_head *head, pcb_t *p)
<li>int emptyChild(pcb_t *p)
<li>void insertChild(pcb_t *prnt, pcb_t *p)
<li>pcb_t *removeChild(pcb_t *p)
<li>pcb_t *outChild(pcb_t *p)
</ul>

## Active Semaphore List (ASL)

The <b>asl.c</b> module is for the management of the active semaphores which are the ones where at least one PCB is not blocked.
The data structures for this file are:
<ol>
<li> <b>semd_table[MAXPROC]</b>: a static array containing all the free semaphores;
<li> <b>semdFree_h</b>: head of the free semaphores array;
<li> <b>semd_h</b>: head of the active semaphore list;
</ol>

### Functions in asl.c

<ul>
<li>void initASL()
<li>int insertBlocked(int* semAdd, pcb_t* p)
<li>pcb_t* removeBlocked(int* semAdd)
<li>pcb_t* outBlocked(pcb_t* p)
<li>pcb_t* headBlocked(int* semAdd)
</ul>

# PandOSsh Phase 2

You can find <b> Phase 2 </b> implementation in the directory named <b>phase2/</b>. Let's shortly take a look at the content of said folder.

## Initialization

The <b>Initial.c</b> module implements the main() function and exports the global variables needed in the other files which are:
<ol>
<li> <b>readyQueue</b>: the list of the processes that are ready to be executed;
<li> <b>processCount</b>: an int whose job is to indicate the number of processes who started but didn't end yet;
<li> <b>softBlock_count</b>: a counter for all the processes that are currently blocked (e.g. waiting for an I/O);
<li> <b>currProc</b>: a pointer to the running pcb;
<li> <b>device_semaphores[NRSEMAPHORES]</b>: the semaphores that each external device could need.
</ol>

### Functions in initial.c

<ul>
<li> pcb_t* allocFirstPcb()
<li> void initVector()
<li> int main()
</ul>

## Scheduler

The file <b>scheduler.c</b> is the place where the scheduler implementation is set. Through the following procedure this module grants the Nucleus finite progress using a round-robin algorithm.

The only procedure in this file is <b>void dispatch()</b>.

## Interrupts

The <b>interrupts.c</b> module handles both timers (processor local timer and interval timer) and devices interrupt exceptions. The handled devices are the 5 entries in the list that follows.
<ol>
<li> Disk Devices
<li> Flash Devices
<li> Network Devices
<li> Printer Devices
<li> Terminal Devices
</ol>
The module is divided in four procedures:
<ul>
<li> <b>void pltInterruptHandler()</b>: manages only the processor local timer exceptions;
<li> <b>void itInterruptHandler()</b>: similarly to the previous one, here are handled the interval timer interrupts;
<li> <b>void deviceInterruptHandler(int excCode)</b>: handles the interrupt caused by any device of the list written above;
<li> <b>void interruptHandler()</b>: is just a wrapper function that calls one of the three procedures accordingly to the exception code.
</ul>

## Exceptions

This module implements TLB, Program Trap and Syscall exception handlers. The five most significant functions are the following:
<ul>
<li><b>void exceptionHandler()</b>:entry point for all exceptions (except TLB-Refill). It dispatches control to the appropriate handler based on the exception cause. In particular, it distinguishes between interrupts, TLB exceptions, SYSCALLs and Program Traps;
<li><b>void passUpOrDie(int)</b>: implements the *Pass Up or Die* mechanism. If the current process does not have a Support Structure, it is terminated (along with its progeny). Otherwise, the exception state is copied into the corresponding support structure area and control is transferred to the Support Level handler using `LDCXT`;
<li><b>void exc_tlbHandler()</b>: handles TLB-related exceptions. It simply invokes `passUpOrDie()` with the `PGFAULTEXCEPT` index;
<li><b>void exc_trapHandler()</b>:handles Program Trap exceptions and all unrecognized cases. It relies on `passUpOrDie()` with the `GENERALEXCEPT` index;
<li><b>void syscallHandler()</b>: handles SYSCALL exceptions. Valid nucleus syscalls (in the range `[-10, -1]`) are executed by dispatching to the corresponding `nsys#` function, while invalid or unauthorized requests are treated as Program Traps.
</ul>
Additionaly this module includes the <b>syscall.c</b> file which defines the various NSYS# system calls that are used in the many handlers of <b>exceptions.c</b>.

## System calls

As we already stated, this module ( file <b>syscall.c</b>) contains the implementation of the system call:
<ul>
<li><b>int nsys1(int a1, int a2, int a3)</b>: this service causes a new process to be created;
<li><b>void nsys2(int a1, state_t *excState)</b>: this service terminates a process and all of its progeny recursively;
<li><b>void nsys3(int a1, state_t *excState)</b>: using this function the Nucleus is asked to perform P() on a semaphore;
<li><b>void nsys4(int a1, state_t *excState)</b>: symmetrically, the Nucleus recieves a request of V() on a semaphore;
<li><b>void nsys5(int a1, int a2, state_t *excState)</b>: this syscall is used to block the current process as it needs to do an I/O operation;
<li><b>void nsys6(state_t *excState)</b>: using this function we request the accumulated time that a certain process has occupied the CPU;
<li><b>void nsys7(state_t *excState)</b>: this service let's you perform a semaphore P() on the Nucleus Pseudo-clock semaphore;
<li><b>void nsys8(state_t *excState)</b>: returns, if possible, the value of the current PCB p_supportStruct;
<li><b>void nsys9(int a1, state_t *excState)</b>: allows us to use the GetProcessId() function;
<li><b>void nsys10(state_t *excState)</b>: the process calling this procedure decides to yield, which means to refuse to use the CPU in his current time slice;
</ul>

# Compiling and running

To compile and run the project using cmake follow these instructions using the terminal:<br>

## Building

```bash
cmake -B build
cmake --build build
```

## Running

```bash
uriscv
```
