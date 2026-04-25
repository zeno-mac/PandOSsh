
#include "../../headers/const.h"    
#include <uriscv/liburiscv.h>       

#include "../headers/vmSupport.h"
#include "../../headers/types.h"
#include "../../headers/const.h"

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


//SEZIONE 5.1 - Mazzo

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