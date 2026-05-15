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
/*
 * Writes a string to terminal 0.
 *
 * This helper implements the low-level part of SYS4. Terminal output is
 * performed character by character using the terminal transmitter sub-device.
 *
 * Access to the transmitter is protected by termWriteSemaphore, because the
 * terminal is a shared device and only one process at a time should write to
 * it.
 *
 * Parameters:
 * - str: pointer to the first character to transmit;
 * - len: number of characters to transmit.
 *
 * Returns:
 * - the number of characters successfully transmitted;
 * - a negative device status if the transmission fails.
 */
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
/*
 * Reads a line from terminal 0.
 *
 * This helper implements the low-level part of SYS5. Terminal input is read
 * one character at a time using the terminal receiver sub-device.
 *
 * Access to the receiver is protected by termReadSemaphore, because terminal
 * input is shared and only one process at a time should read from it.
 *
 * Parameters:
 * - buf: user buffer where the received characters are stored.
 *
 * Returns:
 * - the number of characters received;
 * - a negative device status if the receive operation fails.
 */
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
        if (received < MAXSTRLENG) {
            buf[received] = c;
            received++;
        }

        if (c == '\n')
            break;
    }

    SYSCALL(VERHOGEN, (int)&termReadSemaphore, 0, 0);
    return received;
}

/*
 * SYS2 - Terminate
 *
 * Terminates the current U-proc in an ordered way.
 *
 * This function is used both when a user process explicitly requests
 * TERMINATE and when the Support Level decides to kill a process because of
 * an invalid syscall or a Program Trap.
 *
 * Before invoking the Nucleus termination service, the function performs the
 * required Support Level cleanup:
 * - releases the Swap Pool mutex if the process was holding it;
 * - wakes up the Instantiator Process if the terminating process is the shell;
 * - wakes up the shell if the terminating process is a child program;
 * - marks all Swap Pool frames owned by this ASID as free.
 */
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

/*
 * SYS4 - WriteTerminal
 *
 * Writes a string from the U-proc logical address space to terminal 0.
 *
 * The U-proc passes:
 * - the virtual address of the string in register a1;
 * - the string length in register a2.
 *
 * The syscall validates the parameters, writes the string using
 * writeTerminal(), stores the return value in a0 and restores the saved
 * processor state.
 *
 * Invalid parameters cause the current U-proc to terminate.
 */
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

/*
 * SYS5 - ReadTerminal
 *
 * Reads a line from terminal 0 into a buffer in the U-proc logical address
 * space.
 *
 * The U-proc passes the virtual address of the destination buffer in register
 * a1. The syscall validates the address, reads from terminal using
 * readTerminal(), stores the return value in a0 and restores the saved
 * processor state.
 *
 * Invalid parameters cause the current U-proc to terminate.
 */
void sys5(support_t *sup) {
    state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);

    char *buf = (char *)excState->reg_a1;

    /* Valida indirizzo */
    if ((unsigned int)buf < KUSEG || (unsigned int)buf >= USERSTACKTOP) {
        sys2(sup);
        return;
    }

    excState->reg_a0 = readTerminal(buf);
    LDST(excState);
}

/*
 * SYS6 - Execute
 *
 * Creates and executes a new U-proc.
 *
 * This syscall is intended to be used only by the shell. The shell passes the
 * ASID of the program to execute in register a1. The Support Level validates
 * the request, creates the corresponding U-proc and then blocks the shell
 * until that U-proc terminates.
 *
 * This implements a synchronous execution model:
 * the shell starts a program, waits for its termination, and then resumes.
 */
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
