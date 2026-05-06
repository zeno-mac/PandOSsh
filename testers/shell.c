#include <uriscv/liburiscv.h>

#include "h/print.h"
#include "h/tconst.h"

void chkExitAndTerminate(char *buff) {
    if (buff[0] == 'e' && buff[1] == 'x' && buff[2] == 'i' && buff[3] == 't' &&
        buff[4] == '\0') {
        SYSCALL(TERMINATE, 0, 0, 0);
    }
}

void main() {

    struct mapAsid {
        char *name;
        int asid;
    };
    struct mapAsid arr[] = {
        {"calc", 2}, {"miao", 3}, {"meow", 4},
        {"meow", 5}, {"meow", 6}, {"meow", 7},
    };
    char *startPrompt = "> ";
    char *unknownProg = "command not found :<\n";
    char buffer[80];
    int found, excAsid;
    while (1) {
        SYSCALL(WRITETERMINAL, (int)startPrompt, 2, 0);
        // n = number of characters actually read
        int n = SYSCALL(READTERMINAL, (int)buffer, 0, 0);
        if (buffer[n - 1] == '\n') {
            buffer[n - 1] = '\0';
        }

        chkExitAndTerminate(buffer);

        found = 0;
        for (int i = 0; i < 6; i++) { // search in known program asid's
            int index = 0;

            while (arr[i].name[index] == buffer[index]) {
                if (arr[i].name[index] == '\0') {
                    found = 1;
                    excAsid = arr[i].asid;
                    break;
                }
                index++;
            }
            if (found == 1) {
                SYSCALL(EXECUTE, excAsid, 0, 0);
                break;
            }
        }
        if (found == 0) {
            SYSCALL(WRITETERMINAL, (int)unknownProg, 21, 0);
        }
    }
}
