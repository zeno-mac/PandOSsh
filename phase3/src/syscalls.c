#include "../headers/syscalls.h"
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/auxfun.h"
#include "../headers/initProc.h"
#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#define UPROC_PRIORITY PROCESS_PRIO_LOW
extern int masterSemaphore;
extern int shellSemaphore;
extern int holdingSwapMutex[UPROCMAX];
extern int swapSemaphore;
extern int termWriteSemaphore;
extern int termReadSemaphore;
extern swap_t swapPool[];

static int writeTerminal(char *str, int len) {
    termreg_t *term = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, 0);
    int transmitted = 0;

    /* Spec 7.2: "suspended until a line of output has been transmitted" */
    SYSCALL(PASSEREN, (int)&termWriteSemaphore, 0, 0);

    for (int i = 0; i < len; i++) {
        unsigned int cmd =
            ((unsigned int)(unsigned char)str[i] << 8) | TRANSMITCHAR;
        unsigned int status =
            SYSCALL(DOIO, (int)&term->transm_command, (int)cmd, 0);

        if ((status & 0xFF) != OKCHARTRANS) {
            SYSCALL(VERHOGEN, (int)&termWriteSemaphore, 0, 0);
            return -((int)(status & 0xFF));
        }

        transmitted++;
    }

    SYSCALL(VERHOGEN, (int)&termWriteSemaphore, 0, 0);
    return transmitted;
}

static int readTerminal(char *buf) {
    termreg_t *term = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, 0);
    int received = 0;
    /* Spec 7.3: "suspended until a line of input has been transmitted" */
    SYSCALL(PASSEREN, (int)&termReadSemaphore, 0, 0);

    while (1) {
        unsigned int status =
            SYSCALL(DOIO, (int)&term->recv_command, (int)RECEIVECHAR, 0);

        if ((status & 0xFF) != CHARRECV) {
            SYSCALL(VERHOGEN, (int)&termReadSemaphore, 0, 0);
            return -((int)(status & 0xFF));
        }

        char c = (char)((status >> 8) & 0xFF);
        buf[received++] = c;

        if (c == '\n')
            break;
    }

    SYSCALL(VERHOGEN, (int)&termReadSemaphore, 0, 0);
    return received;
}

// TERMINATE
void sys2(support_t *sup) {
    int asid = sup->sup_asid;

    if (holdingSwapMutex[asid - 1]) {
        holdingSwapMutex[asid - 1] = 0;
        SYSCALL(VERHOGEN, (int)&swapSemaphore, 0, 0);
    }

    if (asid == SHELL_ASID) {
        SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
    } else {
        SYSCALL(VERHOGEN, (int)&shellSemaphore, 0, 0);
    }


    // Optimization number 2:
    // When a U-proc terminates, mark all of the frames it occupied as unoccupied
    SYSCALL(PASSEREN, (int)&swapSemaphore, 0, 0);
    for (int j = 0; j < POOLSIZE; j++)
    {
        if (swapPool[j].sw_asid == asid)
            swapPool[j].sw_asid = -1;
    }
    SYSCALL(VERHOGEN, (int)&swapSemaphore, 0, 0);


    SYSCALL(TERMPROCESS, 0, 0, 0);
}

// WRITETERMINAL
void sys4(support_t *sup) {
    state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);
    
    char *str = (char *)excState->reg_a1;
    int len = (int)excState->reg_a2;

    if ((unsigned int)str < KUSEG ||
        (unsigned int)str + (unsigned int)len > USERSTACKTOP) {
        sys2(sup);
        return;
    }

    if (len < 0 || len > MAXSTRLENG) {
        sys2(sup);
        return;
    }

    excState->reg_a0 = writeTerminal(str, len);
    LDST(excState);
}

// READTERMINAL
void sys5(support_t *sup) {
    state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);

    char *buf = (char *)excState->reg_a1;

    /* Valida indirizzo */
    if ((unsigned int)buf < KUSEG) {
        sys2(sup);
        return;
    }

    excState->reg_a0 = readTerminal(buf);
    LDST(excState);
}

// EXECUTE
void sys6(support_t *sup) {
    state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);

    int asid = excState->reg_a1;

    if (sup->sup_asid != SHELL_ASID) {
        sys2(sup);
        return;
    }

    if (asid <= 0 || asid > UPROCMAX || asid == SHELL_ASID) {
        sys2(sup);
        return;
    }

    int pid = createUProc(asid);

    if (pid == NOPROC) {
        sys2(sup);
        return;
    }

    SYSCALL(PASSEREN, (int)&shellSemaphore, 0, 0);

    LDST(excState);
}
