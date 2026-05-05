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

extern void initSwapStructs(void); /* sarà implementata da chi fa vmSupport */

int createUProc(int asid) {
    if (asid <= 0 || asid > UPROCMAX) {
        PANIC();
    }

    state_t procState;
    support_t *support = &supportPool[asid - 1];

    initUProc(asid, &procState, support);

    // ritorna il pid del nuovo processo o in caso di errore NOPROC = -1
    return SYSCALL(CREATEPROCESS, (int)&procState, UPROC_PRIORITY,
                   (int)support);
}

void test() {

    int pid;

    masterSemaphore = 0;
    shellSemaphore = 0;

    termWriteSemaphore = 1;
    termReadSemaphore = 1;
    /*
     * 2. Inizializzazione strutture VM.
     * Questa funzione dovrebbe inizializzare:
     * - swap pool table
     * - swap pool semaphore
     *
     * Se il tuo compagno non l'ha ancora fatta, puoi commentarla
     * temporaneamente, ma alla fine deve esserci.
     */
    initSwapStructs();

    /*
     * 3. Crea la shell.
     * Per convenzione nel tuo header SHELL_ASID = 1.
     */
    pid = createUProc(SHELL_ASID);

    if (pid == NOPROC) {
        PANIC();
    }

    /*
     * 4. Aspetta che la shell termini.
     * La shell, quando termina con SYS2, dovrà fare V(masterSemaphore).
     */
    SYSCALL(PASSEREN, (int)&masterSemaphore, 0, 0);

    /*
     * 5. Termina l'InstantiatorProcess.
     * Essendo test in kernel mode, qui si usa la syscall negativa del nucleo.
     */
    SYSCALL(TERMPROCESS, 0, 0, 0);
}
