#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_NAME_LENGTH 20
#define MAX_MESSAGE_LENGTH 256
#define MAX_CLIENTS 10
#define SERVER_PORT 8080

typedef struct {
    int sockfd;
    char name[MAX_NAME_LENGTH];
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
int numClients = 0;
sem_t mutex;

void *handleClient(void *arg) {
    ClientInfo *client = (ClientInfo *)arg;
    char message[MAX_MESSAGE_LENGTH];

    while (1) {
        // Receive message from client
        ssize_t bytesRead = recv(client->sockfd, message, sizeof(message), 0);
        if (bytesRead <= 0) {
            // Client disconnected
            sem_wait(&mutex);
            printf("Client '%s' left the chat room\n", client->name);
            // Remove client from the list
            for (int i = 0; i < numClients; i++) {
                if (clients[i].sockfd == client->sockfd) {
                    for (int j = i; j < numClients - 1; j++) {
                        clients[j] = clients[j + 1];
                    }
                    numClients--;
                    break;
                }
            }
            sem_post(&mutex);
            break;
        }

        // Send message to all connected clients
        sem_wait(&mutex);
        printf("[%s]: %s\n", client->name, message);
        for (int i = 0; i < numClients; i++) {
            if (clients[i].sockfd != client->sockfd) {
                ssize_t bytesWritten = send(clients[i].sockfd, message, bytesRead, 0);
                if (bytesWritten <= 0) {
                    perror("send");
                    break;
                }
            }
        }
        sem_post(&mutex);
    }

    close(client->sockfd);
    free(client);
    pthread_exit(NULL);
}

int main() {
    int sockfd;
    struct sockaddr_in serverAddr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(1);
    }

    // Set server details
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    // Bind socket to server address
    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("bind");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(sockfd, MAX_CLIENTS) == -1) {
        perror("listen");
        exit(1);
    }

    sem_init(&mutex, 0, 1);

    while (1) {
        // Accept client connection
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSockfd = accept(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSockfd == -1) {
            perror("accept");
            continue;
        }

        // Check if maximum number of clients reached
        if (numClients >= MAX_CLIENTS) {
            printf("Maximum number of clients reached. Connection rejected.\n");
            close(clientSockfd);
            continue;
        }

        // Read client name
        char name[MAX_NAME_LENGTH];
        ssize_t bytesRead = recv(clientSockfd, name, sizeof(name) - 1, 0);
        if (bytesRead <= 0) {
            perror("recv");
            close(clientSockfd);
            continue;
        }
        name[bytesRead] = '\0';

        // Create client info
        ClientInfo *client = (ClientInfo *)malloc(sizeof(ClientInfo));
        client->sockfd = clientSockfd;
        strncpy(client->name, name, sizeof(client->name) - 1);

        // Add client to list
        sem_wait(&mutex);
        clients[numClients++] = *client;
        sem_post(&mutex);

        // Create thread to handle client
        pthread_t tid;
        if (pthread_create(&tid, NULL, handleClient, client) != 0) {
            perror("pthread_create");
            close(clientSockfd);
            free(client);
            continue;
        }

        printf("Client '%s' joined the chat room\n", name);
    }

    close(sockfd);
    sem_destroy(&mutex);

    return 0;
}