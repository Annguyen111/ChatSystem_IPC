#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_NAME_LENGTH 20
#define SERVER_PORT 8080
#define MAX_CLIENTS 10

void registerUser(int clientSock) {
    // Read user name from client
    char name[MAX_NAME_LENGTH];
    ssize_t bytesRead = read(clientSock, name, sizeof(name));
    if (bytesRead <= 0) {
        perror("read");
        exit(1);
    }

    char response[] = "User registered successfully. Welcome!\n";
    ssize_t bytesWritten = write(clientSock, response, sizeof(response));
    if (bytesWritten <= 0) {
        perror("write");
        exit(1);
    }
}

void sendMessageToAllUsers(int senderSock, char* message) {
    // Send message to all connected clients except the sender
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(struct sockaddr_in);
    int clientSock;

    while ((clientSock = accept(senderSock, (struct sockaddr *)&clientAddr, &addrLen)) != -1) {
        if (clientSock != senderSock) {
            ssize_t bytesWritten = write(clientSock, message, strlen(message));
            if (bytesWritten <= 0) {
                perror("write");
                exit(1);
            }
            close(clientSock);
        }
    }
}

void *handleClient(void *arg) {
    int clientSock = *((int *)arg);
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(struct sockaddr_in);

    // Get client address
    if (getpeername(clientSock, (struct sockaddr *)&clientAddr, &addrLen) == -1) {
        perror("getpeername");
        exit(1);
    }

    printf("Client connected: %s\n", inet_ntoa(clientAddr.sin_addr));

    // Handle client requests
    char request[256];
    while (1) {
        // Read client request
        ssize_t bytesRead = read(clientSock, request, sizeof(request));
        if (bytesRead <= 0) {
            printf("Client disconnected: %s\n", inet_ntoa(clientAddr.sin_addr));
            close(clientSock);
            break;
        }

        // Process client request
        if (strncmp(request, "register", 8) == 0) {
            registerUser(clientSock);
        } else {
            char response[] = "Invalid request.\n";
            ssize_t bytesWritten = write(clientSock, response, sizeof(response));
            if (bytesWritten <= 0) {
                perror("write");
                exit(1);
            }
        }
    }

    pthread_exit(NULL);
}

int main() {
    int serverSock, clientSock;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(struct sockaddr_in);
    pthread_t threads[MAX_CLIENTS];
    int threadCount = 0;

    // Create server socket
    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == -1) {
        perror("socket");
        exit(1);
    }

    // Set server details
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to server address
    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("bind");
        exit(1);
    }

    // Listen for client connections
    if (listen(serverSock, 5) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    // Accept client connections and handle requests
    while (1) {
        // Accept client connection
        clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrLen);
        if (clientSock == -1) {
            perror("accept");
            exit(1);
        }

        // Create a new thread to handle the client request
        if (pthread_create(&threads[threadCount], NULL, handleClient, &clientSock) != 0) {
            perror("pthread_create");
            exit(1);
        }
        threadCount++;

        // Limit the number of concurrent clients
        if (threadCount >= MAX_CLIENTS) {
            printf("Maximum number of clients reached. Rejecting new connections.\n");
            break;
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], NULL);
    }

    // Close the server socket
    close(serverSock);

    return 0;
}