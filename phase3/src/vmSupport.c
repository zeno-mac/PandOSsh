
#include "../../headers/const.h"
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>

#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/initial.h"
#include "../headers/sysSupport.h"
#include "../headers/vmSupport.h"

#define FLASH_LINE_NO 4

// PoolSize = (UPROCMAX * 2) = 16 Frames
swap_t swapPool[POOLSIZE];
int swapSemaphore = 0;

// Forse meglio definirla da un'altra parte?
int holdingSwapMutex[UPROCMAX] = {0};

// Initialize the page table of U-Proc given the page table and the asi
void initPagetable(pteEntry_t *pageTable, int asid)
{
    // The first 31 entries are for the .text and .data pages  the logical address space
    for (int i = 0; i < MAXPAGES - 1; i++)
    {
        // The VPN field will be from 0x8000 to 0x8001E
        pageTable[i].pte_entryHI = (KUSEG + (i << VPNSHIFT)) | (asid << ASIDSHIFT);
        pageTable[i].pte_entryLO = DIRTYON;
    }
    // The final entry is for the U-Proc's stack page
    pageTable[31].pte_entryHI = (0xBFFFF << VPNSHIFT) | (asid << ASIDSHIFT);
    // TODO Global set to 1 or 0? Nelle specifiche segna Global set to 1 (off),
    // DIRTYON = each page is write-enabled
    // GLOBALOFF = the pages are private
    // VALIDOFF = the entry is not valid
    pageTable[31].pte_entryLO = DIRTYON;
}

// Initialize the swap pool structure
void initSwapPool()
{
    for (int i = 0; i < POOLSIZE; i++)
    {
        swapPool[i].sw_asid = -1;   // -1 = The frame is unoccupied
        swapPool[i].sw_pageNo = -1; // -1 = The page number is non valid
        swapPool[i].sw_pte = NULL;
    }
    SYSCALL(VERHOGEN, (int)&swapSemaphore, 0, 0);
}

// SEZIONE 5.1 - Mazzo

// funzione aux per ricavare il blocco dato il VPN - Virtual Page Number
// blocco 31 <=> vpn 0xBFFFF
// blocchi[0..30] <=> vpn 0x80000,0x80001,..,0x8001E
// quindi per vpn=0x80002 il blocco è il 2
int getFlashBlock(int vpn)
{
    if (vpn == 0xBFFFF)
        return 31;
    else
        return (vpn - 0x80000);
}

// funzione aux per calcolare l'addr base dei registri dei flash dev
// ogni processo utente ha un suo flash dev il cui asid va da 1..8,
// segue che i registi vanno da 0..7 = (1..8)-1
devreg_t *getFlashRegister(int asid)
{
    int devNo = asid - 1;
    if (devNo < 0 || devNo >= UPROCMAX)
        return NULL;

    // segue tabella per gestione interrupt implementata in phase2
    int devRegister = START_DEVREG + ((FLASH_LINE_NO - 3) * 0x80) + (devNo * 0x10);

    return ((devreg_t *)devRegister);
}

/* da documentazione:

(A) Write the flash device’s DATA0 field with the appropriate starting physical
address of the 4k block to be read (or written); the particular frame’s starting
address. (B) Use the NSYS5 system call to write the flash device’s COMMAND field
with the device block number (high order three bytes) and the command to read
(or write) in the lower order byte. Where the following C code can be used to
request a NSYS5: int ioStatus = SYSCALL(DOIO, int *commandAddr, int
commandValue, 0); Where the mnemonic constant DOIO has the value of -5.

*/
int readWriteFlashdrive(int asid, int vpn, int phisicalFrame, int op)
{
    int block = getFlashBlock(vpn);
    devreg_t *reg = getFlashRegister(asid);

    if (block < 0 || block >= MAXPAGES || reg == NULL)
    {
        return -1;
    }

    reg->dtp.data0 = phisicalFrame;
    // let's shift "block" up by 8bits(his size) so "op" can occupy the 4th byte
    // we'll end up un a situation like the following: [0..0][0..0][block][op]
    int command = (block << 8) | op;

    int ioStatus = SYSCALL(DOIO, (int)&(reg->dtp.command), command, 0);

    return ioStatus;
}

// SEZIONE 4.2 - Mazzo

void pager()
{
    // Obtain the pointer to the Current Process’s Support Structure: NSYS8.
    support_t *suppPtr = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    // Determine the cause of the TLB exception. The saved exception state
    // responsible for this TLB exception should be found in
    // sup_exceptState[0]’s Cause register
    int cause = suppPtr->sup_exceptState[PGFAULTEXCEPT].cause;
    cause = cause & CAUSE_EXCCODE_MASK;

    // if the Cause is a TLB-Mod exception, treat it as a program trap.
    if (cause == EXC_MOD)
    {
        programTrapHandler(suppPtr);
    }

    //Gain mutual exclusion over the Swap Pool table (NSYS3)
    SYSCALL(PASSEREN, (int)&swapSemaphore, 0, 0);
    holdingSwapMutex[suppPtr->sup_asid - 1] = 1;
    // determine the missing page number: found in the saved exception state’s
    // EntryHi
    int missingPageNo = suppPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi;
    missingPageNo = (missingPageNo & (GETSHAREFLAG | GETPAGENO)) >> VPNSHIFT;

    /*First check for an unoccupied frame before selecting an occupied frame to use.
     This will turn an O(1) operation into an O(n) operation in exchange for fewer I/O (write) operations. */
    int i=-1;
    for(int j=0; j<POOLSIZE; j++){
        if(swapPool[j].sw_asid==-1){
            i=j;
            break;
        }
    }
    // Let's use a FIFO round-robin algorithm fo swap the pages to pick a frame
    // i from the Swap Pool.
    static int fifoCont = 0;
    if(i==-1){
        i = fifoCont;
        fifoCont = (fifoCont + 1) % POOLSIZE;
    }

    int physicalFrame = FLASHPOOLSTART + (i * PAGESIZE);

    // Determine if frame i is occupied; examine entry i in the Swap Pool table
    if (swapPool[i].sw_asid != -1)
    {
        // Frame occupied: assume it is occupied by logical page number k
        // belonging to process x (ASID) and that it is “dirty”
        int x = swapPool[i].sw_asid;
        int k = swapPool[i].sw_pageNo;
        pteEntry_t *xPte = swapPool[i].sw_pte;

        int status = getSTATUS();
        setSTATUS(status & ~MSTATUS_MIE_MASK);
        // Update process x’s Page Table: mark Page Table entry k as not valid.
        xPte->pte_entryLO &= ~VALIDON;

        //(*)Update the TLB: if process x’s page k’s Page Table entry is
        // currently cached, it is clearly out of date; it was just updated in
        // the previous step.
        // Simple implementation: TLBCLR();
        setENTRYHI(swapPool[i].sw_pte->pte_entryHI);
        TLBP();
        if ((getINDEX() & 0x80000000) == 0)
        {
            setENTRYLO(swapPool[i].sw_pte->pte_entryLO);
            TLBWI();
        }

        // TODO Add the improved tlb update once the first test is done
        setSTATUS(status);

        // Write the contents of frame i to the correct location on process x’s
        // flash device
        readWriteFlashdrive(x, k, physicalFrame, FLASHWRITE);
    }
    // Read the contents of the curr proc’s flash device logical page p into
    // frame i

    int ioStatus = readWriteFlashdrive(suppPtr->sup_asid, missingPageNo, physicalFrame, FLASHREAD);

    if (ioStatus != 1)
    {
        // if status!=READY call the Trap Handler
        holdingSwapMutex[suppPtr->sup_asid - 1] = 0; // Aggiunta per luca----------------------
        SYSCALL(VERHOGEN, (int)&swapSemaphore, 0, 0);
        programTrapHandler(suppPtr);
    }

    // Update the Swap Pool table’s entry i to reflect frame i’s new contents:
    // page p belonging
    //  to the curr proc's ASID, and a ptr to the curr proc's Page Table entry
    //  for page p
    swapPool[i].sw_asid = suppPtr->sup_asid;
    swapPool[i].sw_pageNo = missingPageNo;

    // Find the index of the page table: 0..30->text/data, 31->stack
    //  blocco 31 <=> vpn 0xBFFFF
    //  blocchi[0..30] <=> vpn 0x80000,0x80001,..,0x8001E
    //  quindi per vpn=0x80002 il blocco è il 2
    int pageIdx = getFlashBlock(missingPageNo);

    int status = getSTATUS();
    setSTATUS(status & ~MSTATUS_MIE_MASK);
    swapPool[i].sw_pte = &suppPtr->sup_privatePgTbl[pageIdx];

    // Update the curr proc Page Table entry for page p to indicate it's present
    // (Valid) and occupying frame i (PFN field)
    swapPool[i].sw_pte->pte_entryLO = physicalFrame | DIRTYON | VALIDON;

    // Update the TLB. Same as before (*)
    // Simple implementation: TLBCLR();
    setENTRYHI(swapPool[i].sw_pte->pte_entryHI);
    TLBP();
    if ((getINDEX() & 0x80000000) == 0)
    {
        setENTRYLO(swapPool[i].sw_pte->pte_entryLO);
        TLBWI();
    }

    setSTATUS(status);

    holdingSwapMutex[suppPtr->sup_asid - 1] = 0; // Aggiunta per luca----------------------
    // Release mutual exclusion over the Swap Pool table
    SYSCALL(VERHOGEN, (int)&swapSemaphore, 0, 0);
    // Return control to the curr proc to retry the instruction that caused the
    // page fault: LDST on the saved exception state.
    LDST(&suppPtr->sup_exceptState[0]);
}
