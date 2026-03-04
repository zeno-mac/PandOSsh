#include "../../headers/const.h"
#include "../../headers/types.h"
#include <uriscv/liburiscv.h>
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "../headers/initial.h"
#include "../headers/scheduler.h"

//----------------------TO AVOID MEMCPY ERRORS--------------------
void *memcpy(void *dest, const void *src, unsigned int n) {
    char *d = dest;
    const char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}


//Import global vars from initial.c
extern struct list_head readyQueue; 
//extern int processCount; 
extern int softBlock_count; 
extern pcb_t* currProc; 
extern int device_semaphores[NRSEMAPHORES]; 

// Aux function to get the exception state of the curr processor
state_t * getCurrExceptionState(){
    return GET_EXCEPTION_STATE_PTR(getPRID());
}



void pltInterruptHandler(){
    setTIMER(TIMESLICE); //let 5ms pass on the processor: 
    if(currProc){
        state_t * excState=getCurrExceptionState(); //Copy the processor state at the time of the interrupt
        currProc->p_s=*excState; 

        insertProcQ(&readyQueue, currProc); //Place the curr process on the ready queue
    }

    dispatch(); 
}


void itInterruptHandler(){
    LDIT(PSECOND); //Acknowledging the interrupt by loading the Interval Timer with 100ms.

    int * pseudoClockSem=&device_semaphores[NSUPPSEM]; //Get the last semaphore
    pcb_t * unblockedPcb;

    while((unblockedPcb=removeBlocked(pseudoClockSem))){ //Unlock all the procs waiting for a tick
        insertProcQ(&readyQueue, unblockedPcb);
        softBlock_count--;
    }

    *pseudoClockSem=0;

    //Returning the control to the curr proc. if that exists. Identical to the pltInterruptHandler
    if(currProc){
        state_t *excState=getCurrExceptionState(); // Loading the state on the current proc.
        currProc->p_s=*excState;
        insertProcQ(&readyQueue, currProc);        
    }

    dispatch();
}



void interruptHandler(){
    state_t * excState=getCurrExceptionState();
    int excCause=excState->cause;
    int excCode=(excCause & GETEXECCODE) >> CAUSESHIFT;

    if(excCode==IL_CPUTIMER){
        //Process Local Timer
        pltInterruptHandler();
    }
    else if(excCode==IL_TIMER){
        //Interval Timer
        itInterruptHandler();
    }
    else if(excCode>=IL_DISK && excCode<=IL_TERMINAL){
        //exceptions 17 to 21 for Devices
        //deviceInterruptHandler(excCode);
    }
}