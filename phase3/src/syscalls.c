#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/src/auxfun.c"
#include "../headers/initProc.h"
#include <uriscv/liburiscv.h>

#define UPROC_PRIORITY PROCESS_PRIO_LOW // TODO: define priority properly

extern int masterSemaphore;
extern int shellSemaphore;

// TERMINATE
void sys2(support_t *sup) {
  if (sup->sup_asid == SHELL_ASID) {
    SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
  } else {
    SYSCALL(VERHOGEN, (int)&shellSemaphore, 0, 0);
  }

  SYSCALL(TERMPROCESS, 0, 0, 0);
}

// EXECUTE
void sys6(support_t *sup) {
  state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);

  int asid = excState->reg_a1;

  /*
   * In questa fase semplificata solo la shell può chiamare EXECUTE.
   */
  if (sup->sup_asid != SHELL_ASID) {
    sys2(sup);
  }

  /*
   * ASID validi: 1..UPROCMAX.
   * Però ASID 1 è la shell, quindi non la lasciamo rieseguire tramite SYS6.
   */
  if (asid <= 0 || asid > UPROCMAX || asid == SHELL_ASID) {
    sys2(sup);
  }

  /*
   * La support_t deve essere globale/static,
   * perché il PCB conserverà un puntatore a questa struttura.
   */
  support_t *childSupport = &supportPool[asid - 1];

  /*
   * childState può essere locale perché CREATEPROCESS copia subito
   * lo stato dentro il PCB, come fa la tua nsys1 con copyState().
   */
  state_t childState;

  initUProc(asid, &childState, childSupport);

  int pid = SYSCALL(CREATEPROCESS, (int)&childState, UPROC_PRIORITY,
                    (int)childSupport);

  /*
   * Qui blocchi la shell.
   * shellSemaphore parte da 0.
   * Verrà sbloccata quando il figlio termina e SYS2 fa V(shellSemaphore).
   */
  SYSCALL(PASSEREN, (int)&shellSemaphore, 0, 0);
}
