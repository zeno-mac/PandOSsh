#include "../headers/initial.h"
#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../headers/exceptions.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include <uriscv/liburiscv.h>
#include "../headers/scheduler.h"

struct list_head readyQueue; // List of processed ready to be execute
int processCount = 0; // Counter indicating the number of started, but not yet terminated processes
int softBlock_count = 0; // Counter indicating the number of process blocked
pcb_t* currProc = NULL; // Pointer to the pcb in "running" state
int device_semaphores[NRSEMAPHORES] = {0}; // Semaphores for each external device (24*2 +1)
//TODO Check if semaphores are correct this way

extern void uTLB_RefillHandler();
extern void test();

// Allocate, initiliaze and returns the first pcb
pcb_t* allocFirstPcb(){
    pcb_t* pcb = allocPcb();
    pcb->p_s.pc_epc = (memaddr)test;
    pcb->p_s.mie = MIE_ALL;
    pcb->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;
    pcb->p_parent = NULL;
    INIT_LIST_HEAD(&pcb->p_child);
    INIT_LIST_HEAD(&pcb->p_sib);
    pcb->p_time = 0;
    pcb->p_semAdd = NULL;
    pcb->p_supportStruct = NULL;
    RAMTOP(pcb->p_s.reg_sp);
    return pcb;
}

//Initialize passupvector and all its fields
void initVector(){
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;
    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->exception_handler = (memaddr)exceptionHandler;
    passupvector->tlb_refill_stackPtr = KERNELSTACK;
    passupvector->exception_stackPtr = KERNELSTACK;
}

int main(){
    mkEmptyProcQ(&readyQueue);
    initPcbs();
    initASL();
    LDIT(PSECOND);

    pcb_t* pcb = allocFirstPcb();
    insertProcQ(&readyQueue, pcb);
    processCount++;

    dispatch();
    return 0;
}
