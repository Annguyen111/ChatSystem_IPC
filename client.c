#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_NAME_LENGTH 20
#define MAX_MESSAGE_LENGTH 256
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

typedef struct {
    int sockfd;
    const char *name;
} ThreadData;

void *receiveMessages(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int sockfd = data->sockfd;
    const char *name = data->name;
    char message[MAX_MESSAGE_LENGTH];

    while (1) {
        // Receive message from server
        ssize_t bytesRead = recv(sockfd, message, sizeof(message), 0);
        if (bytesRead <= 0) {
            perror("recv");
            break;
        }
        message[bytesRead] = '\0';
        printf("[%s]: %s\n", name, message);
    }

    pthread_exit(NULL);
}

void sendMessage(int sockfd, const char *name) {
    char message[MAX_MESSAGE_LENGTH];

    while (1) {
        // Read message from client
        printf("[%s]: ", name);
        fgets(message, MAX_MESSAGE_LENGTH, stdin);
        message[strcspn(message, "\n")] = '\0';

        // Check if client wants to quit
        if (strcmp(message, "q") == 0) {
            break;
        }

        // Send message to server
        ssize_t bytesWritten = send(sockfd, message, strlen(message), 0);
        if (bytesWritten <= 0) {
            perror("send");
            break;
        }
    }

    // Close socket
    close(sockfd);
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
    serverAddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &(serverAddr.sin_addr)) <= 0) {
        perror("inet_pton");
        exit(1);
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("connect");
        exit(1);
    }

    // Register user
    char name[MAX_NAME_LENGTH];
    printf("Enter your name: ");
    fgets(name, MAX_NAME_LENGTH, stdin);
    name[strcspn(name, "\n")] = '\0';

    // Send name to server for registration
    ssize_t bytesWritten = send(sockfd, name, strlen(name), 0);
    if (bytesWritten <= 0) {
        perror("send");
        exit(1);
    }

    // Create thread data
    ThreadData data;
    data.sockfd = sockfd;
    data.name = name;

    // Create thread to receive messages from server
    pthread_t tid;
    if (pthread_create(&tid, NULL, receiveMessages, &data) != 0) {
        perror("pthread_create");
        exit(1);
    }

    // Send and receive messages
    sendMessage(sockfd, name);

    return 0;
}