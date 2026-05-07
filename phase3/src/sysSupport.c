#include "../headers/sysSupport.h"
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/auxfun.h"
#include "../headers/syscalls.h"
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
extern void klog_print(char *str);
extern void klog_print_hex(unsigned int num);
extern void klog_print_dec(unsigned int num);
/* ==========================================================================
 * VARIABILI ESTERNE — vmSupport.c (Persona 1/2)
 * ========================================================================== */
extern int swapSemaphore;
extern int holdingSwapMutex[UPROCMAX]; // andrà aggiunto che serve per capire se il processo sta tenendo il mutex del swap pool, in modo da rilasciarlo in caso di terminazione forzata

void generalExceptionHandler() {

    support_t *sup = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* Estrai ExcCode: bit 6:2 del registro cause */
    unsigned int cause = sup->sup_exceptState[GENERALEXCEPT].cause;
    unsigned int excCode = cause & CAUSE_EXCCODE_MASK;

    klog_print("GENERAL EXCEPTION\n");
    klog_print("cause=");
    klog_print_hex(cause);
    klog_print(" excCode=");
    klog_print_dec(excCode);
    klog_print("\n");
    if (excCode == SYSEXCEPTION || excCode == 11) // non so se serve anche cause=11
        pos_syscallHandler(sup);
    else
        programTrapHandler(sup);
}

void programTrapHandler(support_t *sup) {
    state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);

    klog_print("PROGRAM TRAP\n");
    klog_print("trap cause=");
    klog_print_hex(excState->cause);
    klog_print("\n");

    klog_print("trap pc=");
    klog_print_hex(excState->pc_epc);
    klog_print("\n");

    klog_print("trap entryhi=");
    klog_print_hex(excState->entry_hi);
    klog_print("\n");

    sys2(sup);
}

void pos_syscallHandler(support_t *sup) {
    state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);
    int a0_numSys = excState->reg_a0;
    /*int a1 = excState->reg_a1;
    int a2 = excState->reg_a2;
    int a3 = excState->reg_a3;*/
    klog_print("POS SYSCALL a0=");
    klog_print_dec(a0_numSys);
    klog_print("\n");
    // #TODO: increment pc
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
