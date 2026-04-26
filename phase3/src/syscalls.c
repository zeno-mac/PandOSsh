#include "../headers/syscalls.h"
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/auxfun.h"
#include "../headers/initProc.h"
#include <uriscv/liburiscv.h>

#define UPROC_PRIORITY PROCESS_PRIO_LOW // TODO: understand priority properly

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

  if (sup->sup_asid != SHELL_ASID) {
    sys2(sup);
  }

  if (asid <= 0 || asid > UPROCMAX || asid == SHELL_ASID) {
    sys2(sup);
  }

  int pid = createUProc(asid);

  if (pid == NOPROC) {
    sys2(sup);
  }

  SYSCALL(PASSEREN, (int)&shellSemaphore, 0, 0);

  LDST(excState);
}
