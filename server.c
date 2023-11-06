#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <errno.h>

#define MAX_NAME_LENGTH 20
#define SERVER_PORT 8080
#define MAX_CLIENTS 10

int semid;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void registerUser(int clientSock)
{
    // Read user name from client
    char name[MAX_NAME_LENGTH];
    ssize_t bytesRead = read(clientSock, name, sizeof(name));
    if (bytesRead <= 0)
    {
        perror("read");
        exit(1);
    }

    char response[] = "User registered successfully. Welcome!\n";
    ssize_t bytesWritten = write(clientSock, response, strlen(response));
    if (bytesWritten <= 0)
    {
        perror("write");
        exit(1);
    }
}

void sendMessageToAllUsers(int senderSock, char *message)
{
    pthread_mutex_lock(&mutex);
    int clientSockets[MAX_CLIENTS];
        // Send message to all connected clients except the sender
        for (int i = 0; i < MAX_CLIENTS; i++)
    {
        int clientSock = clientSockets[i];
        if (clientSock != -1 && clientSock != senderSock)
        {
            ssize_t bytesWritten = write(clientSock, message, strlen(message));
            if (bytesWritten <= 0)
            {
                perror("write");
                exit(1);
            }
        }
    }

    pthread_mutex_unlock(&mutex);
}

void *handleClient(void *arg)
{
    int clientSock = *((int *)arg);
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(struct sockaddr_in);

    // Get client address
    if (getpeername(clientSock, (struct sockaddr *)&clientAddr, &addrLen) == -1)
    {
        perror("getpeername");
        exit(1);
    }

    printf("Client connected: %s\n", inet_ntoa(clientAddr.sin_addr));

    // Handle client requests
    char request[256];
    while (1)
    {
        // Read client request
        ssize_t bytesRead = read(clientSock, request, sizeof(request));
        if (bytesRead <= 0)
        {
            printf("Client disconnected: %s\n", inet_ntoa(clientAddr.sin_addr));
            close(clientSock);
            break;
        }

        // Process client request
        if (strncmp(request, "register", 8) == 0)
        {
            // Lock the mutex before registering the user
            pthread_mutex_lock(&mutex);

            registerUser(clientSock);

            // Unlock the mutex after registering the user
            pthread_mutex_unlock(&mutex);
        }
        else
        {
            char response[] = "Invalid request.\n";
            ssize_t bytesWritten = write(clientSock, response, strlen(response));
            if (bytesWritten <= 0)
            {
                perror("write");
                exit(1);
            }
        }
    }

    close(clientSock);
    pthread_exit(NULL);
}

int main()
{
    int serverSock, clientSock;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(struct sockaddr_in);
    pthread_t threads[MAX_CLIENTS];
    int clientSockets[MAX_CLIENTS];
    int threadCount = 0;

    // Initialize clientSockets array
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clientSockets[i] = -1;
    }

    // Create server socket
    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == -1)
    {
        perror("socket");
        exit(1);
    }

    // Set server details
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to server address
    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("bind");
        exit(1);
    }

    // Initialize semaphore
    key_t key;
    if ((key = ftok(".", 'S')) == -1)
    {
        perror("ftok");
        exit(1);
    }

    semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (semid == -1)
    {
        if (errno == EEXIST)
        {
            semid = semget(key, 1, 0);
        }
        else
        {
            perror("semget");
            exit(1);
        }
    }
    else
    {
        union semun
        {
            int val;
            struct semid_ds *buf;
            unsigned short *array;
        } arg;

        arg.val = 1;
        if (semctl(semid, 0, SETVAL, arg) == -1)
        {
            perror("semctl");
            exit(1);
        }
    }

    // Listen for incoming connections
    if (listen(serverSock, MAX_CLIENTS) == -1)
    {
        perror("listen");
        exit(1);
    }

    printf("Server started. Listening on port %d\n", SERVER_PORT);

    // Accept client connections and create threads to handle them
    while (1)
    {
        // Accept new connection
        clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrLen);
        if (clientSock == -1)
        {
            perror("accept");
            exit(1);
        }

        // Check if maximum number of clients reached
        if (threadCount >= MAX_CLIENTS)
        {
            printf("Maximum number of clients reached. Connection rejected.\n");
            close(clientSock);
            continue;
        }

        // Store client socket in array
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clientSockets[i] == -1)
            {
                clientSockets[i] = clientSock;
                break;
            }
        }

        // Create thread to handle client
        if (pthread_create(&threads[threadCount], NULL, handleClient, &clientSock) != 0)
        {
            perror("pthread_create");
            exit(1);
        }

        // Increase thread count
        threadCount++;
    }

    // Wait for all threads to finish
    for (int i = 0; i < threadCount; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Close server socket
    close(serverSock);

    return 0;
}