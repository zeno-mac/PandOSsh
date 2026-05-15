#include "../headers/initProc.h"
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../headers/helpers.h"
#include <uriscv/liburiscv.h>

#define UPROC_PRIORITY PROCESS_PRIO_LOW

int masterSemaphore;
int shellSemaphore;

int termWriteSemaphore;
int termReadSemaphore;

support_t supportPool[UPROCMAX];

extern void initSwapPool(void); 
/*
 * Creates a new U-proc with the given ASID.
 *
 * The function prepares both the initial processor state and the Support
 * Structure required by the Nucleus CREATEPROCESS syscall.
 *
 * The ASID must be valid and non-zero, because ASID 0 is reserved for kernel
 * processes. For a valid ASID, the corresponding Support Structure is selected
 * from supportPool and initialized by initUProc().
 *
 * Returns:
 * - the PID of the newly created process on success;
 * - NOPROC if CREATEPROCESS fails.
 */
int createUProc(int asid) {
    if (asid <= 0 || asid > UPROCMAX) {
        PANIC();
    }

    state_t procState;
    support_t *support = &supportPool[asid - 1];

    initUProc(asid, &procState, support);

    /*
     * Ask the Nucleus to create the new process.
     *
     * The new process receives:
     * - the initialized processor state;
     * - the default U-proc priority;
     * - the initialized Support Structure.
     */
    return SYSCALL(CREATEPROCESS, (int)&procState, UPROC_PRIORITY,
                   (int)support);
}
/*
 * Phase 3 Instantiator Process.
 * The Instantiator Process performs the following operations:
 * - initializes Phase 3 synchronization semaphores;
 * - initializes virtual memory support structures;
 * - creates the shell U-proc;
 * - waits for the shell to terminate;
 * - terminates itself, allowing the system to halt cleanly.
 */
void test() {
    int pid;

    masterSemaphore = 0;
    shellSemaphore = 0;

    termWriteSemaphore = 1;
    termReadSemaphore = 1;

    initSwapPool();

    pid = createUProc(SHELL_ASID);
    if (pid == NOPROC) {
        PANIC();
    }

    SYSCALL(PASSEREN, (int)&masterSemaphore, 0, 0);

    SYSCALL(TERMPROCESS, 0, 0, 0);
}
