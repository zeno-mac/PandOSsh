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

void dispatch(){
    while(emptyProcQ(&readyQueue)){
        if (processCount == 0){
            HALT();
        }
        else if(softBlock_count > 0){
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);
            WAIT();
        }
        else{
            PANIC();
        }
    }
    pcb_t* pcb = removeProcQ(&readyQueue);
    currProc = pcb;
    setTIMER(TIMESLICE);
    LDST(&(pcb->p_s));
}