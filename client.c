#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_NAME_LENGTH 20
#define MAX_MESSAGE_LENGTH 256
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

void registerUser(int sockfd) {
    // Read user name from client
    char name[MAX_NAME_LENGTH];
    printf("Enter your name: ");
    fgets(name, MAX_NAME_LENGTH, stdin);
    name[strcspn(name, "\n")] = '\0';

    // Send name to server for registration
    ssize_t bytesWritten = write(sockfd, name, strlen(name));
    if (bytesWritten <= 0) {
        perror("write");
        exit(1);
    }

    // Receive response from server
    char response[256];
    ssize_t bytesRead = read(sockfd, response, sizeof(response));
    if (bytesRead <= 0) {
        perror("read");
        exit(1);
    }
    printf("%s\n", response);

    printf("User registered successfully. Welcome, %s!\n", name);
}

void sendMessage(int sockfd) {
    char message[MAX_MESSAGE_LENGTH];

    while (1) {
        // Read message from client
        printf("Enter your message (or 'q' to quit): ");
        fgets(message, MAX_MESSAGE_LENGTH, stdin);
        message[strcspn(message, "\n")] = '\0';

        // Send message to server
        ssize_t bytesWritten = write(sockfd, message, strlen(message));
        if (bytesWritten <= 0) {
            perror("write");
            exit(1);
        }

        // Check if client wants to quit
        if (strcmp(message, "q") == 0) {
            break;
        }

        // Receive response from server
        char response[MAX_MESSAGE_LENGTH];
        ssize_t bytesRead = read(sockfd, response, sizeof(response));
        if (bytesRead <= 0) {
            perror("read");
            exit(1);
        }
        printf("Server response: %s\n", response);
    }
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
    registerUser(sockfd);

    // Send and receive messages
    sendMessage(sockfd);

    // Close socket
    close(sockfd);

    return 0;
}