#include "../headers/syscalls.h"
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/auxfun.h"
#include "../headers/initProc.h"
#include <uriscv/arch.h> /* DEV_REG_ADDR, IL_TERMINAL        */
#include <uriscv/liburiscv.h>
#include <uriscv/types.h> /* termreg_t                        */
extern void klog_print(char *str);
extern void klog_print_hex(unsigned int num);
extern void klog_print_dec(unsigned int num);
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

    klog_print("writeTerminal start\n");

    SYSCALL(PASSEREN, (int)&termWriteSemaphore, 0, 0);

    for (int i = 0; i < len; i++) {
        unsigned int cmd =
            ((unsigned int)(unsigned char)str[i] << 8) | TRANSMITCHAR;

        klog_print("before DOIO term write\n");

        unsigned int status =
            SYSCALL(DOIO, (int)&term->transm_command, (int)cmd, 0);

        klog_print("after DOIO term write status=");
        klog_print_dec(status & 0xFF);
        klog_print("\n");

        if ((status & 0xFF) != OKCHARTRANS) {
            klog_print("term write error\n");
            SYSCALL(VERHOGEN, (int)&termWriteSemaphore, 0, 0);
            return -((int)(status & 0xFF));
        }

        transmitted++;
    }

    SYSCALL(VERHOGEN, (int)&termWriteSemaphore, 0, 0);

    klog_print("writeTerminal end transmitted=");
    klog_print_dec(transmitted);
    klog_print("\n");

    return transmitted;
}

static int readTerminal(char *buf) {
    termreg_t *term = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, 0);
    int received = 0;

    klog_print("readTerminal start\n");

    SYSCALL(PASSEREN, (int)&termReadSemaphore, 0, 0);

    while (1) {
        klog_print("before DOIO term read\n");

        unsigned int status =
            SYSCALL(DOIO, (int)&term->recv_command, (int)RECEIVECHAR, 0);

        klog_print("after DOIO term read status=");
        klog_print_dec(status & 0xFF);
        klog_print("\n");

        if ((status & 0xFF) != CHARRECV) {
            klog_print("term read error\n");
            SYSCALL(VERHOGEN, (int)&termReadSemaphore, 0, 0);
            return -((int)(status & 0xFF));
        }

        char c = (char)((status >> 8) & 0xFF);
        buf[received++] = c;

        if (c == '\n')
            break;
    }

    SYSCALL(VERHOGEN, (int)&termReadSemaphore, 0, 0);

    klog_print("readTerminal end received=");
    klog_print_dec(received);
    klog_print("\n");

    return received;
}

// TERMINATE
void sys2(support_t *sup) {
    klog_print("SYS2 terminate asid=");
    klog_print_dec(sup->sup_asid);
    klog_print("\n");
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
    klog_print("SYS4 write terminal\n");
    state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);
    klog_print("addr=");
    klog_print_hex(excState->reg_a1);
    klog_print(" len=");
    klog_print_dec(excState->reg_a2);
    klog_print("\n");
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
        return; /* mai raggiunto */
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
