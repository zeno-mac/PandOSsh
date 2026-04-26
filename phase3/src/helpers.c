#include "../headers/helpers.h"
#include "../../headers/const.h"
#include <uriscv/liburiscv.h>
/*
 * Queste funzioni sono implementate in altri moduli della Phase 3.
 *
 * pager:
 *   gestore delle eccezioni TLB/Page Fault.
 *
 * generalExceptionHandler:
 *   gestore generale delle eccezioni del Support Level:
 *   syscall positive + program trap.
 */
extern void pager(void);
extern void generalExceptionHandler(void);
/*
 * Stato iniziale degli U-proc:
 * - user mode
 * - interrupt abilitati
 *
 * Il campo mie viene poi impostato separatamente a MIE_ALL.
 */
#define UPROC_STATUS (MSTATUS_MPP_U | MSTATUS_MIE_MASK)

/*
 * Stato dei gestori del Support Level:
 * - kernel mode
 * - interrupt abilitati
 *
 * I gestori del Support Level vengono eseguiti in kernel mode.
 */
#define SUPPORT_STATUS (MSTATUS_MPP_M | MSTATUS_MIE_MASK)

/*
 * Ultimo indice della page table privata.
 * Le entry 0..30 sono per text/data.
 * L'entry 31 è per lo stack.
 */
#define STACK_PAGE_INDEX (USERPGTBLSIZE - 1)

/*
 * Ultimo indice degli stack interni alla support_t.
 * Gli stack crescono verso il basso, quindi lo stack pointer
 * deve puntare alla fine dell'area allocata.
 */
#define SUPPORT_STACK_LAST_INDEX 499

/*
 * Azzera manualmente uno state_t.
 *
 * Evitiamo memset per restare coerenti con lo stile del progetto
 * e con il codice delle fasi precedenti.
 */
static void clearState(state_t *state) {
  state->entry_hi = 0;
  state->cause = 0;
  state->status = 0;
  state->pc_epc = 0;
  state->mie = 0;

  for (int i = 0; i < STATE_GPR_LEN; i++) {
    state->gpr[i] = 0;
  }
}

/*
 * Costruisce il valore EntryHI per una pagina privata di un U-proc.
 *
 * EntryHI contiene:
 * - VPN, cioè il numero di pagina virtuale;
 * - flag di segmento privato;
 * - ASID del processo.
 */
static unsigned int makePrivateEntryHI(memaddr virtualAddress, int asid) {
  unsigned int vpn = virtualAddress & GETPAGENO;
  unsigned int privateSegment = PRIVATE << SHAREDSEGFLAG;
  unsigned int processAsid = asid << ASIDSHIFT;

  return vpn | privateSegment | processAsid;
}

/*
 * Inizializza una singola entry della page table privata.
 *
 * All'inizio nessuna pagina è caricata in RAM, quindi:
 * - V = 0, pagina non valida;
 * - D = 1, pagina scrivibile;
 * - PFN non impostato.
 */
static void initPageTableEntry(pteEntry_t *entry, memaddr virtualAddress,
                               int asid) {
  entry->pte_entryHI = makePrivateEntryHI(virtualAddress, asid);
  entry->pte_entryLO = DIRTYON;
}

/*
 * Inizializza la page table privata dell'U-proc.
 *
 * La page table ha 32 entry:
 * - entry 0..30: pagine text/data a partire da 0x80000000;
 * - entry 31: pagina dello stack, cioè 0xBFFFF000.
 */
static void initPageTable(support_t *support, int asid) {
  for (int i = 0; i < STACK_PAGE_INDEX; i++) {
    memaddr virtualAddress = KUSEG + (i * PAGESIZE);
    initPageTableEntry(&support->sup_privatePgTbl[i], virtualAddress, asid);
  }

  /*
   * Lo stack utente parte da 0xC0000000 e cresce verso il basso.
   * Quindi la sua pagina effettiva inizia a:
   *
   * 0xC0000000 - 0x1000 = 0xBFFFF000
   */
  memaddr stackPageAddress = USERSTACKTOP - PAGESIZE;

  initPageTableEntry(&support->sup_privatePgTbl[STACK_PAGE_INDEX],
                     stackPageAddress, asid);
}

/*
 * Inizializza lo stato iniziale del processore dell'U-proc.
 *
 * Questo è lo stato passato a CREATEPROCESS.
 */
static void initInitialProcessorState(state_t *state, int asid) {
  clearState(state);

  state->pc_epc = UPROCSTARTADDR;
  state->reg_sp = USERSTACKTOP;

  state->status = UPROC_STATUS;
  state->mie = MIE_ALL;

  state->entry_hi = asid << ASIDSHIFT;
}

/*
 * Inizializza il context usato quando viene passata verso l'alto
 * un'eccezione TLB/Page Fault.
 */
static void initPageFaultContext(support_t *support) {
  support->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)pager;

  support->sup_exceptContext[PGFAULTEXCEPT].stackPtr =
      (memaddr)&support->sup_stackTLB[SUPPORT_STACK_LAST_INDEX];

  support->sup_exceptContext[PGFAULTEXCEPT].status = SUPPORT_STATUS;
}

/*
 * Inizializza il context usato quando viene passata verso l'alto
 * un'eccezione generale:
 * - syscall positive;
 * - program trap.
 */
static void initGeneralExceptionContext(support_t *support) {
  support->sup_exceptContext[GENERALEXCEPT].pc =
      (memaddr)generalExceptionHandler;

  support->sup_exceptContext[GENERALEXCEPT].stackPtr =
      (memaddr)&support->sup_stackGen[SUPPORT_STACK_LAST_INDEX];

  support->sup_exceptContext[GENERALEXCEPT].status = SUPPORT_STATUS;
}

/*
 * Funzione pubblica usata da createUProc().
 *
 * Inizializza tutto ciò che serve per creare un U-proc:
 * - stato iniziale del processore;
 * - ASID nella support structure;
 * - context per page fault;
 * - context per eccezioni generali;
 * - page table privata.
 */
void initUProc(int asid, state_t *state, support_t *support) {
  if (asid <= 0 || asid > UPROCMAX || state == NULL || support == NULL) {
    PANIC();
  }

  support->sup_asid = asid;

  initInitialProcessorState(state, asid);
  initPageFaultContext(support);
  initGeneralExceptionContext(support);
  initPageTable(support, asid);
}
