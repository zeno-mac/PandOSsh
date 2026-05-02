
#include "../../headers/const.h"    
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>       

#include "../headers/vmSupport.h"
#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../phase2/src/initial.c"
#include "../headers/sysSupport.h"

static swap_t swapPool[POOLSIZE];
int swapSemaphore;
extern pcb_t *currProc;
extern 

void initPagetable(pteEntry_t *pageTable, int asid) {
    for (int i = 0; i < MAXPAGES-1; i++) {
        pageTable[i].pte_entryHI = (KUSEG + (i << VPNSHIFT) )| (asid << ASIDSHIFT);
        pageTable[i].pte_entryLO = DIRTYON;
    }

    pageTable[31].pte_entryHI = (0xBFFFF << VPNSHIFT) | (asid << ASIDSHIFT);
    //TODO Global set to 1 or 0? Nelle specifiche segna Global set to 1 (off), che vuol dire?
    pageTable[31].pte_entryLO = DIRTYON;
}

//TODO Spostare in exceptions.c secondo specifiche
void uTLB_RefillHandler(void){
    state_t *saved_state = (state_t *) BIOSDATAPAGE;
    unsigned int missingVPN = (saved_state->entry_hi & GETPAGENO) >> VPNSHIFT;
    pteEntry_t *pageTable = currProc->p_supportStruct->sup_privatePgTbl;
    int pageIndex = missingVPN % MAXPAGES;
    setENTRYHI(pageTable[pageIndex].pte_entryHI);
    setENTRYLO(pageTable[pageIndex].pte_entryLO);
    TLBWR();    
    LDST(saved_state);
}

void initSwapPool() {
    for (int i = 0; i < POOLSIZE; i++){
        swapPool[i].sw_asid = -1;   // -1 = unoccupied
        swapPool[i].sw_pageNo = -1; // Numero di pagina non valido
        swapPool[i].sw_pte = NULL;}
    swapSemaphore = 1;
}

// SEZIONE 4.2 - Mazzo

void pager() {
    
    support_t *suppPtr = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0); //Forse non serve casting a (support_t *)

    
    int cause = suppPtr->sup_exceptState[0].cause;
    cause=(cause & GETEXECCODE) >> CAUSESHIFT;

    
    if(cause==EXC_MOD) programTrapHandler(suppPtr);

    
    SYSCALL(PASSEREN, &swapSemaphore, 0, 0);

    
    int missingPageNo = suppPtr->sup_exceptState[0].entry_hi;
    missingPageNo = missingPageNo & GETPAGENO;

    static int fifoCont=0;
    int i=fifoCont;
    fifoCont=(fifoCont+1) % POOLSIZE;

    int physicalFrame=FLASHPOOLSTART +(i*PAGESIZE);

    
    if(swapPool[i].sw_asid!=-1) {
        //Frame occupied
        int x=swapPool[i].sw_asid;
        int k=swapPool[i].sw_pageNo;
        pteEntry_t *xPte=swapPool[i].sw_pte;

        xPte->pte_entryLO &= ~VALIDON;

        int status=getSTATUS();
        setSTATUS(status & ~MSTATUS_MIE_MASK);
        TLBCLR();
        setSTATUS(status);

        readWriteFlashdrive(x,k,physicalFrame,FLASHWRITE);

    }

    readWriteFlashdrive(suppPtr->sup_asid, missingPageNo, physicalFrame, FLASHREAD);

    swapPool[i].sw_asid=suppPtr->sup_asid;
    swapPool[i].sw_pageNo=missingPageNo;

    int pageIdx;
    if(missingPageNo==0xBFFFF) //Entry 31: pagina stack (VPN 0xBFFFF)
        pageIdx=31;
    else
        pageIdx=missingPageNo % MAXPAGES;

    swapPool[i].sw_pte=&suppPtr->sup_privatePgTbl[pageIdx];

    swapPool[i].sw_pte->pte_entryLO=physicalFrame | DIRTYON | VALIDON;

    int status=getSTATUS();
    setSTATUS(status & ~MSTATUS_MIE_MASK);
    TLBCLR();
    setSTATUS(status);

    SYSCALL(VERHOGEN,&swapSemaphore,0,0);

    LDST(&suppPtr->sup_exceptState[0]);
}

//SEZIONE 5.1 - Mazzo


// funzione aux per ricavare il blocco dato il VPN - Virtual Page Number
// blocco 31 <=> vpn 0xBFFFF
// blocchi[0..30] <=> vpn 0x80000,0x80001,..,0x8001E
// quindi per vpn=0x80002 il blocco è il 2
int getFlashBlock(int vpn){
    if(vpn==0xBFFFF) return 31; 
    else return(vpn-0x80000);
}

// funzione aux per calcolare l'addr base dei registri dei flash dev
// ogni processo utente ha un suo flash dev il cui asid va da 1..8, 
// segue che i gegisti vanno da 0..7 = (1..8)-1
devreg_t * getFlashRegister(int asid){
    int devNo=asid-1;

    //segue tabella per gestione interrupt implementata in phase2
    int devRegister=START_DEVREG+((IL_FLASH-3)*0x80)+(devNo*0x10); 
    
    return( (devreg_t *)devRegister );
}

/* da documentazione:

(A) Write the flash device’s DATA0 field with the appropriate starting physical address of the 4k
block to be read (or written); the particular frame’s starting address.
(B) Use the NSYS5 system call to write the flash device’s COMMAND field with the device block
number (high order three bytes) and the command to read (or write) in the lower order byte.
Where the following C code can be used to request a NSYS5:
int ioStatus = SYSCALL(DOIO, int *commandAddr, int commandValue, 0);
Where the mnemonic constant DOIO has the value of -5.

*/
int readWriteFlashdrive(int asid, int vpn, int phisicalFrame, int op){

    int block=getFlashBlock(vpn);
    devreg_t * reg=getFlashRegister(asid);

    reg->dtp.data0=phisicalFrame; //(A)

    // let's shift "block" up by 8bits(his size) so "op" can occupy the 4th byte
    // we'll end up un a situation like the following: [0..0][0..0][block][op]
    int command= (block<<8) | op; 

    int ioStatus;
    ioStatus=SYSCALL(DOIO, (int)&(reg->dtp.command),command,0); //(B)
    
    //credo ci vada qualcos'altro se status!=1 ossia status!=READY
}