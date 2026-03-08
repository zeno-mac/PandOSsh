#include "../headers/initial.h"
#include "../headers/scheduler.h"
#include "../headers/exceptions.h"
#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../headers/listx.h"

#include <uriscv/liburiscv.h>
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"

extern struct list_head readyQueue;


int nsys1(int a1, int a2, int a3){

  pcb_t *new_pcb = allocPcb();
  if(new_pcb == NULL) return -1;
  else {
    new_pcb->p_s = * (state_t *)a1;
    new_pcb->p_prio = a2;
    new_pcb->p_supportStruct = (support_t *)a3;
    //new_pcb->p_pid = ??
    new_pcb->p_next = insertProcQ(&readyQueue,new_pcb);
    new_pcb->p_child = insertChild(, new_pcb);
    new_pcb->p_time = 0;
    new_pcb->p_semAdd = NULL;



  }
}
