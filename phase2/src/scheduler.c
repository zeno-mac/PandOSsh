#include "../headers/initial.h"
#include "../headers/scheduler.h"
#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../headers/listx.h"
#include <uriscv/liburiscv.h>
#include "../../phase1/headers/pcb.h"

extern struct list_head readyQueue;
extern int processCount;
extern int softBlock_count;
extern pcb_t* currProc;
extern cpu_t tod_start;

void dispatch(){
    while(emptyProcQ(&readyQueue)){
        if (processCount == 0){ 
            HALT();
        }
        else if(softBlock_count > 0){ //There are no ready process but some are waiting
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);
            WAIT();
        }
        else{ //Fallback case when a deadlock is happening
            PANIC();
        }
    }
    pcb_t* pcb = removeProcQ(&readyQueue);
    currProc = pcb;
    setTIMER(TIMESLICE); //Load 5 milliseconds on the PLT
    STCK(tod_start);
    LDST(&(pcb->p_s)); //Perform a Load Processor State on the processor state stored in PCB of the Current Process of the current CPU
}
