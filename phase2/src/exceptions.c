#include "../headers/initial.h"
#include "../headers/scheduler.h"
#include "../headers/interrupts.h"
#include "../headers/syscall.h"
#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../headers/listx.h"
#include <uriscv/liburiscv.h>

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"



void uTLB_RefillHandler() {
  int prid = getPRID();
  setENTRYHI(0x80000000);
  setENTRYLO(0x00000000);
  TLBWR();
  LDST((state_t*) BIOSDATAPAGE);
}



void exceptionHandler() {
  state_t * excState = getCurrExceptionState();
  int excCode = excState->cause;
  if (CAUSE_IS_INT(excCode)) {
    interruptHandler();
    return; 
  }
  //anche switch (expression) {
  //}
  if (excCode>=24 && excCode<=28){
    tlbHandler();
  }
  else if (excCode == 8 || excCode == 11){
    syscallHandler();
  }
  else {
    trapHandler();
  }

}



void syscallHandler() {
  state_t * excState = getCurrExceptionState();
  int a0_numSys = excState->reg_a0;
  int a1 = excState->reg_a1;
  int a2 = excState->reg_a2;
  int a3 = excState->reg_a3;
  
  if(a0_numSys < 0 && (MSTATUS_MPP_MASK & excState->status == MSTATUS_MPP_U)) {
    //interrupt
    return;
  }
  else{

    excState->pc_epc += WORDLEN;

    switch (a0_numSys) {
      case CREATEPROCESS:
        excState->reg_a0 = nsys1(a1,a2,a3);
        LDST(excState);
        break;
      case TERMINATEPROCESS:
        nsys2(a1, excState);
        break;
      case PASSEREN:

        break;
      case VERHOGEN:

        break;
      case DOIO:

        break;
      case GETCPUTIME:

        break;
      case WAITCLOCK:

        break;
      case GETSUPPORTPTR:

        break;
      case GETPID:

        break;
      case YIELD:

        break;
      case default:
        //PASS UP OR DIE for numSys > 0
        break;
    }
      
    
  }




  
}




//TLB-Refill Event

