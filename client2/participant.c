#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

#define BUFFER_SIZE 2048


// Starts the connection at address and port and returns the fD
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

// -=[Important Variables]=-
int ID;
char fileName[BUFFER_SIZE];
char serverAddress[BUFFER_SIZE];
int serverPort;
int serverFD;
bool active = true;
pthread_t thread = -1;
sem_t acknowledgement, ready;
int receivePort;
int messageFD;
int connected = 0;

char inputBuffer[BUFFER_SIZE];

// Opens the socket for connections and accepts the first connection.
int open_socket(int port) {
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
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
    return socketFD;
}

// This prints any message it receives from the server verbatim.
// This should probably be a message processor.
void* serverListener() {
    // Opens the port for listening, waits for the server to connect, and then unblocks the main thread.
    int socketFD = open_socket(receivePort);
    printf("Server Port: %d\n", receivePort);

    sem_post(&ready);
    // Accepts the first connection
    struct sockaddr_in clientAddr;
    socklen_t clientAddrlen = sizeof(clientAddr);
    if((messageFD = accept(socketFD, (struct sockaddr*) &clientAddr, &clientAddrlen)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
    printf("Connected\n");
	connected = 1;

    char buffer[BUFFER_SIZE];
    int readAmount = 0;
    while(1) {
        if(active) {
            readAmount = read(messageFD, buffer, BUFFER_SIZE - 1);
            buffer[readAmount] = '\0';
            // printf("Raw Input: %s\n", buffer);
            char *token;
            token = strtok(buffer, "^");
            while(token != NULL) {
                if(strcmp(token, "ack0") == 0) {
                    sem_post(&acknowledgement);
                } else if (readAmount > 0) {
                    FILE *wfile = fopen(fileName, "a");
                    printf("%s\n", token);
                    fwrite(token, 1, readAmount, wfile);
                    fwrite("\n", 1, 1, wfile);
                    fclose(wfile);
                    fflush(stdout);
                }
                token = strtok(NULL, "^");
            }
        }
    }
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    sem_init(&acknowledgement, 0, 0);
    sem_init(&ready, 0, 0);
	if (argc < 2)
	{
		printf("Usage: ./participant CONFIGFILE.txt\n");
		return 1;
	}

	FILE *file = fopen(argv[1], "r");
	if (file == NULL)
	{
		printf("No file\n");
		return 1;
	}
	if (fscanf(file, "%d %s %s %d", &ID, fileName, serverAddress, &serverPort) == 4)
	{
		printf("ID = %d\n", ID);
		printf("Log File = %s\n", fileName);
		printf("Coordinator Address = %s\n", serverAddress);
		printf("Coordinator Port = %d\n", serverPort);
	}
	else
	{
		printf("Error: Failed to read info from file\n");
		fclose(file);
		return 1; // Exit with error code
	}
	fclose(file);
	fflush(NULL);

    serverFD = create_connection(serverAddress, serverPort);

    while (1)
	{
		int readAmount;

		// Gets input
		readAmount = read(0, inputBuffer, BUFFER_SIZE);
        inputBuffer[readAmount-1] = '\0';

        char input_copy[readAmount];
        strcpy(input_copy, inputBuffer);

		char *command = strtok(input_copy, " ");
		char *param = strtok(NULL, "");

		if (strcmp(command, "register") == 0)
		{
			// Spins up a new thread and then sends the message after it's ready.
            if(thread == -1 && active) {
                receivePort = atoi(param);
                pthread_create(&thread, NULL, serverListener, NULL);
                sem_wait(&ready);
                active = true;
                char sendMessage[BUFFER_SIZE];
                sprintf(sendMessage, "register %d %d", ID, receivePort);
                write(serverFD, sendMessage, strlen(sendMessage));

				/*write(serverFD, "msend not this", strlen("msend not this"));
				sem_wait(&acknowledgement);
				sleep(.2);
				int fix = open(fileName, O_RDWR);
				off_t fixSize = lseek(fix, 0, SEEK_END);
				off_t newSize = fixSize - 9;
				ftruncate(fix, newSize);*/
				sleep(3);
				while(connected != 1) {
					//deregister again
					if(thread != -1) {
						write(serverFD, "deregister", strlen("deregister"));
						pthread_cancel(thread);
						pthread_join(thread, NULL);
						close(messageFD);
						shutdown(messageFD, SHUT_RDWR);
						thread = -1;
						connected = 0;
					}
					//register again
					if(thread == -1 && active) {
						receivePort = atoi(param);
						pthread_create(&thread, NULL, serverListener, NULL);
						sem_wait(&ready);
						active = true;
						char sendMessage[BUFFER_SIZE];
						sprintf(sendMessage, "register %d %d", ID, receivePort);
						write(serverFD, sendMessage, strlen(sendMessage));
						sleep(3);
					}
				}
			}
            else
            {
                printf("Already registered to a server\n");
            }
		}
		else if (strcmp(command, "deregister") == 0)
		{
			// make multicast thread dormant or dead releasing port param from register
            if(thread != -1) {
                write(serverFD, command, strlen(command));
                pthread_cancel(thread);
                pthread_join(thread, NULL);
                close(messageFD);
                shutdown(messageFD, SHUT_RDWR);
                thread = -1;
				connected = 0;
            }
            else
            {
                printf("Not in a group\n");
            }
		}
		else if (strcmp(command, "disconnect") == 0)
		{
			// make multicast thread dormant or dead releasing port param from register
			// will receive messages after reconnect within temporal constraint
            if(thread != -1) {
                write(serverFD, command, strlen(command));
                pthread_cancel(thread);
                pthread_join(thread, NULL);
                active = false;
                close(messageFD);
                shutdown(messageFD, SHUT_RDWR);
                thread = -1;
				connected = 0;
            } else {
                printf("Not connected\n");
            }
		}
		else if (strcmp(command, "reconnect") == 0)
		{
			// spin off thread listening to multicast messages on port param
			// receive messages from disconnect period subject to temporal constraint
            if(thread == -1) {
                receivePort = atoi(param);
                pthread_create(&thread, NULL, serverListener, NULL);
                sem_wait(&ready);
                active = true;
                sleep(1);
                char sendMessage[BUFFER_SIZE];
                sprintf(sendMessage, "reconnect %d %d", ID, receivePort);
                write(serverFD, sendMessage, strlen(sendMessage));

				sleep(3);
				while(connected != 1) {
					//deregister again
					if(thread != -1) {
						write(serverFD, command, strlen(command));
						pthread_cancel(thread);
						pthread_join(thread, NULL);
						active = false;
						close(messageFD);
						shutdown(messageFD, SHUT_RDWR);
						thread = -1;
						connected = 0;
					}
					//register again
					if(thread == -1) {
						receivePort = atoi(param);
						pthread_create(&thread, NULL, serverListener, NULL);
						sem_wait(&ready);
						active = true;
						sleep(1);
						char sendMessage[BUFFER_SIZE];
						sprintf(sendMessage, "reconnect %d %d", ID, receivePort);
						write(serverFD, sendMessage, strlen(sendMessage));
						sleep(3);
					}
				}
			}
            else
            {
                printf("Already registered to a server\n");
            }
		}
		else if (strcmp(command, "msend") == 0)
		{
            if(thread != -1) {
                write(serverFD, inputBuffer, strlen(inputBuffer));
                sem_wait(&acknowledgement);
            } else {
                printf("Not connected\n");
            }

		}
		else
		{
			write(1, "Invalid Command\n", sizeof("Invalid Command\n"));
		}
        fflush(stdout);
	}
}
