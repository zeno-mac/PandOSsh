#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/auxfun.h"
#include <uriscv/liburiscv.h>

void generalExceptionHandler() {}

void pos_syscallHandler() {
  state_t *excState = getCurrExceptionState();
  int a0_numSys = excState->reg_a0;
  int a1 = excState->reg_a1;
  int a2 = excState->reg_a2;
  int a3 = excState->reg_a3;

  // #TODO: increment pc

  switch (a0_numSys) {
  case TERMINATE:
    break;
  case WRITETERMINAL:
    break;
  case READTERMINAL:
    break;
  case EXECUTE:
    break;
  }
}
