#include "state.h"
#include "core.h"


void dsu_socket_list_init(struct dsu_socket_list *dsu_socket) {
    
	dsu_socket->port        = 0;
    dsu_socket->fd      	= -1;
    dsu_socket->comfd       = -1;
	bzero(&dsu_socket->comfd_addr, sizeof(dsu_socket->comfd_addr));
	
	dsu_socket->monitoring	= 0;
	dsu_socket->transfer 	= 0;
	dsu_socket->version 	= 0;
	dsu_socket->locked		= 0;

	dsu_socket->fd_sem		= 0;	
	dsu_socket->status_sem	= 0;
	dsu_socket->status      = NULL;
	
}


struct dsu_socket_list *dsu_sockets_add(struct dsu_socket_list **head, struct dsu_socket_list *new_node) {  
	
	/*	Allocate space for the new node. */
	struct dsu_socket_list *node = (struct dsu_socket_list *) malloc(sizeof(struct dsu_socket_list));
    memcpy(node, new_node, sizeof(struct dsu_socket_list));
	node->next = NULL;		// 	Append at the end of the list.
	
	/* 	The indirect pointer points to the address of the thing we will update. */
	struct dsu_socket_list **indirect = head;
	
	/* 	Walk over the list, look for the end of the linked list. */
	while ((*indirect) != NULL)
		indirect = &(*indirect)->next;
	
	return *indirect = node;

}


void dsu_sockets_remove_fd(struct dsu_socket_list **head, int sockfd) {
	
	/* 	The indirect pointer points to the address of the thing we will remove. */
	struct dsu_socket_list **indirect = head;
	
	/* 	Walk over the list, look for the file descriptor. */
	while ((*indirect) != NULL && (*indirect)->fd != sockfd)
		indirect = &(*indirect)->next;
	
	/* Remove it if found ... */
	if((*indirect) != NULL) {
		struct dsu_socket_list *_indirect = *indirect;
		*indirect = (*indirect)->next;
		free(_indirect);
	}
        
}


struct dsu_socket_list *dsu_sockets_transfer_fd(struct dsu_socket_list **dest, struct dsu_socket_list **src, struct dsu_socket_list *dsu_socketfd) {   
    struct dsu_socket_list *new_dsu_socketfd = dsu_sockets_add(dest, dsu_socketfd);
    dsu_sockets_remove_fd(src, dsu_socketfd->fd);
    return new_dsu_socketfd;
}


struct dsu_socket_list *dsu_sockets_search_fd(struct dsu_socket_list *head, int sockfd) {
    
    while (head != NULL) {
        if (head->fd == sockfd) return head;
        head = head->next;
    }
    
    return NULL;
}
