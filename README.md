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
<li><b>void nsys9(int a1, state_t *excState)</b>: return the PID of the current process or of the father if it has one;allows us to use the GetProcessId() function;
<li><b>void nsys10(state_t *excState)</b>: the process calling this procedure decides to yield, which means to release the use of the CPU in his current time slice;
</ul>

# PandOSsh Phase 3

The <b>Phase 3</b> implementation is located in the directory whose name is <b>phase3/</b>. Additionlay the test files are found in the <b>tetsers/</b> folder.

## Virtual Memory Support
The <b>vmSupport</b> module contains the procedures to manage the virtual memory and the Support Level's TLB exception. Though the pager it handles page faults and loads missing pages from flash devices into the physical
frame, maintaining the Swap Pool. The main data structure of this module is the <b>swapPool[POOLSIZE]</b>: Each entry (`swap_t`) tracks the physical frame allocation, containing the ASID of the owner, the logical page number, and a pointer to the corresponding Page Table entry.

The functions that this module provides are the following:
<ul>
<li><b>void initPagetable(pteEntry_t *pageTable, int asid)</b>: Maps the first 31 page table entries to the code/data segments and the last to the stack.
<li><b>void uTLB_RefillHandler(void)</b>: Handles TLB misses by loading the correct page table entry into the TLB and resuming execution.
<li><b>void initSwapPool()</b>: Marks all swap frames as free and invalidates page numbers and ASIDs.
<li><b>int getFlashBlock(int vpn)</b>: Computes the Flash Device block corresponding to a given virtual page number.
<li><b>devreg_t *getFlashRegister(int asid)</b>: Returns the memory address of the Flash Device registers for a U-Proc based on its ASID.
<li><b>int readWriteFlashdrive(int asid, int vpn, int phisicalFrame, int op)</b>: Translates VPN to a block, sets the frame address, and performs the read/write syscall.
<li><b>void pager()</b>: Handles page faults by selecting a swap frame, writing back if dirty, loading the missing page, updating the page table and TLB, and managing access with a semaphore.
</ul>

## Positive Syscalls and General Exception Handler

The <b>sysSupport.c</b> and <b>syscalls.c</b> modules implement the Support Level general exception handler and the positive syscalls used by U-procs.

The <b>sysSupport.c</b> file handles all non-TLB exceptions passed up by the Nucleus to the Support Level. In particular, it distinguishes between positive syscall exceptions and Program Trap exceptions.

The most important functions in <b>sysSupport.c</b> are:
<ul>
<li><b>void generalExceptionHandler()</b>: entry point for Support Level general exceptions. It obtains the current process Support Structure using <b>GETSUPPORTPTR</b>, reads the saved exception cause and dispatches the execution either to the positive syscall handler or to the Program Trap handler.
<li><b>void programTrapHandler(support_t *sup)</b>: handles Program Trap exceptions. In this implementation, a Program Trap causes the current U-proc to terminate through <b>sys2()</b>.
<li><b>void pos_syscallHandler(support_t *sup)</b>: handles positive syscalls requested by U-procs. It reads the syscall number from register <b>a0</b>, increments the saved PC by one word to avoid repeating the same syscall, and dispatches the request to the correct syscall function.
</ul>

The positive syscalls handled by <b>pos_syscallHandler()</b> are:
<ul>
<li><b>TERMINATE</b>: handled by <b>sys2()</b>;
<li><b>WRITETERMINAL</b>: handled by <b>sys4()</b>;
<li><b>READTERMINAL</b>: handled by <b>sys5()</b>;
<li><b>EXECUTE</b>: handled by <b>sys6()</b>.
</ul>

If the syscall number is not valid, the current process is terminated.

### Functions in syscalls.c

The <b>syscalls.c</b> file contains the implementation of the Support Level positive syscalls.

<ul>
<li><b>void sys2(support_t *sup)</b>: implements the positive syscall <b>TERMINATE</b>. It terminates the current U-proc in an orderly way. If the process was holding the Swap Pool mutex, the mutex is released before termination. If the process is the shell, it performs a V operation on <b>masterSemaphore</b>, allowing the Instantiator Process to continue. Otherwise, it performs a V operation on <b>shellSemaphore</b>, allowing the shell to resume after the executed program terminates. It also marks all Swap Pool frames owned by the terminating ASID as free, then calls the Nucleus <b>TERMPROCESS</b> syscall.
<li><b>void sys4(support_t *sup)</b>: implements <b>WRITETERMINAL</b>. It validates the string address and length, writes characters to terminal 0 and returns the number of transmitted characters in <b>a0</b>.
<li><b>void sys5(support_t *sup)</b>: implements <b>READTERMINAL</b>. It validates the buffer address, reads characters from terminal 0 until a newline is received and returns the number of read characters in <b>a0</b>.
<li><b>void sys6(support_t *sup)</b>: implements the positive syscall <b>EXECUTE</b>. It can only be called by the shell. It reads the ASID of the program to execute from register <b>a1</b>, checks that the ASID is valid and different from the shell ASID, creates the new U-proc with <b>createUProc()</b>, and then blocks the shell on <b>shellSemaphore</b> until the child process terminates.
</ul>

## U-proc Initialization

The <b>initProc.c</b> module contains the initialization logic for Phase 3 and the creation of U-procs. This file implements the Instantiator Process, which is the first process launched by the Nucleus during Phase 3.

The main global variables exported by this module are:
<ol>
<li><b>masterSemaphore</b>: semaphore used to block the Instantiator Process until the shell terminates;
<li><b>shellSemaphore</b>: semaphore used to block the shell while an executed user program is running;
<li><b>termWriteSemaphore</b>: mutual exclusion semaphore for terminal write operations;
<li><b>termReadSemaphore</b>: mutual exclusion semaphore for terminal read operations;
<li><b>supportPool[UPROCMAX]</b>: static array of Support Structures, one for each possible U-proc.
</ol>

### Functions in initProc.c

<ul>
<li><b>int createUProc(int asid)</b>: creates a new U-proc associated with the given ASID. It checks that the ASID is valid, selects the correct Support Structure from <b>supportPool</b>, initializes the processor state and Support Structure through <b>initUProc()</b>, and finally calls the Nucleus <b>CREATEPROCESS</b> syscall. It returns the PID of the new process or <b>NOPROC</b> in case of failure.
<li><b>void test()</b>: Instantiator Process of Phase 3. It initializes the Support Level semaphores, initializes the Swap Pool through <b>initSwapPool()</b>, creates the shell using <b>createUProc(SHELL_ASID)</b>, waits on <b>masterSemaphore</b> until the shell terminates, and finally terminates itself using <b>TERMPROCESS</b>.
</ul>

The actual setup of the initial processor state, private page table and Support Level exception contexts is performed by the helper function <b>initUProc()</b>, defined in <b>helpers.c</b>. This function initializes:
<ul>
<li>the initial processor state of the U-proc;
<li>the ASID inside the Support Structure;
<li>the context for TLB/Page Fault exceptions;
<li>the context for general exceptions;
<li>the private page table of the process.
</ul>

## Shell

The <b>shell.c</b> file implements the user shell. The shell is an interactive U-proc that reads commands from terminal 0 and executes the corresponding user programs.

The shell uses a static mapping between program names and ASID values:
<ul>
<li><b>fibEight</b>: ASID 2;
<li><b>echo</b>: ASID 3;
<li><b>fibEleven</b>: ASID 4;
<li><b>uname</b>: ASID 5;
<li><b>date</b>: ASID 6;
<li><b>sl</b>: ASID 7;
<li><b>calc</b>: ASID 8.
</ul>

### Functions in shell.c

<ul>
<li><b>void trimSpaces(char *s)</b>: removes leading and trailing spaces from the command inserted by the user.
<li><b>void chkExitAndTerminate(char *buff)</b>: checks if the inserted command is <b>exit</b>. If so, the shell terminates itself by calling the positive <b>TERMINATE</b> syscall.
<li><b>void main()</b>: main loop of the shell. It prints the prompt <b>&gt; </b>, reads a command from terminal, removes the final newline, trims spaces, checks for the <b>exit</b> command, searches the command in the static ASID table and, if found, executes it through the positive <b>EXECUTE</b> syscall. If the command is not found, it prints an error message.
</ul>

The shell executes one program at a time. When a valid command is inserted, the shell calls <b>SYS6</b> and remains blocked until the executed process terminates.

## Calc

The <b>calc.c</b> file implements a simple calculator program. It is executed from the shell by typing:

calc

The program reads a simple arithmetic expression composed of:

<ul> <li>one single-digit number; <li>one operator; <li>one second single-digit number. </ul>

The supported operators are:

<ul> <li><b>+</b>: addition; <li><b>-</b>: subtraction; <li><b>*</b>: multiplication; <li><b>/</b>: integer division. </ul>

Example of valid inputs:

3+4,
3 + 4,
8/2,

Functions in calc.c
<ul> <li><b>void deleteSpaces(char *expr, int len, char *newExpr)</b>: copies the input expression into a new buffer while removing spaces. <li><b>int asciiToInt(char *expr, int *first, char *opr, int *second, int len)</b>: parses the expression, converts the two operands from ASCII to integers and checks that the operator is valid. <li><b>int calculate(int num1, char opr, int num2)</b>: executes the requested arithmetic operation and returns the result. Division by zero is detected and causes the program to terminate. <li><b>void main()</b>: main function of the calculator. It asks for an expression, reads it from terminal, parses it, computes the result, prints it and terminates. </ul>

The program terminates using the positive <b>TERMINATE</b> syscall after printing the result or after detecting an invalid input.


# Compiling and running

To compile and run the project using cmake follow these instructions using the terminal:<br>

## Building

```bash
make -C testers/
cmake -B build
cmake --build build
```

## Running

```bash
uriscv
```
