#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/auxfun.h"
#include "../headers/sysSupport.h"
#include "../headers/syscalls.h"
#include <uriscv/liburiscv.h>

/* ==========================================================================
 * VARIABILI ESTERNE — vmSupport.c (Persona 1/2)
 * ========================================================================== */
extern int swapPoolSemaphore;
extern int holdingSwapMutex[UPROCMAX];
extern void tlbExceptionHandler();

void generalExceptionHandler()
{

  support_t *sup = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

  /* Estrai ExcCode: bit 6:2 del registro cause */
  unsigned int cause = sup->sup_exceptState[GENERALEXCEPT].cause;
  unsigned int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;

  if (excCode == SYSEXCEPTION || excCode == 11) // non so se serve anche cause=11
    pos_syscallHandler(sup);
  else
    programTrapHandler(sup);
}

void programTrapHandler(support_t *sup)
{
  sys2(sup);
}

void pos_syscallHandler(support_t *sup)
{
  state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);
  int a0_numSys = excState->reg_a0;
  /*int a1 = excState->reg_a1;
  int a2 = excState->reg_a2;
  int a3 = excState->reg_a3;*/

  // #TODO: increment pc
  excState->pc_epc += WORDLEN;

  switch (a0_numSys)
  {
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

// QUESTI LI METTO MA NON LI HO CONTROLLATI (li ha fatti cluadio :))

/* ==========================================================================
 * SEZIONE 9 — Inizializzazione U-proc
 * ========================================================================== */

/*
 * initPageTable  (statica)
 *
 * Inizializza la Page Table privata di 32 entry.
 *
 * Entry 0..30: pagine .text/.data (VPN 0x80000..0x8001E)
 * Entry 31:    pagina stack       (VPN 0xBFFFF)
 *
 * pte_entryLO = DIRTYON (D=1, V=0 non valida, G=0 privata)
 */
static void initPageTable(pteEntry_t *pgTbl, int asid)
{
  int i;
  for (i = 0; i < USERPGTBLSIZE - 1; i++)
  {
    pgTbl[i].pte_entryHI = ((0x80000 + i) << VPNSHIFT) | (asid << ASIDSHIFT);
    pgTbl[i].pte_entryLO = DIRTYON;
  }
  pgTbl[USERPGTBLSIZE - 1].pte_entryHI = (0xBFFFF << VPNSHIFT) | (asid << ASIDSHIFT);
  pgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;
}

/*
 * initSupportStruct
 *
 * Inizializza completamente una Support Structure per un U-proc.
 *
 * Imposta:
 *   sup_asid
 *   sup_privatePgTbl[32]             — initPageTable() (tutte non valide)
 *   sup_exceptContext[PGFAULTEXCEPT] — Pager: tlbExceptionHandler()
 *   sup_exceptContext[GENERALEXCEPT] — dispatcher: generalExceptionHandler()
 *
 * Status handler: kernel-mode, tutti gli interrupt abilitati.
 * SP dei context: fine dei rispettivi stack (crescono verso il basso).
 * Spec: Sezione 9.1.2, p.12-13
 */
void initSupportStruct(support_t *sup, int asid)
{
  sup->sup_asid = asid;

  initPageTable(sup->sup_privatePgTbl, asid);

  unsigned int handlerStatus = MSTATUS_MPP_M |
                               MSTATUS_MPIE_MASK |
                               MSTATUS_MIE_MASK |
                               IMON;

  /* Context 0 — PGFAULTEXCEPT: Pager (vmSupport.c) */
  sup->sup_exceptContext[PGFAULTEXCEPT].pc = (unsigned int)tlbExceptionHandler;
  sup->sup_exceptContext[PGFAULTEXCEPT].status = handlerStatus;
  sup->sup_exceptContext[PGFAULTEXCEPT].stackPtr =
      (unsigned int)&sup->sup_stackTLB[499];

  /* Context 1 — GENERALEXCEPT: dispatcher (questo file) */
  sup->sup_exceptContext[GENERALEXCEPT].pc = (unsigned int)generalExceptionHandler;
  sup->sup_exceptContext[GENERALEXCEPT].status = handlerStatus;
  sup->sup_exceptContext[GENERALEXCEPT].stackPtr =
      (unsigned int)&sup->sup_stackGen[499];
}

/*
 * initUProcState
 *
 * Inizializza lo stato iniziale del processore per un U-proc.
 * Spec: Sezione 9.1.1, p.12
 *   PC = UPROCSTARTADDR (0x800000B0)
 *   SP = USERSTACKTOP   (0xC0000000)
 *   Status: user-mode, MPIE=1, MIE=1, IMON, TEBITON
 *   EntryHi.ASID = asid
 */
void initUProcState(state_t *s, int asid)
{
  int i;
  for (i = 0; i < STATE_GPR_LEN; i++)
    s->gpr[i] = 0;

  s->pc_epc = UPROCSTARTADDR;
  s->reg_sp = USERSTACKTOP;
  s->status = MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK | IMON | TEBITON;
  s->mie = MIE_ALL;
  s->cause = 0;
  s->entry_hi = (unsigned int)(asid << ASIDSHIFT);
}