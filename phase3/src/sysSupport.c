#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/auxfun.h"
#include "../headers/sysSupport.h"
#include "../headers/syscalls.h"
#include <uriscv/liburiscv.h>

/* ==========================================================================
 * VARIABILI ESTERNE — vmSupport.c (Persona 1/2)
 * ========================================================================== */
extern int swapSemaphore;
extern int holdingSwapMutex[UPROCMAX]; // andrà aggiunto che serve per capire se il processo sta tenendo il mutex del swap pool, in modo da rilasciarlo in caso di terminazione forzata

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
