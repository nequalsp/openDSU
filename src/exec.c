#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define LD_PRELOAD_LIB "LD_PRELOAD=$LD_PRELOAD:/usr/local/lib/libopenDSU.so"


int main (int argc, char *argv[]) {
    
    
    /* Determine the length of the argument. */
    int size = 0;
    for (int i = 1; i < argc; ++i) {
        size += strlen(argv[i]) + 1;
    }
    size += strlen(LD_PRELOAD_LIB) + 1;   
    size += 1;

    /* Reconstruct the arguments with the LD_PRELOAD. */
    char command[size]; int idx = 0;
    
    strcpy(&command[idx], LD_PRELOAD_LIB);
    idx += strlen(LD_PRELOAD_LIB);
    strcpy(&command[idx], " ");    
    idx += 1;
    
    for (int i = 1; i < argc; ++i) {
        strcpy(&command[idx], argv[i]);
        idx += strlen(argv[i]);
        strcpy(&command[idx], " ");    
        idx += 1;    
    }
    
    /* Execute the command. */
    if (system(command) == -1)
        perror("DSU");
    
    return 0;
} 
