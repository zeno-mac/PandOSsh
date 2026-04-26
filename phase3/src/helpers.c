#include "../../headers/const.h"
#include "../../headers/types.h"
#include <uriscv/liburiscv.h>
/*
 * Cambia questi nomi se nel tuo progetto i gestori si chiamano diversamente.
 */
extern void pager(void);
extern void generalExceptionHandler(void);
/*
 * Status per U-proc:
 * - user mode
 * - interrupt abilitati
 *
 * Nota: nel vostro const.h ci sono sia vecchie costanti tipo USERPON/IEPON
 * sia costanti RISC-V MSTATUS_*.
 * Visto che state usando uriscv, parto dalle MSTATUS_*.
 */
#define UPROC_STATUS (MSTATUS_MPP_U | MSTATUS_MIE_MASK)

/*
 * Status per i gestori del support level:
 * - kernel mode
 * - interrupt abilitati
 */
#define SUPPORT_STATUS (MSTATUS_MPP_M | MSTATUS_MIE_MASK)

static unsigned int makeEntryHI(memaddr virtAddr, int asid) {
  return (virtAddr & GETPAGENO) | (PRIVATE << SHAREDSEGFLAG) |
         (asid << ASIDSHIFT);
}

static void initPageTable(support_t *support, int asid) {
  /*
   * Entry 0..30: pagine .text/.data da 0x80000000 in poi.
   */
  for (int i = 0; i < USERPGTBLSIZE - 1; i++) {
    memaddr virtAddr = KUSEG + (i * PAGESIZE);

    support->sup_privatePgTbl[i].pte_entryHI = makeEntryHI(virtAddr, asid);

    /*
     * All'inizio la pagina NON è in RAM:
     * quindi V resta spento.
     *
     * Dirty invece va acceso, come richiesto dalla specifica.
     */
    support->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
  }

  /*
   * Ultima entry: pagina dello stack.
   * Lo stack parte da 0xC0000000 e cresce verso il basso,
   * quindi la pagina è 0xBFFFF000.
   */
  memaddr stackPage = USERSTACKTOP - PAGESIZE;

  support->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI =
      makeEntryHI(stackPage, asid);

  support->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;
}

void initUProc(int asid, state_t *state, support_t *support) {

  /*
   * Controllo difensivo: gli ASID validi sono 1..UPROCMAX.
   * ASID 0 è riservato al kernel.
   */
  if (asid <= 0 || asid > UPROCMAX || state == NULL || support == NULL) {
    PANIC();
  }

  support->sup_asid = asid;

  /*
   * Stato iniziale dell'U-proc.
   *
   * Dalla specifica:
   * PC = 0x800000B0
   * SP = 0xC0000000
   * user mode
   * interrupt/local timer abilitati
   * EntryHi.ASID = asid
   */
  state->pc_epc = UPROCSTARTADDR;
  state->reg_sp = USERSTACKTOP;
  state->status = UPROC_STATUS;
  state->mie = MIE_ALL;
  state->entry_hi = (asid << ASIDSHIFT);

  /*
   * Context per il gestore TLB/Page Fault del support level.
   */
  support->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)pager;

  support->sup_exceptContext[PGFAULTEXCEPT].stackPtr =
      (memaddr) & (support->sup_stackTLB[499]);

  support->sup_exceptContext[PGFAULTEXCEPT].status = SUPPORT_STATUS;

  /*
   * Context per il gestore generale:
   * syscall positive + program trap.
   */
  support->sup_exceptContext[GENERALEXCEPT].pc =
      (memaddr)generalExceptionHandler;

  support->sup_exceptContext[GENERALEXCEPT].stackPtr =
      (memaddr) & (support->sup_stackGen[499]);

  support->sup_exceptContext[GENERALEXCEPT].status = SUPPORT_STATUS;

  /*
   * Page table privata dell'U-proc.
   */
  initPageTable(support, asid);
}
