#include "../headers/interrupts.h"
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase1/headers/asl.h"
#include "../../phase1/headers/pcb.h"
#include "../headers/initial.h"
#include "../headers/scheduler.h"
#include <uriscv/cpu.h> 
#include <uriscv/liburiscv.h>
#include "../headers/auxfun.h"


// Import global vars from initial.c
extern struct list_head readyQueue;
extern int softBlock_count;
extern pcb_t *currProc;
extern int device_semaphores[NRSEMAPHORES];
extern cpu_t tod_start;

/*
 * Handles Processor Local Timer (PLT) interrupts.
 *
 * Acknowledges the interrupt by reloading the PLT with a 5ms TIMESLICE.
 *
 * Updates currProc's accumulated CPU time, saves its exception state
 * into its PCB, and inserts it back into the readyQueue.
 *
 * Calls the scheduler's dispatch() procedure to schedule the next process.
 */
void pltInterruptHandler() {
  setTIMER(TIMESLICE); // Let 5ms pass on the processor
  cpu_t currentTime;
  STCK(currentTime);
  currProc->p_time += (currentTime - tod_start);

  if (currProc) {
    state_t *excState = getCurrExceptionState(); // Copy the processor state at the time of the interrupt
    copyState(&currProc->p_s, excState);

    insertProcQ(&readyQueue, currProc); // Place currProc in the ready queue
  }

  dispatch();
}

/*
 * Handles Interval Timer (IT) pseudo-clock interrupts.
 *
 * Acknowledges the interrupt by reloading the IT with 100ms (PSECOND).
 *
 * Unlocks all processes waiting on the pseudo-clock semaphore, inserts them
 * into the readyQueue, decrements softBlock_count for each of them, and resets the semaphore.
 *
 * Saves currProc's state and re-queues, then calls dispatch().
 */
void itInterruptHandler() {
  LDIT(PSECOND); // Interrupt acknowledgemnet by loading the Interval Timer with 100ms

  int *pseudoClockSem = &device_semaphores[NSUPPSEM]; // Get the last semaphore
  pcb_t *unblockedPcb;

  while ((unblockedPcb = removeBlocked(pseudoClockSem))) { // Unlock all the procs waiting for a tick
    insertProcQ(&readyQueue, unblockedPcb);
    softBlock_count--;
  }

  *pseudoClockSem = 0;

  // Returning the control to the curr proc. if that exists. Identical to the pltInterruptHandler
  if (currProc) {
    state_t *excState = getCurrExceptionState(); // Loading the state on the curr proc.
    copyState(&currProc->p_s, excState);
    
    insertProcQ(&readyQueue, currProc);
  }

  dispatch();
}


/*
 * Handles external hardware device interrupts (Lines 3-7).
 *
 * Identifies the specific line and device using bitmaps, reads its status,
 * and writes the ACK command.
 *
 * Unblocks a waiting process (saving status in a0, decreasing softBlock_count)
 * or increments the Nucleus device semaphore if there's no one waiting.
 *
 * Either resumes currProc using LDST or calls dispatch() if the CPU was idle.
 */
void deviceInterruptHandler(int excCode) {

  int DevNo = -999;
  int IntLineNo = -999;

  switch (excCode) {
  case IL_DISK:
    IntLineNo = 3;
    break;
  case IL_FLASH:
    IntLineNo = 4;
    break;
  case IL_ETHERNET:
    IntLineNo = 5;
    break;
  case IL_PRINTER:
    IntLineNo = 6;
    break;
  case IL_TERMINAL:
    IntLineNo = 7;
    break;
  default:
    return;
  }

  int mapAddr = 0x10000040 + ((IntLineNo - 3) * WORDLEN); // startingAddress+(normilized line of 2nd table)*lengthOfWord
  int bitmap = *((int *)mapAddr); // Saves in bitmap the value of the map pointed by the address

  // Let's find which device actually triggered the interrupt
  if (bitmap & DEV0ON)
    DevNo = 0;
  else if (bitmap & DEV1ON)
    DevNo = 1;
  else if (bitmap & DEV2ON)
    DevNo = 2;
  else if (bitmap & DEV3ON)
    DevNo = 3;
  else if (bitmap & DEV4ON)
    DevNo = 4;
  else if (bitmap & DEV5ON)
    DevNo = 5;
  else if (bitmap & DEV6ON)
    DevNo = 6;
  else if (bitmap & DEV7ON)
    DevNo = 7;

  if (DevNo == -999)
    return;

  int devAddr = START_DEVREG + ((IntLineNo - 3) * 0x80) + (DevNo * 0x10); // startingAddress+(lineOffset)+(deviceOffset)
  int semaphoreIdx = (IntLineNo - 3) * DEVPERINT + DevNo; 
  // To switch from matrix form to array you get the deviceLine and multiply it by 8(since there's 8 devices for each line) and the add the device number
  int status;

  if (IntLineNo == 7) { // Check to see if this interrupt is generated from a terminal "sender"
    int transmissionStatus = *((int *)(devAddr + 0x80)); // Reads the transmission status register using the offset of 0x8
    if ((transmissionStatus & 0xFF) !=0) { // If the transimmion register contains a value, hence the transmission is completed
      status = transmissionStatus;
      *((int *)(devAddr + 0xC)) = ACK; // ACKs the end of this transmission
    } 
    else {   // The interrupts comes from the reciever
      status = *((int *)(devAddr + 0x0)); // Reads the reciever status
      *((int *)(devAddr + 0x4)) = ACK;
      semaphoreIdx += DEVPERINT; // Offsets the semaphore to the recieving semaphore index
    }
  }

  // Remove first process waiting for that semaphore, we are performing a sem.V()
  int *deviceSem = &device_semaphores[semaphoreIdx];

  pcb_t *unblockedPcb = removeBlocked(deviceSem);

  if (unblockedPcb) {
    unblockedPcb->p_s.gpr[24] = status; // gpr[24] maps the a0 register that is  used for syscall return values
    insertProcQ(&readyQueue, unblockedPcb);
    softBlock_count--;

    // Saves the currProc state and puts him in the queue since there is another process(the unblocked one) who should be starting as we call the scheduler with dispatch()
    if (currProc) {
      LDST(getCurrExceptionState());
    } else {
      dispatch();
    }

  } else {
    (*deviceSem)++; // Increase the semaphore value if there were no process waiting
  }
}


/*
 * Wrapper function that works as the entry point for the interrupt exceptions. 
 *
 * Identifies the specific interrupt type:
 * extracts the pending interrupt bits from the exception state cause register.
 *
 * Calls pltInterruptHandler(), itInterruptHandler() or deviceInterruptHandler() respectively for the interrupts caused by:
 * - Processor Local Timer, 
 * - Interval Timer,
 * - External Devices.
 */
void interruptHandler() {
  state_t *excState = getCurrExceptionState();
  int excCause = excState->cause;
  int excCode = excCause & CAUSE_EXCCODE_MASK; 

  if (excCode == IL_CPUTIMER) {
    // Process Local Timer
    pltInterruptHandler();
  } else if (excCode == IL_TIMER) {
    // Interval Timer
    itInterruptHandler();
  } else if (excCode >= IL_DISK && excCode <= IL_TERMINAL) {
    // exceptions 17 to 21 for Devices
    deviceInterruptHandler(excCode);
  }
}
