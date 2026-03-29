#include "../headers/aux.h"

void copyState(state_t *myStruct, state_t *newState) {
  myStruct->cause = newState->cause;
  myStruct->entry_hi = newState->entry_hi;
  myStruct->mie = newState->mie;
  myStruct->pc_epc = newState->pc_epc;
  myStruct->status = newState->status;
  for (int i = 0; i < STATE_GPR_LEN; i++)
    myStruct->gpr[i] = newState->gpr[i];
}
