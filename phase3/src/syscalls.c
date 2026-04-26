#include "../headers/syscalls.h"
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/auxfun.h"
#include "../headers/initProc.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h> /* termreg_t                        */
#include <uriscv/arch.h>  /* DEV_REG_ADDR, IL_TERMINAL        */

#define UPROC_PRIORITY PROCESS_PRIO_LOW // TODO: understand priority properly

extern int masterSemaphore;
extern int shellSemaphore;

/* ==========================================================================
 * VARIABILI ESTERNE — vmSupport.c (Persona 1/2)
 * ========================================================================== */

extern int swapPoolSemaphore;
extern int holdingSwapMutex[UPROCMAX];

// Quella a seguire farle ma mi serve questo:

/* ==========================================================================
 * HELPER I/O TERMINALE
 *
 * Phase 3 usa un solo terminale (devNo = 0), condiviso tra tutti gli U-proc.
 *
 * Due semafori binari (dichiarati in initProc.c) separano lettura e scrittura:
 *   termWriteSemaphore — protegge transm_command / transm_status
 *   termReadSemaphore  — protegge recv_command  / recv_status
 *
 * Indirizzo del terminale: DEV_REG_ADDR(IL_TERMINAL, 0) da uriscv/arch.h.
 *
 * Registri accessibili tramite termreg_t (uriscv/types.h):
 *   recv_status    — bit 7:0 = stato, bit 15:8 = char ricevuto
 *   recv_command   — scrivi RECEIVECHAR (2) per ricevere
 *   transm_status  — bit 7:0 = stato trasmissione
 *   transm_command — scrivi (char<<8)|TRANSMITCHAR per trasmettere
 * ========================================================================== */

extern int termWriteSemaphore;
extern int termReadSemaphore;
/*
 * writeTerminal  (statica, privata al modulo)
 *
 * Scrive 'len' caratteri di 'str' sul terminale 0.
 * Ogni carattere e' trasmesso singolarmente tramite DOIO (NSYS5).
 *
 * Comando al trasmettitore: (char << 8) | TRANSMITCHAR
 *   TRANSMITCHAR = 2  (da headers/const.h)
 *
 * Status (bit 7:0):
 *   OKCHARTRANS = 5 -> carattere trasmesso  (da headers/const.h)
 *   altro           -> errore, ritorna il negativo del byte di stato
 *
 * Spec: Sezione 7.2, p.10
 */
static int writeTerminal(char *str, int len);

/*
 * readTerminal  (statica, privata al modulo)
 *
 * Legge caratteri dal terminale 0 nel buffer 'buf'.
 * Ogni carattere e' letto singolarmente tramite DOIO (NSYS5).
 *
 * Comando al ricevitore: RECEIVECHAR = 2  (da headers/const.h)
 *
 * Status restituito da DOIO:
 *   bit  7:0 -> stato  (CHARRECV = 5 se ok, da headers/const.h)
 *   bit 15:8 -> carattere ricevuto
 *
 * Termina al '\n' o su errore.
 * Ritorna numero di caratteri ricevuti o negativo del device status.
 *
 * Spec: Sezione 7.3, p.10
 */
static int readTerminal(char *buf);

// TERMINATE
void sys2(support_t *sup) {
  int asid = sup->sup_asid;
  /*
   * rilascia il mutex della Swap Pool se trattenuto.
   * Il codice originale non lo faceva: questo poteva causare deadlock
   * se un Program Trap avveniva dentro la sezione critica del Pager.
   */
  if (holdingSwapMutex[asid - 1])
  {
    holdingSwapMutex[asid - 1] = 0;
    SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
  }

  if (asid == SHELL_ASID)
  {
    SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
  }
  else
  {
    SYSCALL(VERHOGEN, (int)&shellSemaphore, 0, 0);
  }

  SYSCALL(TERMPROCESS, 0, 0, 0);
}

// WRITETERMINAL
void sys4(support_t *sup)
{
  state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);

  char *str = (char *)excState->reg_a1;
  int len = (int)excState->reg_a2;

  if ((unsigned int)str < KUSEG ||
      (unsigned int)str + (unsigned int)len > USERSTACKTOP)
  {
    sys2(sup);
    return;
  }

  if (len < 0 || len > MAXSTRLENG)
  {
    sys2(sup);
    return;
  }

  excState->reg_a0 = writeTerminal(str, len);
  LDST(excState);
}

// READTERMINAL
void sys5(support_t *sup)
{
  state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);

  char *buf = (char *)excState->reg_a1;

  /* Valida indirizzo */
  if ((unsigned int)buf < KUSEG)
  {
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
