#include "../headers/sysSupport.h"
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/auxfun.h"
#include "../headers/syscalls.h"
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>

extern int swapSemaphore;
extern int holdingSwapMutex[UPROCMAX];
void generalExceptionHandler() {

    support_t *sup = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* Estrai ExcCode: bit 6:2 del registro cause */
    unsigned int cause = sup->sup_exceptState[GENERALEXCEPT].cause;
    unsigned int excCode = cause & CAUSE_EXCCODE_MASK;

    if (excCode == SYSEXCEPTION || excCode == 11)
        pos_syscallHandler(sup);
    else
        programTrapHandler(sup);
}

void programTrapHandler(support_t *sup) {
    sys2(sup);
}

void pos_syscallHandler(support_t *sup) {
    state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);
    int a0_numSys = excState->reg_a0;

    excState->pc_epc += WORDLEN;

    switch (a0_numSys) {
    case TERMINATE:
        sys2(sup);
        break;
    case WRITETERMINAL:
        sys4(sup);
        break;
    case READTERMINAL:
        sys5(sup);
        break;
    case EXECUTE:
        sys6(sup);
        break;
    default:
        sys2(sup);
        break;
    }
}
