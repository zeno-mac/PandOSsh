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

/* Istante TOD in cui il processo corrente ha iniziato il suo quanto.
 * Viene impostato in dispatch() ogni volta che un processo viene eseguito. */
extern cpu_t tod_start;

extern struct list_head readyQueue;
extern int device_semaphores[NRSEMAPHORES];
extern pcb_t *currProc;
extern int softBlock_count;
extern int processCount;

extern struct list_head semd_h;

static int isDeviceSemaphore(int *semAdd) {
  return (semAdd >= &device_semaphores[0] &&
          semAdd <= &device_semaphores[NRSEMAPHORES - 1]);
}

static void terminateProcess(pcb_t *p) {
  if (p == NULL)
    return;

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
      softBlock_count--; // in attesa di io o di un clock
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
 * findPcbByPid
 * Cerca il PCB con il dato pid in tutte le strutture del sistema:
 *   1. Albero radicato nel processo corrente (running)
 *   2. Alberi radicati nei processi nella ready queue
 * Restituisce il puntatore al PCB trovato, oppure NULL.
 * Usata da NSYS2 quando pid != 0.
 */
static pcb_t *findPcbByPid(int pid) {

  /* cerca tra i figli del processo corrente */
  if (currProc != NULL) {
    pcb_t *found = searchInTree(currProc, pid);
    if (found != NULL)
      return found;
  }

  /* cerca nella ready queue */
  struct list_head *pos;
  list_for_each(pos, &readyQueue) {
    pcb_t *p = container_of(pos, pcb_t, p_list);
    pcb_t *found = searchInTree(p, pid);
    if (found != NULL)
      return found;
  }

  /* cerca tra i processi bloccati nell'ASL */
  pcb_t *found = searchBlockedByPid(pid);
  if (found != NULL)
    return found;

  return NULL;
}

//------------------------------------------------------------------

int nsys1(int a1, int a2, int a3) {
  state_t *newState = (state_t *)a1;
  int prio = a2;
  support_t *supportp = (support_t *)a3;

  pcb_t *new_pcb = allocPcb();
  if (new_pcb == NULL)
    return NOPROC; // NOPROC=-1
  else {
    // new_pcb->p_s = *newState; genera memcpy errors
    new_pcb->p_s.cause = newState->cause;
    new_pcb->p_s.entry_hi = newState->entry_hi;
    new_pcb->p_s.mie = newState->mie;
    new_pcb->p_s.pc_epc = newState->pc_epc;
    new_pcb->p_s.status = newState->status;
    for (int i = 0; i < STATE_GPR_LEN; i++)
      new_pcb->p_s.gpr[i] = newState->gpr[i];

    new_pcb->p_prio = prio;
    new_pcb->p_supportStruct = supportp;
    insertProcQ(&readyQueue, new_pcb);
    insertChild(currProc, new_pcb);
    processCount++;
    new_pcb->p_time = 0;
    new_pcb->p_semAdd = NULL;
    return new_pcb->p_pid;
  }
}

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

void nsys3(int a1, state_t *excState) {
  int *semAdd = (int *)a1;
  if (*semAdd > 0) {
    (*semAdd)--;
    LDST(excState);
  } else {

    currProc->p_s.entry_hi = excState->entry_hi;
    currProc->p_s.cause = excState->cause;
    currProc->p_s.mie = excState->mie;
    currProc->p_s.pc_epc = excState->pc_epc;
    currProc->p_s.status = excState->status;
    for (int i = 0; i < STATE_GPR_LEN; i++)
      currProc->p_s.gpr[i] = excState->gpr[i];

    cpu_t currentTime;
    STCK(currentTime);
    currProc->p_time += (currentTime - tod_start);
    insertBlocked(semAdd, currProc);

    currProc = NULL;
    dispatch();
  }
  return;
}

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

void nsys5(int a1, int a2, state_t *excState) {

  /* Azzera gli ultimi 4 bit per ottenere la base del device register */
  int devRegBase = a1 & 0xFFFFFFF0;

  /* Offset dall'inizio dell'area device register */
  int offset = devRegBase - START_DEVREG;

  /* Indice di linea (0=DISK, 1=FLASH, 2=ETH, 3=PRINTER, 4=TERMINAL) */
  int lineIndex = offset / 0x80;

  /* Numero del device sulla linea (0..7) */
  int devNo = (offset % 0x80) / 0x10;

  /* Indice base nel array device_semaphores */
  int semIndex = lineIndex * DEVPERINT + devNo;

  /* Per i terminali (lineIndex == 4) distingui recv da transm:
   * TRANSM_COMMAND è a offset 0xC dalla base del device register,
   * RECV_COMMAND è a offset 0x4.
   * Se a1 - devRegBase == 0xC → trasmissione → semaforo nella
   * seconda metà dell'area terminali */
  if (lineIndex == 4) {
    int subDevOffset = a1 - devRegBase;
    if (subDevOffset == 0xC) {
      /* Trasmissione: usa il semaforo con offset DEVPERINT */
      semIndex += DEVPERINT;
    }
    /* Ricezione: semIndex rimane invariato */
  }

  int *semAdd = &device_semaphores[semIndex];

  /* --- 2. Scrivi il comando nel registro del dispositivo --- */
  /* Questo avvia fisicamente l'operazione I/O */
  *((int *)a1) = a2;

  /* --- 3. Salva lo stato corrente nel PCB --- */
  currProc->p_s.entry_hi = excState->entry_hi;
  currProc->p_s.cause = excState->cause;
  currProc->p_s.mie = excState->mie;
  currProc->p_s.pc_epc = excState->pc_epc;
  currProc->p_s.status = excState->status;
  for (int i = 0; i < STATE_GPR_LEN; i++)
    currProc->p_s.gpr[i] = excState->gpr[i];

  /* --- 4. Aggiorna il tempo accumulato --- */
  cpu_t currentTime;
  STCK(currentTime);
  currProc->p_time += (currentTime - tod_start);

  /* --- 5. Incrementa softBlock_count ---
   * Questo processo è ora bloccato in attesa di I/O */
  softBlock_count++;

  /* --- 6. Blocca il processo sul semaforo del dispositivo --- */
  insertBlocked(semAdd, currProc);

  /* --- 7. Nessun processo in esecuzione --- */
  currProc = NULL;

  /* --- 8. Chiama lo scheduler --- */
  dispatch();

  return;
}

void nsys6(state_t *excState) {
  cpu_t currentTime;
  STCK(currentTime);

  excState->reg_a0 = currProc->p_time + (currentTime - tod_start);
}

void nsys7(state_t *excState) {
  /* Il semaforo pseudo-clock è sempre l'ultimo dell'array */
  int *pseudoClockSem = &device_semaphores[NSUPPSEM];

  /* 1. Salva lo stato corrente nel PCB */
  currProc->p_s.entry_hi = excState->entry_hi;
  currProc->p_s.cause = excState->cause;
  currProc->p_s.mie = excState->mie;
  currProc->p_s.pc_epc = excState->pc_epc;
  currProc->p_s.status = excState->status;
  for (int i = 0; i < STATE_GPR_LEN; i++)
    currProc->p_s.gpr[i] = excState->gpr[i];

  /* 2. Aggiorna il tempo accumulato */
  cpu_t currentTime;
  STCK(currentTime);
  currProc->p_time += (currentTime - tod_start);

  /* 3. Incrementa softBlock_count:
   * questo processo è bloccato in attesa di un evento timer,
   * non di un semaforo utente */
  softBlock_count++;

  /* 4. Blocca il processo sul semaforo pseudo-clock */
  insertBlocked(pseudoClockSem, currProc);

  /* 5. Nessun processo in esecuzione */
  currProc = NULL;

  /* 6. Chiama lo scheduler */
  dispatch();

  return;
}

void nsys8(state_t *excState) {
  excState->reg_a0 = (int)currProc->p_supportStruct;
  return;
}

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

void nsys10(state_t *excState) {

  currProc->p_s.entry_hi = excState->entry_hi;
  currProc->p_s.cause = excState->cause;
  currProc->p_s.mie = excState->mie;
  currProc->p_s.pc_epc = excState->pc_epc;
  currProc->p_s.status = excState->status;
  for (int i = 0; i < STATE_GPR_LEN; i++)
    currProc->p_s.gpr[i] = excState->gpr[i];

  cpu_t currentTime;
  STCK(currentTime);
  currProc->p_time += (currentTime - tod_start);

  insertProcQ(&readyQueue, currProc);

  currProc = NULL;

  dispatch();

  return;
}
