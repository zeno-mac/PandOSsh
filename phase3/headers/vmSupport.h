void uTLB_RefillHandler(void);
void pager(void);
int readWriteFlashdrive(int asid, int vpn, int phisicalFrame, int op);

extern int holdingSwapMutex[UPROCMAX];