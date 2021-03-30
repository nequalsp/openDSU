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
    printf("test1\n");
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    
    //unlink("/tmp/dsu_comm");
    struct sockaddr_un dsu_name;
    bzero(&dsu_name, sizeof(dsu_name));
    dsu_name.sun_family = AF_UNIX;
    strncpy(dsu_name.sun_path, "/tmp/dsu_comm", 13);
    
    printf("test1\n");
    bind(sockfd, (struct sockaddr *) &dsu_name, (socklen_t) sizeof(dsu_name));
    printf("test2\n");
    listen(sockfd, 1);
    
    struct sockaddr_un client_name;
    accept(sockfd, (struct sockaddr *) &client_name, (socklen_t *) sizeof(client_name));
    
    close(sockfd); 
} 




