#include "state.h"
#include "core.h"


void dsu_socket_list_init(struct dsu_socket_list *dsu_socket) {
    
	dsu_socket->port        = 0;
    dsu_socket->fd      	= -1;
	dsu_socket->fds      	= NULL;
    

    dsu_socket->comfd       = -1;
	dsu_socket->comfd_close = -1;
    dsu_socket->comfds      = NULL;
	

    dsu_socket->readyfd		= -1;
	dsu_socket->readyfd		= -1;
    dsu_socket->ready		= 0;
    
	
	//memset(&dsu_socket->ev, 0, sizeof(struct epoll_event));

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


struct dsu_socket_list *dsu_sockets_search_port(struct dsu_socket_list *head, int port) {

    while (head != NULL) {
        if (head->port == port) return head;
        head = head->next;
    }
    
    return NULL;

}


void dsu_socket_add_fds(struct dsu_socket_list *node, int comfd, int flag) {
	
	/*	Allocate space for the new node. */
    struct dsu_fd_list *new_comfd = (struct dsu_fd_list *) malloc(sizeof(struct dsu_fd_list));
    memcpy(&new_comfd->fd, &comfd, sizeof(int));
    new_comfd->next = NULL;     // Append at the end of the list.
	
	/* 	The indirect pointer points to the address of the thing we will update. */
	struct dsu_fd_list **indirect;
	if (flag == DSU_INTERNAL_FD)
		indirect = &node->comfds;
	else
		indirect = &node->fds;
	
	/* 	Walk over the list, look for the end of the linked list. */
	while ((*indirect) != NULL)
		indirect = &(*indirect)->next;
	
	*indirect = new_comfd;
	
    return;

}


void dsu_socket_remove_fds(struct dsu_socket_list *node, int comfd, int flag) {
	
	/* 	The indirect pointer points to the address of the thing we will remove. */
	struct dsu_fd_list **indirect;
	if (flag == DSU_INTERNAL_FD)
		indirect = &node->comfds;
	else
		indirect = &node->fds;
	
	/* 	Walk over the list, look for the file descriptor. */
	while ((*indirect) != NULL && (*indirect)->fd != comfd)
		indirect = &(*indirect)->next;
	
	/* Remove it if found ... */
	if((*indirect) != NULL) {
		struct dsu_fd_list *_indirect = *indirect;
		*indirect = (*indirect)->next;
		free(_indirect);
	}

}


struct dsu_socket_list *dsu_sockets_search_fds(struct dsu_socket_list *node, int sockfd, int flag) {
	/* Either search for file descriptor that are*/    

    while (node != NULL) {
		
		
		if (flag == DSU_NON_INTERNAL_FD) {
			/*	Open connections	*/
			struct dsu_fd_list *comfds = node->fds;

			while(comfds != NULL) {
			
				if (comfds->fd == sockfd) return node;
				
				comfds = comfds->next;
			}
		}
	
		
		if (flag == DSU_MONITOR_FD) {	
			/*	Main fd */
			if (node->comfd == sockfd) return node;
		}
		
	
		if (flag == DSU_INTERNAL_FD) {
			/*	Open connections internally	*/
			struct dsu_fd_list *comfds = node->comfds;

			while(comfds != NULL) {
			
				if (comfds->fd == sockfd) return node;
				
				comfds = comfds->next;
			}
		}


		node = node->next;
    }
    
    return NULL;
}


//int dsu_is_internal_conn(int fd) {
	/*	If socket is not used for monitoring or internal connection, it is not an internal connection. */
	
//	if ( dsu_sockets_search_fds(dsu_program_state.binds, fd, DSU_MONITOR_FD) != NULL) return 1;


//	if ( dsu_sockets_search_fds(dsu_program_state.binds, fd, DSU_INTERNAL_FD) != NULL) return 1;
	

 //	return 0;

//}
