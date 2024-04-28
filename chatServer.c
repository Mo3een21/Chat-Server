#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <errno.h>
#include "chatServer.h"
/*
 * Init the conn_pool_t structure.
 * @pool - allocated pool
 * @ return value - 0 on success, -1 on failure
 */
int initPool(conn_pool_t* pool) {
    if (pool == NULL)
        return -1;

    pool->maxfd = -1;
    pool->nready = 0;
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->ready_write_set);
    pool->conn_head = NULL;
    pool->nr_conns = 0;

    return 0;
}

/*
 * Add connection when new client connects the server.
 * @ sd - the socket descriptor returned from accept
 * @pool - the pool
 * @ return value - 0 on success, -1 on failure
 */
int addConn(int sd, conn_pool_t* pool) {
    if (pool == NULL)
        return -1;

    //allocate memory for the new connection
    conn_t* new_conn = (conn_t*)malloc(sizeof(conn_t));
    if (new_conn == NULL)
        return -1;

    //initialize connection fields
    new_conn->fd = sd;
    new_conn->write_msg_head = NULL;
    new_conn->write_msg_tail = NULL;
    new_conn->prev = NULL; // Initialize prev pointer to NULL
    new_conn->next = NULL; // Initialize next pointer to NULL

    //add the new connection to the pool
    new_conn->next = pool->conn_head;
    if (pool->conn_head != NULL)
        pool->conn_head->prev = new_conn;
    pool->conn_head = new_conn;
    pool->nr_conns++;

    //update maxfd if needed
    if (sd > pool->maxfd)
        pool->maxfd = sd;
    FD_SET(sd,&pool->read_set);
    return 0;
}


/*
 * Remove connection when a client closes connection, or clean memory if server stops.
 * @ sd - the socket descriptor of the connection to remove
 * @pool - the pool
 * @ return value - 0 on success, -1 on failure
 */


int removeConn(int sd, conn_pool_t* pool) {
    if (pool == NULL)
        return -1;

    conn_t* current_conn = pool->conn_head;
    while (current_conn != NULL) {
        if (current_conn->fd == sd) {
            //remove connection from the pool
            if (current_conn->prev != NULL)
                current_conn->prev->next = current_conn->next;
            if (current_conn->next != NULL)
                current_conn->next->prev = current_conn->prev;
            if (pool->conn_head == current_conn)
                pool->conn_head = current_conn->next;

            //update maxfd if needed
            if (sd == pool->maxfd) {
                pool->maxfd = -1;
                conn_t* temp_conn = pool->conn_head;
                while (temp_conn != NULL) {
                    if (temp_conn->fd > pool->maxfd)
                        pool->maxfd = temp_conn->fd;
                    temp_conn = temp_conn->next;
                }
            }
            pool->nr_conns--;
            //free memory for the connection
            free(current_conn);
            break;
        }
        current_conn = current_conn->next;
    }
    printf("removing connection with sd %d \n", sd);
    //remove from sets (if necessary)
    FD_CLR(sd, &pool->read_set);
    FD_CLR(sd, &pool->write_set);
    return 0;
}

void cleanupConnections(conn_pool_t* pool) {
    conn_t* current_connection = pool->conn_head;
    while (current_connection != NULL) {
        int socket_descriptor_to_close;
        if (current_connection->next != NULL) {
            current_connection = current_connection->next;
            socket_descriptor_to_close = current_connection->prev->fd;
        } else {
            socket_descriptor_to_close = current_connection->fd;
            current_connection = NULL;
        }

        removeConn(socket_descriptor_to_close, pool);
        close(socket_descriptor_to_close);

    }
}
/*
 * Add msg to the queues of all connections (except of the origin).
 * @ sd - the socket descriptor of the origin client
 * @ buffer - the msg to add
 * @ len - length of msg
 * @pool - the pool
 * @ return value - 0 on success, -1 on failure
 */

int addMsg(int sd, char* buffer, int len, conn_pool_t* pool) {
    if (pool == NULL || buffer == NULL)
        return -1;

    //iterate through connections in the pool
    conn_t* current_conn = pool->conn_head;
    while (current_conn != NULL) {
        //check if the connection is not the sender's connection
        if (current_conn->fd != sd) {
            //create a new msg
            msg_t* new_msg = (msg_t*)malloc(sizeof(msg_t));
            if (new_msg == NULL)
                return -1;

            //allocate memory for the msg and copy the content
            new_msg->message = (char*)malloc((len + 1) * sizeof(char)); // +1 for null terminator
            if (new_msg->message == NULL) {
                free(new_msg);
                return -1;
            }
            strncpy(new_msg->message, buffer, len);
            new_msg->message[len] = '\0'; // Null-terminate the message
            new_msg->size = len;
            new_msg->next = NULL; // Initialize next pointer to NULL

            //add the msg to the connection write msg queue
            if (current_conn->write_msg_head == NULL) {
                current_conn->write_msg_head = new_msg;
                current_conn->write_msg_tail = new_msg;
            } else {
                current_conn->write_msg_tail->next = new_msg;
                new_msg->prev = current_conn->write_msg_tail;
                current_conn->write_msg_tail = new_msg;
            }

            //set the file descriptor to be checked for write readiness
            FD_SET(current_conn->fd, &pool->write_set);
        }
        current_conn = current_conn->next;
    }

    return 0;
}

/*
 * Write msg to client.
 * @ sd - the socket descriptor of the connection to write msg to
 * @pool - the pool
 * @ return value - 0 on success, -1 on failure
 */

int writeToClient(int socket_descriptor, conn_pool_t* pool) {
    if (pool == NULL)
        return -1;
    //find the connection corresponding to the given socket descriptor
    conn_t* current_connection = pool->conn_head;
    while (current_connection != NULL) {
        if (current_connection->fd == socket_descriptor) {
            //iterate through the msg queue and write messages to the client
            msg_t* current_message = current_connection->write_msg_head;
            while (current_message != NULL) {
                //convert msg to uppercase
                   int i=0;
                   while(i<current_message->size){
                       current_message->message[i]= toupper(current_message->message[i]);
                       i++;
                   }
                if (write(socket_descriptor, current_message->message, current_message->size) < 0) {
                    //error handling:writing failed
                    perror("Error writing to client");
                    return -1;
                } else {
                    msg_t* next_message = current_message->next; // Store next message before freeing current_message
                    free(current_message->message);
                    free(current_message);
                    current_message = next_message; // Move to next message

                    if (current_message == NULL) {
                        break; // Exit loop if there are no more messages
                    }
                }
            }
            //clear the write set for this descriptor
            FD_CLR(socket_descriptor, &pool->write_set);
            current_connection->write_msg_head = NULL;
            current_connection->write_msg_tail = NULL;
            return 0;
        }
        current_connection = current_connection->next;
    }

    return -1;
}


static int end_server = 0;

void intHandler(int SIG_INT) {
    /* use a flag to end_server to break the main loop */
    end_server = 1;
}

int main (int argc, char *argv[])
{
    signal(SIGINT, intHandler);
    int port = atoi(argv[1]);
//    int port=8088;
    if(port<1 || port >65536 || argc!=2){
        printf("Usage: server <port>\n");
        exit(0);
    }
    conn_pool_t* pool = malloc(sizeof(conn_pool_t));
    if (pool == NULL) {
        perror("Error: Unable to allocate memory for connection pool");
        exit(EXIT_FAILURE);
    }
    initPool(pool);

    /*************************************************************/
    /* Create an AF_INET stream socket to receive incoming      */
    /* connections on                                            */
    /*************************************************************/
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating server socket");
        exit(EXIT_FAILURE);
    }

    /*************************************************************/
    /* Set socket to be nonblocking. All of the sockets for      */
    /* the incoming connections will also be nonblocking since   */
    /* they will inherit that state from the listening socket.   */
    /*************************************************************/
    int on = 1;
    if (ioctl(server_socket, FIONBIO, (char *)&on) == -1) {
        perror("Error setting socket to non-blocking");
        exit(EXIT_FAILURE);
    }

    /*************************************************************/
    /* Bind the socket                                           */
    /*************************************************************/
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
    server_addr.sin_port = htons(port); // PORT is the port number defined in chatServer.h

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding server socket");
        exit(EXIT_FAILURE);
    }

    /*************************************************************/
    /* Set the listen back log                                   */
    /*************************************************************/
    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("Error setting listen backlog");
        exit(EXIT_FAILURE);
    }

    /*************************************************************/
    /* Initialize fd_sets  			                             */
    /*************************************************************/
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->write_set);
    FD_SET(server_socket, &pool->read_set);
    /*************************************************************/
    /* Loop waiting for incoming connects, for incoming data or  */
    /* to write data, on any of the connected sockets.           */
    /*************************************************************/
    do {
        fd_set tmp_read_set = pool->read_set;
        fd_set tmp_write_set = pool->write_set;
        pool->maxfd=server_socket;

        conn_t *last_client = pool->conn_head;
        while(last_client != NULL){
            if(pool->maxfd < last_client->fd){
                pool->maxfd = last_client->fd;
            }
            last_client = last_client->next;
        }

        pool->ready_read_set = pool->read_set;
        tmp_read_set = pool->ready_read_set;
        pool->ready_write_set = pool->write_set;
        tmp_write_set = pool->ready_write_set;
        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
        /**********************************************************/
        /* Call select() 										  */
        /**********************************************************/
        int num_ready = select(pool->maxfd + 1, &tmp_read_set, &tmp_write_set, NULL, NULL);
        if (num_ready == -1) {
            if (errno == EINTR)
                continue;
            perror("Error in select");
            break;
        }

        /**********************************************************/
        /* One or more descriptors are readable or writable.      */
        /* Need to determine which ones they are.                 */
        /**********************************************************/

        for (int i = 0; i <= pool->maxfd; i++) {
            /* Each time a ready descriptor is found, one less has  */
            /* to be looked for.  This is being done so that we     */
            /* can stop looking at the working set once we have     */
            /* found all of the descriptors that were ready         */

            /*******************************************************/
            /* Check to see if this descriptor is ready for read   */
            /*******************************************************/
            if (FD_ISSET(i, &tmp_read_set)) {
                if (i == server_socket) {
                    /****************************************************/
                    /* A descriptor was found that was readable         */
                    /* This is the listening socket, accept one         */
                    /* incoming connection that is queued up on the     */
                    /* listening socket before we loop back and call    */
                    /* select again.                                    */
                    /****************************************************/
                    int client_socket = accept(server_socket, NULL, NULL);
                    if (client_socket == -1) {
                        perror("Error accepting connection");
                        continue;
                    }
                    if (addConn(client_socket, pool) == -1) {
                        perror("Error adding connection to pool");
                        close(client_socket);
                        continue;
                    }
                    printf("New incoming connection on sd %d\n", client_socket);
                } else {
                    /****************************************************/
                    /* If this is not the listening socket, an          */
                    /* existing connection must be readable             */
                    /* Receive incoming data on this socket             */
                    /****************************************************/
                    char buffer[BUFFER_SIZE];
                    int bytes_received = read(i, buffer, BUFFER_SIZE);
                    if (bytes_received == -1) {
                        perror("Error reading from client");
                        continue;
                    } else if (bytes_received == 0) {
                        // Connection closed by client
                        printf("Connection closed for sd %d\n", i);
                        removeConn(i, pool);
                        close(i);
                        continue;
                    } else {
                        printf("descriptor %d is readable\n",i);
                        buffer[bytes_received] = '\0'; // Null-terminate the buffer
                        printf("%d bytes received from sd %d\n", bytes_received, i);
                        addMsg(i, buffer, bytes_received, pool);
                    }
                }
            } /* End of if (FD_ISSET()) */

            /*******************************************************/
            /* Check to see if this descriptor is ready for write  */
            /*******************************************************/
            if (FD_ISSET(i, &tmp_write_set)) {
                /* try to write all msgs in queue to sd */
                writeToClient(i, pool);
            }
            /*******************************************************/
        } /* End of loop through selectable descriptors */
    } while (end_server == 0);

    /*************************************************************/
    /* If we are here, Control-C was typed,						 */
    /* clean up all open connections					         */
    /*************************************************************/



    cleanupConnections(pool);
    close(server_socket);
    //free memory
    free(pool);

    return 0;
}





