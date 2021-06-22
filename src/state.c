#include "core.h"


struct dsu_socket_struct *dsu_sockets_add(struct dsu_socket_struct **head, struct dsu_socket_struct *new_node) {  

	
	struct dsu_socket_struct *node = (struct dsu_socket_struct *) malloc(sizeof(struct dsu_socket_struct));
    memcpy(node, new_node, sizeof(struct dsu_socket_struct));
    
	
    /*  First node of the list. */
    if (*head == NULL) {
        node->next = NULL;
        *head = node;
        return node;
    }        
	

    /*  Add to the last node of non-empty list. */
    while ((*head)->next != NULL) {
        *head = (*head)->next;
    }
    (*head)->next = node;
	
	
    return node;

}


void dsu_sockets_remove_fd(struct dsu_socket_struct **head, int sockfd) {
	
    /*  Empty list. */
    if (*head == NULL) return;
    
    /*  List of size 1. */
    if ((*head)->next == NULL) {
        if ((*head)->fd == sockfd) {
            free(*head);
            *head = NULL;
            return;
        } else
            return;
    };    
    
    /*  List of size > 1. */							// Does this work??
    struct dsu_socket_struct *prev_socket = *head;
    struct dsu_socket_struct *cur_socket = (*head)->next;
    
    while (cur_socket != NULL) {
        if (cur_socket->fd == sockfd) {
            prev_socket->next = cur_socket->next;
            free(cur_socket);
            return;
        }
        prev_socket = cur_socket;
        cur_socket = cur_socket->next;
    }
        
}


struct dsu_socket_struct *dsu_sockets_transfer_fd(struct dsu_socket_struct **dest, struct dsu_socket_struct **src, struct dsu_socket_struct *dsu_socketfd) {   
    struct dsu_socket_struct *new_dsu_socketfd = dsu_sockets_add(dest, dsu_socketfd);
    dsu_sockets_remove_fd(src, dsu_socketfd->fd);
    return new_dsu_socketfd;
}


struct dsu_socket_struct *dsu_sockets_search_fd(struct dsu_socket_struct *head, int sockfd) {
    
    while (head != NULL) {
        if (head->fd == sockfd) return head;
        head = head->next;
    }
    
    return NULL;
}


struct dsu_socket_struct *dsu_sockets_search_port(struct dsu_socket_struct *head, int port) {

    while (head != NULL) {
        if (head->port == port) return head;
        head = head->next;
    }
    
    return NULL;

}


void dsu_socket_add_comfd(struct dsu_socket_struct *head, int comfd) {
       
    struct dsu_comfd_struct **comfds = &head->comfds;

    struct dsu_comfd_struct *new_comfd = (struct dsu_comfd_struct *) malloc(sizeof(struct dsu_comfd_struct));
    memcpy(&new_comfd->fd, &comfd, sizeof(int));
    new_comfd->next = NULL;     // Append at the end of the list.
    
    /*  First node of the list. */
    if (*comfds == NULL) {
        *comfds = new_comfd;
        return;
    }        

    /*  Add to the last node of non-empty list. */
    while ((*comfds)->next != NULL) {
        *comfds = (*comfds)->next;
    }
    (*comfds)->next = new_comfd;
	
    return;

}


void dsu_socket_remove_comfd(struct dsu_socket_struct *head, int comfd) {

    struct dsu_comfd_struct **comfds = &head->comfds;

    /*  Empty comfds. */
    if (*comfds == NULL) return;
    
    /*  List of size 1. */
    if ((*comfds)->next == NULL) {
        if ((*comfds)->fd == comfd) {
            free(*comfds);
            *comfds = NULL;
            return;
        } else
            return;
    };    
    
    /*  List of size > 1. */
    struct dsu_comfd_struct **prev_comfds = comfds;
    struct dsu_comfd_struct **cur_comfds = &(*comfds)->next;
    
    while (*cur_comfds != NULL) {
        if ((*cur_comfds)->fd == comfd) {
            (*prev_comfds)->next = (*cur_comfds)->next;
            free(*cur_comfds);
            return;
        }
        prev_comfds = cur_comfds;
        cur_comfds = &(*cur_comfds)->next;
    }

}


struct dsu_socket_struct *dsu_sockets_search_comfd(struct dsu_socket_struct *head, int sockfd, int flag) {
	/*	List in list. */    

    while (head != NULL) {
		
		if (flag == DSU_MONITOR)
			if (head->comfd == sockfd && head->monitoring) return head;
		
		if (flag == DSU_NONMONITOR) {
			struct dsu_comfd_struct *comfds = head->comfds;

			while(comfds != NULL) {
			
				if (comfds->fd == sockfd) return head;
				
				comfds = comfds->next;
			}
			
		}

		head = head->next;
    }
    
    return NULL;
}


int dsu_shadowfd(int sockfd) {
    /*  If sockfd is in the list of binded sockets, return the shadowfd. If not in the list, it is not a
        socket that is binded to a port. */
    
    struct dsu_socket_struct *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
    if (dsu_sockfd != NULL)
        return dsu_sockfd->shadowfd;
    
    return sockfd;
}
