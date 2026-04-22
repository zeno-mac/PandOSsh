#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"
#include "../headers/exceptions.h"
#include "../headers/initial.h"
#include "../headers/scheduler.h"

#include "../headers/syscall.h"

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>

#include "../../phase1/headers/asl.h"
#include "../../phase1/headers/pcb.h"

#include "../headers/auxfun.h"
extern cpu_t tod_start;

extern struct list_head readyQueue;
extern int device_semaphores[NRSEMAPHORES];
extern pcb_t *currProc;
extern int softBlock_count;
extern int processCount;

extern struct list_head semd_h;

/*
 * Returns true if the given semaphore address belongs to the Nucleus-maintained
 * device semaphore array (including the pseudo-clock semaphore at index
 * NSUPPSEM). Used by terminateProcess to decide whether to decrement
 * softBlock_count.
 */
static int isDeviceSemaphore(int *semAdd) {
  return (semAdd >= &device_semaphores[0] &&
          semAdd <= &device_semaphores[NRSEMAPHORES - 1]);
}

/*
 * Recursively terminates the process pointed to by p and all of its progeny.
 * For each process:
 *   - If running (p == currProc): set currProc to NULL.
 *   - If blocked on a semaphore (p->p_semAdd != NULL): remove from ASL,
 *     decrement softBlock_count if it was waiting on a device/clock semaphore.
 *   - If ready (in the ready queue): remove from the queue.
 * Then detaches p from its parent's child list, decrements processCount,
 * and frees the PCB.
 */
static void terminateProcess(pcb_t *p) {
  if (p == NULL)
    return;

  while (!emptyChild(p)) {
    terminateProcess(removeChild(p));
  }

  if (p == currProc) {
    currProc = NULL;

  } else if (p->p_semAdd != NULL) {
    outBlocked(p);

    if (isDeviceSemaphore(p->p_semAdd)) {
      softBlock_count--;
    }

  } else {
    outProcQ(&readyQueue, p);
  }

  outChild(p);
  processCount--;
  freePcb(p);
}

/*
 * Recursively searches the process subtree rooted at 'root' for a process
 * with the given pid.
 * Returns a pointer to the matching PCB, or NULL if not found.
 */
static pcb_t *searchInTree(pcb_t *root, int pid) {
  if (root == NULL)
    return NULL;
  if (root->p_pid == pid)
    return root;

  struct list_head *pos;
  list_for_each(pos, &root->p_child) {
    pcb_t *child = container_of(pos, pcb_t, p_sib);
    pcb_t *found = searchInTree(child, pid);
    if (found != NULL)
      return found;
  }
  return NULL;
}

/*
 * Searches all semaphore queues in the Active Semaphore List (ASL)
 * for a process with the given pid.
 * Returns a pointer to the matching PCB, or NULL if not found.
 */
pcb_t *searchBlockedByPid(int pid) {
  semd_t *sem;
  list_for_each_entry(sem, &semd_h, s_link) {
    struct list_head *pos;
    list_for_each(pos, &sem->s_procq) {
      pcb_t *p = container_of(pos, pcb_t, p_list);
      if (p->p_pid == pid)
        return p;
    }
  }
  return NULL;
}

/*
 * Searches for a PCB with the given pid across all process structures:
 *   1. The subtree rooted at the current running process (currProc)
 *   2. The subtrees rooted at each process in the ready queue
 *   3. All processes blocked on semaphores in the ASL
 *
 * Returns a pointer to the found PCB, or NULL if not found.
 * Used by nsys2 when pid != 0.
 */
static pcb_t *findPcbByPid(int pid) {

  /* Search in the subtree of the currently running process */
  if (currProc != NULL) {
    pcb_t *found = searchInTree(currProc, pid);
    if (found != NULL)
      return found;
  }

  /* Search in the subtrees of all ready processes */
  struct list_head *pos;
  list_for_each(pos, &readyQueue) {
    pcb_t *p = container_of(pos, pcb_t, p_list);
    pcb_t *found = searchInTree(p, pid);
    if (found != NULL)
      return found;
  }

  /* Search among all blocked processes in the ASL */
  pcb_t *found = searchBlockedByPid(pid);
  if (found != NULL)
    return found;

  return NULL;
}

/*
 * NSYS1 — CreateProcess
 *
 * Creates a new child process of the current process.
 *
 * The new PCB is initialized, inserted into the ready queue, and made
 * a child of currProc. processCount is incremented.
 *
 * Returns: the PID of the new process, or NOPROC (-1) if no PCB is available.
 */
int nsys1(int a1, int a2, int a3) {
  state_t *newState = (state_t *)a1;
  int prio = a2;
  support_t *supportp = (support_t *)a3;

  pcb_t *new_pcb = allocPcb();
  if (new_pcb == NULL)
    return NOPROC;
  else {
    copyState(&new_pcb->p_s, newState);

    new_pcb->p_prio = prio;
    new_pcb->p_supportStruct = supportp;
    // Insert into the ready queue (ordered by priority)
    insertProcQ(&readyQueue, new_pcb);
    // Make the new process a child of the current process
    insertChild(currProc, new_pcb);
    processCount++;
    new_pcb->p_time = 0;
    new_pcb->p_semAdd = NULL;
    return new_pcb->p_pid;
  }
}

/*
 * NSYS2 — TerminateProcess
 *
 * Terminates a process and all of its progeny recursively.
 *   a1: PID of the process to terminate, or 0 to terminate currProc
 *
 * If a1 != 0 and the process is not found, returns NOPROC in a0.
 * After termination, calls dispatch() to schedule the next process.
 */
void nsys2(int a1, state_t *excState) {
  pcb_t *termPcb = NULL;

  if (a1 == 0) {
    termPcb = currProc;
  } else {
    termPcb = findPcbByPid(a1);
    if (termPcb == NULL) {
      excState->reg_a0 = NOPROC;
      LDST(excState);
      return;
    }
  }

  terminateProcess(termPcb);
  dispatch();
}

/*
 * NSYS3 — Passeren (P operation)
 *
 * Performs a P (wait) operation on the semaphore at address a1.
 *   a1: physical address of the semaphore
 *
 * If *semAdd > 0: decrement the semaphore and return to the caller.
 * Otherwise: save the process state, update p_time, block the process
 * on the ASL, and call dispatch().
 */
void nsys3(int a1, state_t *excState) {
  int *semAdd = (int *)a1;
  if (*semAdd > 0) {
    // Semaphore available: decrement and continue execution
    (*semAdd)--;
    LDST(excState);
  } else {
    /*
     * Semaphore not available: block the current process.
     * Save the updated processor state (PC already incremented) into the PCB.
     */
    copyState(&currProc->p_s, excState);

    cpu_t currentTime;
    STCK(currentTime);
    currProc->p_time += (currentTime - tod_start);
    insertBlocked(semAdd, currProc);

    currProc = NULL;
    dispatch();
  }
  return;
}

/*
 * NSYS4 — Verhogen (V operation)
 *
 * Performs a V (signal) operation on the semaphore at address a1.
 *   a1: physical address of the semaphore
 *
 * If a process is blocked on the semaphore: unblock it and insert it
 * into the ready queue (does NOT increment softBlock_count — that was
 * not incremented by NSYS3 for user semaphores).
 * Otherwise: increment the semaphore value.
 *
 * The V operation never blocks the calling process.
 */
void nsys4(int a1, state_t *excState) {
  int *semAdd = (int *)a1;
  pcb_t *unblocked = removeBlocked(semAdd);

  if (unblocked != NULL) {
    insertProcQ(&readyQueue, unblocked);
  } else {
    (*semAdd)++;
  }
  return;
}

/*
 * NSYS5 — DoIO
 *
 * Initiates a synchronous I/O operation and blocks the current process
 * until the device interrupt signals completion.
 *   a1: physical address of the device's command register
 *   a2: command value to write to the command register
 *
 *   Compute the semaphore index from the command register address
 *   and write the command to the device register.
 *   Increment softBlock_count and block the process on 
 *   the device semaphore,  then call dispatch().
 */
void nsys5(int a1, int a2, state_t *excState) {
  /*
   * Compute the base address of the device register by zeroing
   * the last 4 bits (device registers are aligned to 0x10 bytes).
   */
  int devRegBase = a1 & 0xFFFFFFF0;
  // Compute offset from the start of the device register area
  int offset = devRegBase - START_DEVREG;
  // Line index: 0=DISK, 1=FLASH, 2=ETH, 3=PRINTER, 4=TERMINAL
  int lineIndex = offset / 0x80;
  /* Device number within the line (0..7) */
  int devNo = (offset % 0x80) / 0x10;
  // Base semaphore index in device_semaphores[]
  int semIndex = lineIndex * DEVPERINT + devNo;
  /*
   * For terminal devices (lineIndex == 4), distinguish between
   * receiver and transmitter
   * Transmission semaphores are stored in the second half
   * of the terminal semaphore range.
   */
  if (lineIndex == 4) {
    int subDevOffset = a1 - devRegBase;
    if (subDevOffset == 0xC) {
      semIndex += DEVPERINT;
    }
  }

  int *semAdd = &device_semaphores[semIndex];

    copyState(&currProc->p_s, excState);
  cpu_t currentTime;
  STCK(currentTime);
  currProc->p_time += (currentTime - tod_start);

  softBlock_count++;

  insertBlocked(semAdd, currProc);

  currProc = NULL;
  *((int *)a1) = a2;

  dispatch();

  return;
}

/*
 * NSYS6 — GetCPUTime
 *
 * Returns the total accumulated CPU time used by the current process,
 * including time elapsed during the current time quantum.
 */
void nsys6(state_t *excState) {
  cpu_t currentTime;
  STCK(currentTime);

  excState->reg_a0 = currProc->p_time + (currentTime - tod_start);
}

/*
 * NSYS7 — WaitForClock
 *
 * Blocks the current process until the next pseudo-clock tick.
 * The pseudo-clock semaphore (device_semaphores[NSUPPSEM]) is V'ed
 * by the Interval Timer interrupt handler every 100 milliseconds.
 */
void nsys7(state_t *excState) {
  int *pseudoClockSem = &device_semaphores[NSUPPSEM];
  copyState(&currProc->p_s, excState);

  cpu_t currentTime;
  STCK(currentTime);
  currProc->p_time += (currentTime - tod_start);

  softBlock_count++;
  insertBlocked(pseudoClockSem, currProc);

  currProc = NULL;
  dispatch();
  return;
}

/*
 * NSYS8 — GetSupportPtr
 *
 * Returns a pointer to the current process's Support Structure.
 * If no Support Structure was assigned at creation time, returns NULL.
 */
void nsys8(state_t *excState) {
  excState->reg_a0 = (int)currProc->p_supportStruct;
  return;
}

/*
 * NSYS9 — GetProcessID
 *
 * Returns the PID of the current process or its parent.
 *   a1 == 0: return PID of the current process
 *   a1 != 0: return PID of the parent process (0 if no parent)
 */
void nsys9(int a1, state_t *excState) {
  if (a1 == 0) {
    excState->reg_a0 = currProc->p_pid;
  } else {
    if (currProc->p_parent == NULL)
      excState->reg_a0 = 0;
    else
      excState->reg_a0 = currProc->p_parent->p_pid;
  }
}

/*
 * NSYS10 — Yield
 *
 * Causes the current process to voluntarily relinquish the CPU.
 * The process is re-inserted into the ready queue
 * and dispatch() is called to schedule the next process.
 */
void nsys10(state_t *excState) {
  copyState(&currProc->p_s, excState);

  cpu_t currentTime;
  STCK(currentTime);
  currProc->p_time += (currentTime - tod_start);
  
  pcb_t * old_head = removeProcQ(&readyQueue);
  insertProcQ(&readyQueue, currProc);
  list_add(&(old_head->p_list), &readyQueue);
  currProc = NULL;
  dispatch();
  return;
}
