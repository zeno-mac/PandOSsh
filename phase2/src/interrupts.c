#include "../../headers/const.h"
#include "../../headers/types.h"
#include <uriscv/liburiscv.h>
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "../headers/initial.h"
#include "../headers/scheduler.h"

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
        itInterruptHndler();
    }
    else if(excCode>=IL_DISK && excCode<=IL_TERMINAL){
        //exceptions 17 to 21 for Devices
        deviceInterruptHandler(excCode);
    }
}

