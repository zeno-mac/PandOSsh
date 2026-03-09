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

static int isDeviceSemaphore(int *semAdd) {
    return (semAdd >= &device_semaphores[0] &&
            semAdd <= &device_semaphores[NRSEMAPHORES - 1]);
}

static void terminateProcess(pcb_t *p) {
    if (p == NULL) return;

    /*
     * Termina ricorsivamente tutti i figli prima di terminare p.
     * Usiamo removeChild() in loop perche' terminateProcess() modifica
     * la lista p_child durante la ricorsione.
     */
    while (!emptyChild(p)) {
        terminateProcess(removeChild(p));
    }

    /* Rimuovi p dalla struttura in cui si trova attualmente */
    if (p == currProc) {
        /* Processo in esecuzione: non e' in nessuna lista */
        currProc = NULL;

    } else if (p->p_semAdd != NULL) {
        /* Processo bloccato su un semaforo nell'ASL */
        outBlocked(p);

        /*
         * Decrementa softBlock_count solo se il semaforo e' uno dei
         * semafori device o pseudo-clock (non un semaforo utente).
         */
        if (isDeviceSemaphore(p->p_semAdd)) {
            softBlock_count--;//in attesa di io o di un clock
        }

    } else {
        /* Processo pronto: si trova nella ready queue */
        outProcQ(&readyQueue, p);
    }

    /* Stacca p dall'albero del padre (se ne ha ancora uno) */
    outChild(p);

    processCount--;
    freePcb(p);
}

/*
 * searchInTree
 * Cerca ricorsivamente il PCB con il dato pid nell'albero radicato in root.
 * Restituisce il puntatore al PCB trovato, oppure NULL.
 */
static pcb_t *searchInTree(pcb_t *root, int pid) {
    if (root == NULL) return NULL;
    if (root->p_pid == pid) return root;

    struct list_head *pos;
    list_for_each(pos, &root->p_child) {
        pcb_t *child = container_of(pos, pcb_t, p_sib);
        pcb_t *found = searchInTree(child, pid);
        if (found != NULL) return found;
    }
    return NULL;
}

/*
 * findPcbByPid
 * Cerca il PCB con il dato pid in tutte le strutture del sistema:
 *   1. Albero radicato nel processo corrente (running)
 *   2. Alberi radicati nei processi nella ready queue
 * Restituisce il puntatore al PCB trovato, oppure NULL.
 * Usata da NSYS2 quando pid != 0.
 */
static pcb_t *findPcbByPid(int pid) {

  //cerca tra i figli del processo corrente
  if (currProc != NULL) {
    pcb_t *found = searchInTree(currProc, pid);
    if (found != NULL) return found;
  }
  
  //cerca nella ready Queue 
  struct list_head *pos;
  list_for_each(pos, &readyQueue) {
    pcb_t *pcb    = container_of(pos, pcb_t, p_list);
    pcb_t *found = searchInTree(pcb, pid);
    if (found != NULL) return found;
  }
  //cerca tra processi in stato waiting (tramite semafori in asl)
  //searchBlockedByPid(pid);

  return NULL;
}

    

//-------------------------------------------------------------------
// da aggiungere in asl.c
// in asl.c - ha accesso a semd_h che è privata
pcb_t *searchBlockedByPid(int pid) {
    semd_t *sem;
    list_for_each_entry(sem, &semd_h, s_link) {
        struct list_head *pos;
        list_for_each(pos, &sem->s_procq) {
            pcb_t *p = container_of(pos, pcb_t, p_list);
            if (p->p_pid == pid) return p;
        }
    }
    return NULL;
}
//------------------------------------------------------------------







int nsys1(int a1, int a2, int a3){
  state_t *newState = (state_t*)a1;
  int prio = a2;
  support_t *supportp = (support_t*)a3;

  pcb_t *new_pcb = allocPcb();
  if(new_pcb == NULL) return NOPROC; //NOPROC=-1
  else {
    new_pcb->p_s = *newState;
    new_pcb->p_prio = prio;
    new_pcb->p_supportStruct = supportp;
    new_pcb->p_next = insertProcQ(&readyQueue,new_pcb);
    new_pcb->p_child = insertChild(currProc, new_pcb);
    processCount++;
    new_pcb->p_time = 0;
    new_pcb->p_semAdd = NULL;
    return new_pcb->p_pid;
  }
}



void nsys2(int a1, state_t * excState) {
  pcb_t *termPcb = NULL;

  if(a1 == 0){
    termPcb = currProc;
  }
  else {
    termPcb = findPcbByPid(Pid);
    if(termPcb == NULL) {
      excState->reg_a0 = NOPROC;
      LDST(excState);
      return;
    }
  }

  terminateProcess(termPcb);
  dispatch();
}



