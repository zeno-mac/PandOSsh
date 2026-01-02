# PandOSsh Phase 1

This is the folder containing the <b> Phase 1 </b> implementation for <b>PandOSsh</b> operative system which is the 2025/2026 Operatig System project.

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

## Compiling and running
To compile and run the project using cmake follow these instructions using the terminal:<br>
### Building:

```bash
cmake -B build
cmake --build build
```
### Running:

```bash
uriscv
```



