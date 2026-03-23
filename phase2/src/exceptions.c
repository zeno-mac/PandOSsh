#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"
#include "../headers/initial.h"
#include "../headers/interrupts.h"
#include "../headers/scheduler.h"
#include "../headers/syscall.h"

// File della libreria uriscv da includere -------------------------
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>

#include "../../phase1/headers/asl.h"
#include "../../phase1/headers/pcb.h"

extern pcb_t *currProc;

extern void uTLB_RefillHandler();

void exceptionHandler() {
  state_t *excState = getCurrExceptionState();
  int excCode = excState->cause;

  if (CAUSE_IS_INT(excCode)) {
    interruptHandler();
    return;
  }
  int excCodeMask = excCode & CAUSE_EXCCODE_MASK;
  if (excCodeMask >= 24 && excCodeMask <= 28) {
    exc_tlbHandler();
  } else if (excCodeMask == 8 || excCodeMask == 11) {
    syscallHandler();
  } else {
    exc_trapHandler();
  }
}

static void passUpOrDie(int exceptionType) {

  if (currProc->p_supportStruct == NULL) {
    /* Die: termina il processo corrente e tutti i suoi figli */
    nsys2(0, getCurrExceptionState());
    /* nsys2 chiama dispatch() internamente, non torna mai qui */
  } else {
    /* Pass Up */
    state_t *excState = getCurrExceptionState();

    /* 1. Copia lo stato salvato nel campo corretto del support (Campo per
     * Campo!) */
    currProc->p_supportStruct->sup_exceptState[exceptionType].entry_hi =
        excState->entry_hi;
    currProc->p_supportStruct->sup_exceptState[exceptionType].cause =
        excState->cause;
    currProc->p_supportStruct->sup_exceptState[exceptionType].status =
        excState->status;
    currProc->p_supportStruct->sup_exceptState[exceptionType].pc_epc =
        excState->pc_epc;
    currProc->p_supportStruct->sup_exceptState[exceptionType].mie =
        excState->mie;
    for (int i = 0; i < STATE_GPR_LEN; i++) {
      currProc->p_supportStruct->sup_exceptState[exceptionType].gpr[i] =
          excState->gpr[i];
    }

    /* 2. Recupera il contesto del Support Level handler */
    context_t *ctx =
        &(currProc->p_supportStruct->sup_exceptContext[exceptionType]);

    /* 3. Passa il controllo al Support Level handler. */
    LDCXT(ctx->stackPtr, ctx->status, ctx->pc);
  }
}
void exc_tlbHandler() {
  passUpOrDie(PGFAULTEXCEPT);
  return;
}

void exc_trapHandler() {
  passUpOrDie(GENERALEXCEPT);
  return;
}

void syscallHandler() {
  state_t *excState = getCurrExceptionState();
  int a0_numSys = excState->reg_a0;
  int a1 = excState->reg_a1;
  int a2 = excState->reg_a2;
  int a3 = excState->reg_a3;

  if (a0_numSys < 0 && (MSTATUS_MPP_MASK & excState->status) == MSTATUS_MPP_U) {
    excState->cause =
        (excState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
    exc_trapHandler();
    return;
  } else {

    excState->pc_epc += WORDLEN;

    switch (a0_numSys) {
    case CREATEPROCESS:
      excState->reg_a0 = nsys1(a1, a2, a3);
      LDST(excState);
      break;
    case TERMPROCESS:
      nsys2(a1, excState);
      break;
    case PASSEREN:
      nsys3(a1, excState);
      break;
    case VERHOGEN:
      nsys4(a1, excState);
      LDST(excState);
      break;
    case DOIO:
      nsys5(a1, a2, excState);
      break;
    case GETTIME:
      nsys6(excState);
      LDST(excState);
      break;
    case CLOCKWAIT:
      nsys7(excState);
      break;
    case GETSUPPORTPTR:
      nsys8(excState);
      LDST(excState);
      break;
    case GETPROCESSID: // GETPID dalle spec. sezione 6.9 dovrebbe valere -9.
                       // Facendo un $grep di GETPID nella libreria che ha mazzo
                       // non c'è niente, forse bisogna usare GETPROCESSID
                       // definito in  "../headers/const.h"
      nsys9(a1, excState);
      LDST(excState);
      break;
    case YIELD:
      nsys10(excState);
      break;
    default:
      exc_trapHandler();
      break;
    }
  }
}
