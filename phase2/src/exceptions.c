#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../headers/initial.h"
#include "../headers/interrupts.h"
#include "../headers/scheduler.h"
#include "../headers/syscall.h"

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>

#include "../headers/auxfun.h"

extern pcb_t *currProc;

// The the TLB_Refill is called when a logical address translation’s search of the of the TLB for a matching entry fails
// The TLB-Refill inserts the missing Page Table entry and restart the instruction
void uTLB_RefillHandler(void) {

    state_t *saved_state = (state_t *)BIOSDATAPAGE;

    unsigned int missingVPN = (saved_state->entry_hi & GETPAGENO) >> VPNSHIFT;

    pteEntry_t *pageTable = currProc->p_supportStruct->sup_privatePgTbl;
    int pageIndex = missingVPN % MAXPAGES;

    setENTRYHI(pageTable[pageIndex].pte_entryHI);
    setENTRYLO(pageTable[pageIndex].pte_entryLO);
    TLBWR();

    LDST(saved_state);
}


/*
 * Entry point for all exceptions (excluding TLB-Refill events).
 * The processor state at the time of the exception is saved by the BIOS
 * at the start of the BIOS Data Page (BIOSDATAPAGE).
 *
 * Dispatches to the appropriate handler based on the exception cause:
 *   - Interrupt         → interruptHandler()
 *   - TLB exception     → exc_tlbHandler()      (codes 24-28)
 *   - SYSCALL           → syscallHandler()       (codes 8, 11)
 *   - Program Trap      → exc_trapHandler()      (all other codes)
 */

void exceptionHandler() {
  state_t *excState = getCurrExceptionState();
  int excCode = excState->cause;
  /* Check if the exception is an interrupt (bit 31 of cause is set) */
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
/*
 * Implements the "Pass Up or Die" mechanism for TLB and Program Trap
exceptions.
 *
 * If the current process has no Support Structure (p_supportStruct == NULL):
 *    "Die": terminate the current process and all its progeny, then call
 *    dispatch().
 *
 * If the current process has a Support Structure:
 *    "Pass Up": copy the saved exception state from the BIOS Data Page into
 *    the appropriate sup_exceptState field of the Support Structure, then
 *    transfer control to the Support Level handler via LDCXT.
*/
void passUpOrDie(int exceptionType) {

  if (currProc->p_supportStruct == NULL) { // Die
    nsys2(0, getCurrExceptionState());

  } else { // Pass Up
    state_t *excState = getCurrExceptionState();

    copyState(&currProc->p_supportStruct->sup_exceptState[exceptionType],
              excState);

    /* Retrieve the Support Level handler context (SP, status, PC) */
    context_t *ctx =
        &(currProc->p_supportStruct->sup_exceptContext[exceptionType]);

    /* Transfer control to the Support Level handler. */
    LDCXT(ctx->stackPtr, ctx->status, ctx->pc);
  }
}

/*
 * Handles TLB exceptions (exception codes 24-28).
 * Performs Pass Up or Die using the PGFAULTEXCEPT index.
 */
void exc_tlbHandler() {
  passUpOrDie(PGFAULTEXCEPT);
  return;
}

/*
 * Handles Program Trap exceptions (codes 0-7, 9, 10, 12-23)
 * and unrecognized/positive SYSCALLs.
 * Performs Pass Up or Die using the GENERALEXCEPT index.
 */
void exc_trapHandler() {
  passUpOrDie(GENERALEXCEPT);
  return;
}

/*
 * Handles SYSCALL exceptions (exception codes 8 and 11).
 *
 * If a negative syscall is requested from user-mode (MPP == 0):
 *    simulate a Program Trap by setting cause to PRIVINSTR and
 *    calling exc_trapHandler().
 *
 * For valid Nucleus syscalls (a0 in [-10, -1]):
 *    increment PC by 4 to avoid infinite SYSCALL loop,
 *    then dispatch to the appropriate nsysN() function.
 *
 * For unrecognized syscall numbers (a0 >= 1 or a0 < -10):
 *    Pass Up or Die via exc_trapHandler().
 */
void syscallHandler() {
  state_t *excState = getCurrExceptionState();
  int a0_numSys = excState->reg_a0;
  int a1 = excState->reg_a1;
  int a2 = excState->reg_a2;
  int a3 = excState->reg_a3;

  // cause a trap if the syscall is requested in user mode

  if (a0_numSys < 0 && (MSTATUS_MPP_MASK & excState->status) == MSTATUS_MPP_U) {
    excState->cause =
        (excState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
    exc_trapHandler();
    return;

  } else {

    /* Increment pc only for negative syscalls (-10 to -1) */
    if (a0_numSys >= -10 && a0_numSys <= -1) {
      excState->pc_epc += WORDLEN;
    }

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
    case GETPROCESSID:
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
