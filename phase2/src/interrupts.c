#include "../../headers/const.h"
#include "../../headers/types.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h> //NON AVREBBE SENSO INCLUDERE TUTTO CIO' CHE SI TROVA IN uriscv/ ?????????????????????????
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "../headers/initial.h"
#include "../headers/scheduler.h"

//----------------------TO AVOID MEMCPY ERRORS--------------------
void *memcpy(void *dest, const void *src, unsigned int n) {
    char *d = dest;
    const char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}


//Import global vars from initial.c
extern struct list_head readyQueue; 
//extern int processCount; 
extern int softBlock_count; 
extern pcb_t* currProc; 
extern int device_semaphores[NRSEMAPHORES]; 

// Aux function to get the exception state of the curr processor
state_t * getCurrExceptionState(){
    return GET_EXCEPTION_STATE_PTR(getPRID());
}



void pltInterruptHandler(){
    setTIMER(TIMESLICE); //let 5ms pass on the processor: 
    if(currProc){
        state_t * excState=getCurrExceptionState(); //Copy the processor state at the time of the interrupt
        currProc->p_s=*excState; 

        insertProcQ(&readyQueue, currProc); //Place the curr process on the ready queue
    }

    dispatch(); 
    //LDST(getCurrExceptionState());

}


void itInterruptHandler(){
    LDIT(PSECOND); //Acknowledging the interrupt by loading the Interval Timer with 100ms.

    int * pseudoClockSem=&device_semaphores[NSUPPSEM]; //Get the last semaphore
    pcb_t * unblockedPcb;

    while((unblockedPcb=removeBlocked(pseudoClockSem))){ //Unlock all the procs waiting for a tick
        insertProcQ(&readyQueue, unblockedPcb);
        softBlock_count--;
    }

    *pseudoClockSem=0;

    //Returning the control to the curr proc. if that exists. Identical to the pltInterruptHandler
    if(currProc){
        state_t *excState=getCurrExceptionState(); // Loading the state on the current proc.
        currProc->p_s=*excState;
        insertProcQ(&readyQueue, currProc);        
    }

    dispatch();
    //LDST(getCurrExceptionState()); 

}

void deviceInterruptHandler(int excCode){

    int DevNo=-999;
    int IntLineNo=-999;

    switch (excCode){

        case IL_DISK:
            IntLineNo=3;
            break;

        case IL_FLASH:
            IntLineNo=4;
            break;

        case IL_ETHERNET:
            IntLineNo=5;
            break;

        case IL_PRINTER:
            IntLineNo=6;
            break;

        case IL_TERMINAL:
            IntLineNo=7;
            break;        

        default:
            return;
    }

    int mapAddr=0x10000040+((IntLineNo-3)*WORDLEN); // startingAddress+(normilized line of 2nd table)*lengthOfWord
    int bitmap=*((int*)mapAddr); // Saves in bitmap the value of the map pointed by the address

    // Let's find which device actually triggered the interrupt
    if(bitmap & DEV0ON) DevNo=0;
    else if(bitmap & DEV1ON) DevNo=1;
    else if(bitmap & DEV2ON) DevNo=2;
    else if(bitmap & DEV3ON) DevNo=3;
    else if(bitmap & DEV4ON) DevNo=4;
    else if(bitmap & DEV5ON) DevNo=5;
    else if(bitmap & DEV6ON) DevNo=6;
    else if(bitmap & DEV7ON) DevNo=7;

    if(DevNo==-999) return;

    int devAddr=START_DEVREG+((IntLineNo-3)*0x80) + (DevNo*0x10); // startingAddress+(lineOffset)+(deviceOffset)
    int semaphoreIdx=(IntLineNo-3)*DEVPERINT+DevNo; // To switch from matrix form to array you get the deviceLine and multiply it by 8(since there's 8 devices for each line) and the add the device number
    int status;
    

    if(IntLineNo==7){ // Check to see if this interrupt is generated from a terminal "sender"
        int transmissionStatus=*((int*)(devAddr+0x8)); //Reads the transmission status register using the offset of 0x8
        if((transmissionStatus & 0xFF)!=0){ // If the transimmion register contains a value, hence the transmission is completed
            status=transmissionStatus; 
            *((int*)(devAddr+0xC))=ACK; // Acknowledges the end of this transmission
        }
        else{ // The interrupts comes from the reciever
            status=*((int*)(devAddr+0x0)); // Reads the reciever status
            *((int*)(devAddr+0x4))=ACK; 
            semaphoreIdx+=DEVPERINT; // Offsets the semaphore to the recieving semaphore index
        }
    }

    // Remove first process waiting for that semaphore, we are performing a sem.V()
    int* deviceSem=&device_semaphores[semaphoreIdx];
    // (*deviceSem)++; //---------------------DEVO INCREMENTARE IL VALORE DEL SEMAFORO?
    pcb_t * unblockedPcb=removeBlocked(deviceSem);

    if(unblockedPcb){
        unblockedPcb->p_s.gpr[24]=status;// gpr[24] maps the a0 register that is used for syscall return values
        insertProcQ(&readyQueue, unblockedPcb);
        softBlock_count--;

        // Saves the currProc state and puts him in the queue since there is another process(the unblocked one) who should be starting as we call the scheduler with dispatch()
        if(currProc){
            LDST(getCurrExceptionState());
        }
        else{
            dispatch();
        }
                
    }

}


void interruptHandler(){
    state_t * excState=getCurrExceptionState();
    int excCause=excState->cause;
    int excCode=excCause & CAUSE_EXCCODE_MASK;  // 0x7FFFFFFF
    
    if(excCode==IL_CPUTIMER){
        //Process Local Timer
        pltInterruptHandler();
    }
    else if(excCode==IL_TIMER){
        //Interval Timer
        itInterruptHandler();
    }
    else if(excCode>=IL_DISK && excCode<=IL_TERMINAL){
        //exceptions 17 to 21 for Devices
        deviceInterruptHandler(excCode);
    }
}
