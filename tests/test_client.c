#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h>

#include <errno.h>
#include <unistd.h>
#include <sys/un.h>


int main() 
{ 
    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    
    struct sockaddr_un dsu_name;
    bzero(&dsu_name, sizeof(dsu_name));
    dsu_name.sun_family = AF_UNIX;
    strncpy(dsu_name.sun_path, "/tmp/dsu_comm", 13);

    connect(sockfd, (struct sockaddr *) &dsu_name, (socklen_t) sizeof(dsu_name));
    
    close(sockfd); 
} 




