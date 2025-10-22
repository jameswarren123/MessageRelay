
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include "linkedlist.h"
#include <semaphore.h>

#define BUFFER_SIZE 2048

// Client Data Structure
typedef struct
{
	int id;
    char ipaddr[INET_ADDRSTRLEN];
    int clientFD;
    int messageFD;
	int port;
    bool active;
	time_t offlineTimestamp;
    time_t connectTime;
} client_t;

// Client Initializer
// client_t* createClient(int id, char *ip_address, int port) {
client_t* createClient(int id, char *ip_address, int port, int FD, int msgFD) {
    client_t* newClient = (client_t*)malloc(sizeof(client_t));
    newClient->id = id;
    strcpy(newClient->ipaddr, ip_address);
    newClient->active = true;
    newClient->offlineTimestamp = 0;
    newClient->connectTime = time(NULL);

    newClient->port = port;
    newClient->clientFD = FD;
    newClient->messageFD = msgFD;

    return newClient;
}

// Message Data Structure
typedef struct {
    char message[BUFFER_SIZE];
    time_t messageTime;
} message_t;

// Message Initializer
message_t* createMessage(char *input_message, time_t time) {
    message_t* newMessage = (message_t*)malloc(sizeof(message_t));
    strcpy(newMessage->message, input_message);
    newMessage->messageTime = time;
    return newMessage;
}

// BUG SOMEWHERE HERE OR IN POPHEAD
void deleteClient(list_t *list, int fd) {
	node_t *currentNode = list->head;
	client_t *currentClient = (client_t *)(currentNode->data);
	if(currentClient->clientFD == fd) {
		popHead(list);
		return;
	}
	while(currentNode != NULL) {
		client_t *currentClient = (client_t *)(currentNode->data);
		node_t *nextNode = currentNode->next;
		if(nextNode == NULL) {
			return;
		}
		if(((client_t *)(nextNode->data))->clientFD == fd) {
			currentNode->next = nextNode->next;
			free(nextNode->data);
			free(nextNode);
			return;
		}
		currentNode = currentNode -> next;
	}
}


// -=[Important Variables]=-
list_t *clientList;
list_t *messageList;
sem_t messageFlag;
int messageLifetime;
int socketFD;

// Opens the socket for connections
void open_socket(int port) {
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if( socketFD < 0 ) {
        perror( "Socket Creation Failed" );
        exit(EXIT_FAILURE);
    }
    setsockopt( socketFD, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int) );
    setsockopt( socketFD, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int) );

    struct sockaddr_in sockaddr;
    memset(&sockaddr, '\0', sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if( bind(socketFD,(struct sockaddr*) &sockaddr, sizeof(sockaddr)) < 0 ) {
        perror("Bind Failed");
        exit(EXIT_FAILURE);
    }

    if( listen(socketFD, 1) < 0 ) {
        perror("Listen Failed");
        exit(EXIT_FAILURE);
    }
    return;
}

typedef struct
{
    int fd;
    char ipaddr[INET_ADDRSTRLEN];
} clientDataStruct;

// Connects to the specified address and port and returns the FD
int create_connection(char *address, int port) {
    int dataFD = socket(AF_INET, SOCK_STREAM, 0);
    if( dataFD < 0 ) {
        perror("Socket Creation Failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in sockaddr;
    memset(&sockaddr, '\0', sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    inet_pton(AF_INET, address, &sockaddr.sin_addr);

    if( connect(dataFD, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) <0 ) {
        perror("Create Connection Fail");
        exit(EXIT_FAILURE);
    }

    return dataFD;
}

void* handleCommands(void* args) {
    clientDataStruct* newClientStruct = (clientDataStruct *)args;
    client_t* client;
	int clientFD = newClientStruct->fd;
    char ip_address[INET_ADDRSTRLEN];
    int messageFD;
    strcpy(ip_address, newClientStruct->ipaddr);
	while (1)
	{
		int readAmount = 0;
		char buf[BUFFER_SIZE];
		memset(buf, 0, BUFFER_SIZE);
		readAmount = read(clientFD, buf, BUFFER_SIZE-1);
        buf[readAmount] = '\0';
		if (readAmount == 0) {
			return NULL;
		}

        printf("%s\n", buf);
        fflush(stdout);
        // write(1, buf, strlen(buf));
        // write(1, "\n", sizeof("\n"));

		// Seperates the first word of the string out.
		char *command = strtok(buf, " ");

		if (strcmp(command, "register") == 0)
		{
            int ID = atoi(strtok(NULL, " "));
            int port = atoi(strtok(NULL, ""));
            printf("ID: %d, Port: %d, Address: %s\n", ID, port, ip_address);
            fflush(stdout);
            messageFD = create_connection(ip_address, port);

			// Adds client to client list.
			client = createClient(ID, ip_address, port, clientFD, messageFD);
            node_t* node = createNode((void *) client, sizeof(client_t));
			add(clientList, node);
		}
		else if (strcmp(command, "deregister") == 0)
		{
			// Removes client from the client list.
            char *param = strtok(NULL, "");
            deleteClient(clientList, clientFD);
            close(messageFD);
            shutdown(messageFD, SHUT_RDWR);
		}
		else if (strcmp(command, "disconnect") == 0)
		{
			// make multicast thread dormant or dead releasing port param from register
			// will send messages after reconnect within temporal constraint
			// Maybe remove client from list and add to secondary temporary holding list.
			// Set offlineTimestamp.
            client->offlineTimestamp = time(NULL);
            client->active = false;
            close(messageFD);
            shutdown(messageFD, SHUT_RDWR);
		}
		else if (strcmp(command, "reconnect") == 0)
		{
			// spin off thread to send multicast messages on port param
			// receive messages from disconnect period subject to temporal constraint
			// Set client back to active.
			// set offlineTimestamp back to 0.
            // printf("Reconnecting: ID: %d, Port: %d, Address: %s\n", client->id, client->port, client->ipaddr);
            int ID = atoi(strtok(NULL, " "));
            int port = atoi(strtok(NULL, ""));
            messageFD = create_connection(client->ipaddr, port);
            client->active = true;
            // client->offlineTimestamp = 0;
            client->port = port;
            if(messageList->head != NULL) {
                message_t *currentMessageItem = (message_t *)(messageList->head->data);
                while( time(NULL) - currentMessageItem->messageTime > messageLifetime ) {
                    // printf("Checking: %s\n", currentMessageItem->message);
                    popHead(messageList);
                    if(isListEmpty(messageList)) {
                        break;
                    }
                    currentMessageItem = (message_t *)(messageList->head->data);
                }
                sleep(1);
                node_t *currentMessageNode = messageList->head;
                while(currentMessageNode != NULL) {
                    // The commented out one sent all from the existance of the client. Changed to anything newer than when it went offline.
                    // if(((message_t *)(currentMessageNode->data))->messageTime >= client->connectTime) {
                    if(((message_t *)(currentMessageNode->data))->messageTime >= client->offlineTimestamp) {
                        printf("Sending: %s\n", ((message_t *)(currentMessageNode->data))->message);
                        write(messageFD, (((message_t *)(currentMessageNode->data))->message) , strlen(((message_t *)(currentMessageNode->data))->message));
                        write(messageFD, "^", sizeof("^"));
                    }
                    currentMessageNode = currentMessageNode->next;
                }
                client->offlineTimestamp = 0;
            }
		}
		else if (strcmp(command, "msend") == 0)
		{
            char *param = strtok(NULL, "");
            write(messageFD, "ack0^", strlen("ack0^"));
            message_t *newMessage = createMessage(param, time(NULL));
            node_t *newMessageNode = createNode((void *) newMessage, sizeof(message_t));
            add(messageList, newMessageNode);
			sem_post(&messageFlag);
		}
	}
}

// Handles all incoming connections by creating a new thread to accept them.
void handle_connections() {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrlen = sizeof(clientAddr);

    int clientDataFD;

    // Loops forever.
    while(1) {
        // Waits for connections from the data and socket ports
        if((clientDataFD = accept(socketFD, (struct sockaddr*) &clientAddr, &clientAddrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }
        write(1, "Client Connected to Port\n", sizeof("Client Connected to Port\n"));

		// Spins off a new thread to handle all the client commands.
        clientDataStruct newClientStruct = {clientDataFD, ""};
        inet_ntop(AF_INET, &clientAddr.sin_addr, newClientStruct.ipaddr, sizeof(newClientStruct.ipaddr));
        pthread_t thread;
		pthread_create(&thread, NULL, handleCommands, (void*)&newClientStruct);
    }
}

// Whenever it is signaled by sem_signal(messageFlag), it'll send the last message in the list to every active client.
void* handle_messages() {
    while(1) {
        sem_wait(&messageFlag);
        node_t *currentNode = clientList->head;
        // THERE IS CURRENTLY AN INFINITE LOOP BEING MADE HERE. IF YOU HAVE TWO CLIENTS STARTED UP. REGISTER 1. REGISTER 2. DEREGISTER 1. REGISTER 1. SEND A MESSAGE
        // PROBABLY IN OUR POP SPECIFIC CLIENT.
        while(currentNode != NULL) {
            client_t *currentClient = (client_t *)(currentNode->data);
            if(currentClient->active) {
                printf("Sending message: %s\n", ((message_t *)(messageList->tail->data))->message);
                fflush(stdout);
                write(currentClient->messageFD, ((message_t *)(messageList->tail->data))->message, strlen(((message_t *)(messageList->tail->data))->message));
            }
            currentNode = currentNode->next;
        }
    }
}

int main(int argc, char *argv[])
{
    sem_init(&messageFlag, 0, 0);
	int port;
	if (argc < 2)
	{
		printf("Usage: ./coordinator CONFIGFILE.txt\n");
		return 1;
	}

	FILE *file = fopen(argv[1], "r");
	if (file == NULL)
	{
		printf("No file\n");
		return 1;
	}
	if (fscanf(file, "%d %d", &port, &messageLifetime) == 2)
	{
		printf("Server Port = %d\n", port);
		printf("Message Lifetime (s) = %d\n", messageLifetime);
	}
	else
	{
		printf("Error: Failed to read two integers from file\n");
		fclose(file);
		return 1; // Exit with error code
	}
	fclose(file);
    fflush(NULL);

	char hold[20];
	snprintf(hold, sizeof(hold), "%d", port);
	port = strtol(hold, NULL, 10);
    clientList = (list_t *)malloc(sizeof(list_t));
    messageList = (list_t *)malloc(sizeof(list_t));
    clientList = createList();
    messageList = createList();

	open_socket(port);
    pthread_t messageHandler;
    pthread_create(&messageHandler, NULL, handle_messages, NULL);
    handle_connections();
    sem_destroy(&messageFlag);
}
