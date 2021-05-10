#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>

#include "includes/simple_select_linker.h"


#define PORT    3000
#define MAXMSG  512


int main() {
    
	init();

}

