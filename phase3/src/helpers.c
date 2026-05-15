#include "../headers/helpers.h"
#include "../../headers/const.h"
#include <uriscv/liburiscv.h>
/*
 * These handlers are implemented in other Phase 3 modules.
 *
 * pager:
 *   Support Level TLB exception handler. It handles page faults.
 *
 * generalExceptionHandler:
 *   Support Level general exception handler. It handles positive syscalls
 *   and Program Trap exceptions.
 */
extern void pager(void);
extern void generalExceptionHandler(void);
/*
 * Initial status for U-procs:
 * - user mode enabled;
 * - global interrupt enable bit set.
 *
 * The mie register is initialized separately with MIE_ALL.
 */
#define UPROC_STATUS (MSTATUS_MPP_U | MSTATUS_MIE_MASK)

/*
 * Status used by Support Level exception handlers:
 * - kernel mode enabled;
 * - global interrupt enable bit set.
 *
 * Support Level handlers execute in kernel mode even if the interrupted
 * U-proc was executing in user mode.
 */
#define SUPPORT_STATUS (MSTATUS_MPP_M | MSTATUS_MIE_MASK)

/*
 * Index of the last entry in the private page table.
 *
 * Entries 0..30 are used for text/data pages.
 * Entry 31 is used for the user stack page.
 */
#define STACK_PAGE_INDEX (USERPGTBLSIZE - 1)

/*
 * Last valid index of the internal stacks stored inside support_t.
 *
 * Since stacks grow downward, the initial stack pointer must point to the
 * end of the allocated stack area.
 */
#define SUPPORT_STACK_LAST_INDEX 499

/*
 * Clears all fields of a processor state.
 *
 * This function manually resets the state_t structure instead of using
 * memset, keeping the code explicit and consistent with the rest of the
 * project. All general purpose registers are also reset to zero.
 */
static void clearState(state_t *state) {
  state->entry_hi = 0;
  state->cause = 0;
  state->status = 0;
  state->pc_epc = 0;
  state->mie = 0;

  for (int i = 0; i < STATE_GPR_LEN; i++) {
    state->gpr[i] = 0;
  }
}

/*
 * Builds the EntryHI value for a private page belonging to a U-proc.
 *
 * EntryHI contains:
 * - the VPN, extracted from the virtual address;
 * - the private segment flag;
 * - the ASID of the process.
 *
 * The resulting value is later stored inside the private page table entry
 * and used by the TLB refill handler when loading entries into the TLB.
 */
static unsigned int makePrivateEntryHI(memaddr virtualAddress, int asid) {
  unsigned int vpn = virtualAddress & GETPAGENO;
  unsigned int privateSegment = PRIVATE << SHAREDSEGFLAG;
  unsigned int processAsid = asid << ASIDSHIFT;

  return vpn | privateSegment | processAsid;
}

/*
 * Initializes a single private page table entry.
 *
 * At process creation time, no user page is loaded in RAM yet. For this
 * reason, the entry is initialized as not valid. The dirty bit is set so
 * that the page is considered writable once it is loaded by the pager.
 *
 * The physical frame number is not set here, because it will be filled
 * later by the pager when the corresponding page is actually brought into
 * the Swap Pool.
 */
static void initPageTableEntry(pteEntry_t *entry, memaddr virtualAddress,
                               int asid) {
  entry->pte_entryHI = makePrivateEntryHI(virtualAddress, asid);
  entry->pte_entryLO = DIRTYON;
}

/*
 * Initializes the private page table of a U-proc.
 *
 * Each U-proc owns a private page table with 32 entries:
 * - entries 0..30 map the text/data logical pages, starting from KUSEG;
 * - entry 31 maps the user stack page.
 *
 * All entries are initially invalid because pages are loaded on demand by
 * the pager after a page fault.
 */
static void initPageTable(support_t *support, int asid) {
  for (int i = 0; i < STACK_PAGE_INDEX; i++) {
    memaddr virtualAddress = KUSEG + (i * PAGESIZE);
    initPageTableEntry(&support->sup_privatePgTbl[i], virtualAddress, asid);
  }

    /*
     * The user stack starts at USERSTACKTOP and grows downward.
     * Therefore, the actual stack page starts one page before USERSTACKTOP.
     *
     * Example:
     * 0xC0000000 - 0x1000 = 0xBFFFF000
     */
  memaddr stackPageAddress = USERSTACKTOP - PAGESIZE;

  initPageTableEntry(&support->sup_privatePgTbl[STACK_PAGE_INDEX],
                     stackPageAddress, asid);
}


/*
 * Initializes the starting processor state of a U-proc.
 *
 * This is the state passed to the Nucleus CREATEPROCESS syscall.
 * The U-proc starts execution at UPROCSTARTADDR, uses USERSTACKTOP as its
 * initial user stack pointer, runs in user mode and has interrupts enabled.
 *
 * The ASID is stored in EntryHI so that address translation can distinguish
 * this process from the other U-procs.
 */
static void initInitialProcessorState(state_t *state, int asid) {
  clearState(state);

  state->pc_epc = UPROCSTARTADDR;
  state->reg_sp = USERSTACKTOP;

  state->status = UPROC_STATUS;
  state->mie = MIE_ALL;

  state->entry_hi = asid << ASIDSHIFT;
}

/*
 * Initializes the Support Level context used for TLB exceptions.
 *
 * When the Nucleus passes up a TLB exception using the PGFAULTEXCEPT index,
 * execution will continue at pager(). The stack pointer is set to the end
 * of the TLB exception stack stored inside the Support Structure.
 */
static void initPageFaultContext(support_t *support) {
  support->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)pager;

  support->sup_exceptContext[PGFAULTEXCEPT].stackPtr =
      (memaddr)&support->sup_stackTLB[SUPPORT_STACK_LAST_INDEX];

  support->sup_exceptContext[PGFAULTEXCEPT].status = SUPPORT_STATUS;
}

/*
 * Initializes the Support Level context used for general exceptions.
 *
 * General exceptions include:
 * - positive syscalls;
 * - Program Trap exceptions.
 *
 * When the Nucleus passes up one of these exceptions using the
 * GENERALEXCEPT index, execution will continue at generalExceptionHandler().
 * The stack pointer is set to the end of the general exception stack stored
 * inside the Support Structure.
 */
static void initGeneralExceptionContext(support_t *support) {
  support->sup_exceptContext[GENERALEXCEPT].pc =
      (memaddr)generalExceptionHandler;

  support->sup_exceptContext[GENERALEXCEPT].stackPtr =
      (memaddr)&support->sup_stackGen[SUPPORT_STACK_LAST_INDEX];

  support->sup_exceptContext[GENERALEXCEPT].status = SUPPORT_STATUS;
}

/*
 * Initializes all the data needed to create a U-proc.
 *
 * This public helper is called by createUProc(). It prepares both the initial
 * processor state passed to CREATEPROCESS and the Support Structure used by
 * the Support Level.
 *
 * In particular, it initializes:
 * - the ASID stored in the Support Structure;
 * - the initial processor state of the U-proc;
 * - the context for page fault exceptions;
 * - the context for general exceptions;
 * - the private page table.
 *
 * Invalid parameters are treated as fatal kernel errors because this function
 * is part of the Support Level initialization path.
 */
void initUProc(int asid, state_t *state, support_t *support) {
  if (asid <= 0 || asid > UPROCMAX || state == NULL || support == NULL) {
    PANIC();
  }

  support->sup_asid = asid;

  initInitialProcessorState(state, asid);
  initPageFaultContext(support);
  initGeneralExceptionContext(support);
  initPageTable(support, asid);
}
