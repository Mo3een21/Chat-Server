# Chat-Server
[Creator]: Moeen Abu Katish

[Introduction]
This project is a simple chat server implementation in C, allowing multiple clients to connect and exchange messages. The server utilizes non-blocking I/O and the select system call to handle multiple connections efficiently. This README provides an overview of the features, usage, and design of the chat server implementation.

[Functionality Overview]

==initPool(conn_pool_t* pool)==
-Initializes the connection pool structure.
-Sets initial values for the pool attributes.
-Initializes file descriptor sets for read and write operations.

==addConn(int sd, conn_pool_t* pool)==
-Adds a new connection to the connection pool.
-Allocates memory for the new connection.
-Updates the connection pool and file descriptor sets.

==removeConn(int sd, conn_pool_t* pool)==
-Removes a connection from the connection pool.
-Frees memory allocated for the connection.
-Updates the connection pool and file descriptor sets.

==cleanupConnections(conn_pool_t* pool)==
-Cleans up all open connections upon termination.
-Closes socket descriptors and frees memory.

==addMsg(int sd, char* buffer, int len, conn_pool_t* pool)==
-Adds a message to the message queues of all connections except the sender.
-Allocates memory for the message and copies the content.
-Updates the write message queues of recipient connections.

==writeToClient(int socket_descriptor, conn_pool_t* pool)==
-Writes messages from the message queue to the client.
-Converts messages to uppercase before sending.
-Clears the write set for the corresponding socket descriptor.


==intHandler(int SIG_INT)==
-Signal handler for interrupt signals (SIGINT).
-Sets a flag to end the server loop gracefully.

==main(int argc, char *argv[])==
-Entry point of the chat server application.
-Parses command-line arguments for the port number.
-Initializes the connection pool and server socket.
-Sets the server socket to non-blocking mode.
-Binds the server socket, sets the listen backlog, and initializes file descriptor sets.
-Enters a loop to handle incoming connections, data reception, and message transmission.
-Utilizes the select system call for I/O multiplexing.
-Handles incoming data from clients and distributes messages.
-Cleans up resources and closes connections upon termination.

==Features==
-Non-blocking I/O using the select system call for handling multiple connections.
-Support for adding and removing connections dynamically.
-Message distribution to all connected clients except the sender.
-Automatic conversion of messages to uppercase before transmission.
-Graceful termination handling using signal handling.

Usage:
Compile the chatServer.c source file into an executable:
gcc -o chatServer chatServer.c
./chatServer <port>
<port>: Port number for the chat server to listen on.

Example:
./chatServer 8080
This command starts the chat server on port 8080, allowing clients to connect and exchange messages.
