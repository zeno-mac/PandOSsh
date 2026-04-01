#include "../headers/auxfun.h"


#include "../../headers/const.h"    
#include <uriscv/liburiscv.h>       
#include <uriscv/cpu.h>

void copyState(state_t *myStruct, state_t *newState) {
  myStruct->cause = newState->cause;
  myStruct->entry_hi = newState->entry_hi;
  myStruct->mie = newState->mie;
  myStruct->pc_epc = newState->pc_epc;
  myStruct->status = newState->status;
  for (int i = 0; i < STATE_GPR_LEN; i++)
    myStruct->gpr[i] = newState->gpr[i];
}


// Aux function to get the exception state of the curr processor
state_t *getCurrExceptionState() { return GET_EXCEPTION_STATE_PTR(getPRID()); }