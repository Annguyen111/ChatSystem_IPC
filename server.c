#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <semaphore.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_CLIENTS 10

sem_t sem; // semaphore

struct Client
{
    int sockfd;
    char name[50];
    char password[20];
    int authenticated;
    char privateChatWith[50];
};

void getCurrentTime(char *timeStr)
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(timeStr, 20, "%Y-%m-%d %H:%M:%S", timeinfo);
}

void sendAuthenticationStatus(int sockfd, int status)
{
    write(sockfd, &status, sizeof(status));
}

void notifyClientsAboutNewConnection(struct Client clients[], int activeconnections, const char *clientName, int state)
{
    char notification[100];
    if (state == 1)
    {
        sprintf(notification, "Client %s joined\n", clientName);
    }
    else
    {
        sprintf(notification, "Client %s logouted\n", clientName);
    }

    for (int i = 0; i < activeconnections; i++)
    {
        int clientSockfd = clients[i].sockfd;
        if (clientSockfd > 0)
        {
            write(clientSockfd, notification, strlen(notification));
        }
    }
}

// Xác thực người dùng
int authenticateUser(const char *username, const char *password, struct Client *client)
{
    FILE *dbFile = fopen("database.txt", "r");
    if (dbFile == NULL)
    {
        perror("Error opening database.txt");
        exit(EXIT_FAILURE);
    }

    char name[50];
    char dbPassword[20];

    while (fscanf(dbFile, "%s %s", name, dbPassword) == 2)
    {
        if (strcmp(username, name) == 0 && strcmp(password, dbPassword) == 0)
        {
            fclose(dbFile);
            strcpy(client->name, name);
            strcpy(client->password, dbPassword);
            return 1; // Authentication successful
        }
    }

    fclose(dbFile);
    return 0; // Authentication failed
}

// Hàm này dùng để nhận file từ client
void receiveFile(int sockfd, const char *fileName, struct Client clients[], struct Client rootClient , int activeconnections)
{
    sem_wait(&sem);

    // Kiểm tra và tạo thư mục nếu nó chưa tồn tại
    struct stat st;
    stat("uploads/", &st);
    if (!(st.st_mode & S_IWUSR))
    {
        perror("No write permission for directory");
        sem_post(&sem);
        return;
    }

    FILE *file = fopen(fileName, "ab");
    if (file == NULL)
    {
        perror("Error opening file for writing");
        sem_post(&sem);
        return;
    }

    // Kiểm tra lỗi ghi vào file
    if (ferror(file))
    {
        perror("Error writing to file");
        sem_post(&sem);
        fclose(file);
        return;
    }

    fclose(file);

    // Thông báo cho tất cả các client khác về việc gửi file thành công
    char notification[100];
    sprintf(notification, "Client %s sent a file: %s\n", rootClient.name, fileName);
    for (int i = 0; i < activeconnections; i++)
    {
        int destSockfd = clients[i].sockfd;
        if (destSockfd != sockfd)
        {
            write(destSockfd, notification, strlen(notification));
        }
    }

    sem_post(&sem);
}

int main(int argc, char const *argv[])
{
    int mastersockfd, activeconnections = 0;
    int privateChat = 0;
    struct Client clients[MAX_CLIENTS];

    struct sockaddr_in serv_addr, clientIP;
    int addrlen = sizeof(clientIP);

    char inBuffer[1024] = {0};
    char outBuffer[1024] = {0};

    // khởi tạo semaphore với giá trị ban đầu là 1 (unlocked)
    if (sem_init(&sem, 0, 1) == -1)
    {
        perror("Semaphore initialization failed");
        exit(EXIT_FAILURE);
    }

    // tạo master socket
    if ((mastersockfd = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        perror("error while creating socket...");
        exit(1);
    }

    // thiết lập thông tin địa chỉ của server
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // thiết lập tùy chọn cho socket để tái sử dụng địa chỉ ngay sau khi đóng socket
    int opt = 1;
    setsockopt(mastersockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // liên kết master socket với địa chỉ và cổng
    if (bind(mastersockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind failed...");
        exit(1);
    }

    // lắng nghe kết nối đến master socket
    if (listen(mastersockfd, 3) < 0)
    {
        perror("Listen failed ...");
        exit(1);
    }

    fprintf(stdout, "Server is listening on port %d\n", ntohs(serv_addr.sin_port));

    fd_set readfds;
    int max_fd, readyfds;

    // vòng lặp chính để xử lý kết nối
    while (1)
    {
        // thiết lập set readfds và max_fd
        FD_ZERO(&readfds);
        FD_SET(mastersockfd, &readfds);
        max_fd = mastersockfd;

        for (int i = 0; i < activeconnections; i++)
        {
            int sockfd = clients[i].sockfd;
            if (sockfd > 0)
            {
                FD_SET(sockfd, &readfds);
                if (sockfd > max_fd)
                {
                    max_fd = sockfd;
                }
            }
        }

        // sử dụng select để chờ sự kiện từ các socket
        readyfds = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (readyfds < 0)
        {
            perror("Select failed...");
            exit(1);
        }

        // kiểm tra xem master socket có sẵn sàng để chấp nhận kết nối mới không
        if (FD_ISSET(mastersockfd, &readfds))
        {
            int newsockfd;
            if ((newsockfd = accept(mastersockfd, (struct sockaddr *)&clientIP, (socklen_t *)&addrlen)) < 0)
            {
                perror("Accept failed...");
                exit(1);
            }

            // thêm kết nối mới vào danh sách kết nối
            sem_wait(&sem);
            if (activeconnections < MAX_CLIENTS)
            {
                struct Client newClient;
                newClient.sockfd = newsockfd;
                newClient.authenticated = 0;
                newClient.privateChatWith[0] = '\0';
                clients[activeconnections] = newClient;
                activeconnections++;
            }
            else
            {
                // Đạt đến số lượng kết nối tối đa, từ chối kết nối mới
                fprintf(stderr, "Too many connections. Connection rejected.\n");
                close(newsockfd);
            }
            sem_post(&sem);
        }

        // kiểm tra các kết nối hiện tại nếu có dữ liệu đến
        for (int i = 0; i < activeconnections; i++)
        {
            int sockfd = clients[i].sockfd;
            if (FD_ISSET(sockfd, &readfds))
            {
                // đọc dữ liệu từ socket
                int valread = read(sockfd, inBuffer, sizeof(inBuffer));

                if (valread <= 0)
                {
                    // Kết nối đã đóng hoặc xảy ra lỗi, xóa kết nối khỏi danh sách
                    close(sockfd);
                    // Thông báo ngắt kết nối ra màn hình
                    printf("Client %s disconnected\n", clients[i].name);
                    notifyClientsAboutNewConnection(clients, activeconnections, clients[i].name, 0);

                    sem_wait(&sem);
                    clients[i] = clients[activeconnections - 1];
                    activeconnections--;
                    sem_post(&sem);
                    i--;
                }
                else
                {
                    // Xử lý dữ liệu
                    if (clients[i].authenticated == 0)
                    {
                        // Chưa xác thực người dùng
                        char username[50];
                        char password[20];

                        sscanf(inBuffer, "%s %s", username, password);
                        // printf("%s %s\n", username, password);

                        if (authenticateUser(username, password, &clients[i]) == 1)
                        {
                            // Xác thực thành công
                            clients[i].authenticated = 1;
                            sendAuthenticationStatus(sockfd, 1);
                            printf("User '%s' authenticated successfully.\n", clients[i].name);

                            // Thông báo cho tất cả các client hiện tại về kết nối mới
                            notifyClientsAboutNewConnection(clients, activeconnections, clients[i].name, 1);
                        }
                        else
                        {
                            // Xác thực thất bại
                            clients[i].authenticated = 0;
                            sendAuthenticationStatus(sockfd, 0);
                            printf("User authentication failed.\n");
                        }
                    }
                    else
                    {
                        // Nếu dữ liệu là lệnh "sendfile" thì nhận file từ client
                        if (strncmp(inBuffer, "sendfile", 8) == 0)
                        {
                            fprintf(stdout, "Receiving a file...\n");

                            // Nhận tên file từ client
                            char fileName[256];
                            sscanf(inBuffer, "sendfile %s", fileName);
                            // read(sockfd, fileName, sizeof(fileName));

                            // Gọi hàm nhận file
                            receiveFile(sockfd, fileName, clients, clients[i], activeconnections);

                            fprintf(stdout, "File received successfully!\n");
                        }
                        else if (strncmp(inBuffer, "private", 7) == 0)
                        {
                            privateChat = 1;
                            char targetUser[50];
                            sscanf(inBuffer, "private %s", targetUser);

                            // Gửi yêu cầu chat riêng đến người dùng có tên là targetUser
                            int targetIndex = -1;
                            for (int j = 0; j < activeconnections; j++)
                            {
                                if (strcmp(clients[j].name, targetUser) == 0)
                                {
                                    targetIndex = j;
                                    break;
                                }
                            }

                            // Nếu tìm thấy người dùng, thiết lập cuộc trò chuyện riêng
                            if (targetIndex != -1 && clients[targetIndex].authenticated == 1)
                            {
                                strcpy(clients[i].privateChatWith, targetUser);
                                strcpy(clients[targetIndex].privateChatWith, clients[i].name);

                                printf("%s and %s are staying in private chat.\n", clients[i].name, clients[targetIndex].name);
                                // Gửi thông báo cho cả hai người dùng
                                char notification[100];
                                sprintf(notification, "You are now in a private chat with %s\n", targetUser);
                                write(sockfd, notification, strlen(notification));

                                sprintf(notification, "You are now in a private chat with %s\n", clients[i].name);
                                write(clients[targetIndex].sockfd, notification, strlen(notification));
                            }
                            else
                            {
                                // Không tìm thấy người dùng
                                char notification[100];
                                sprintf(notification, "User %s not found or not online\n", targetUser);
                                write(sockfd, notification, strlen(notification));
                            }
                        }
                        else if ((strncmp(inBuffer, "endprivate", 7) == 0))
                        {
                            privateChat = 0;
                            char targetUser[50];
                            sscanf(inBuffer, "endprivate %s", targetUser);

                            // Gửi yêu cầu chat riêng đến người dùng có tên là targetUser
                            int targetIndex = -1;
                            for (int j = 0; j < activeconnections; j++)
                            {
                                if (strcmp(clients[j].name, targetUser) == 0)
                                {
                                    targetIndex = j;
                                    break;
                                }
                            }

                            // Nếu tìm thấy người dùng, thiết lập cuộc trò chuyện riêng
                            if (targetIndex != -1 && clients[targetIndex].authenticated == 1)
                            {
                                clients[i].privateChatWith[0] = '\0';
                                clients[targetIndex].privateChatWith[0] = '\0';

                                printf("%s and %s quited in private chat.\n", clients[i].name, clients[targetIndex].name);
                                // Gửi thông báo cho cả hai người dùng
                                char notification[100];
                                sprintf(notification, "You quited in a private chat with %s\n", targetUser);
                                write(sockfd, notification, strlen(notification));

                                sprintf(notification, "You quited in a private chat with %s\n", clients[i].name);
                                write(clients[targetIndex].sockfd, notification, strlen(notification));
                            }
                            else
                            {
                                // Không tìm thấy người dùng
                                char notification[100];
                                sprintf(notification, "User %s not found or not online\n", targetUser);
                                write(sockfd, notification, strlen(notification));
                            }
                        }
                        else
                        {
                            // Người dùng đã xác thực, xử lý tin nhắn
                            char timeStr[20];
                            getCurrentTime(timeStr);

                            // In và log nội dung chat với thời gian
                            FILE *logFile = fopen("chatlog.txt", "a");
                            if (logFile != NULL)
                            {
                                fprintf(logFile, "[%s] %s: %s", timeStr, clients[i].name, inBuffer);
                                fclose(logFile);
                            }

                            printf("[%s] %s: %s\n", timeStr, clients[i].name, inBuffer);

                            // Gửi tin nhắn đến các client khác
                            sem_wait(&sem);
                            for (int j = 0; j < activeconnections; j++)
                            {
                                int destSockfd = clients[j].sockfd;

                                // Kiểm tra điều kiện để gửi tin nhắn đến các client khác
                                if (destSockfd != sockfd && clients[j].authenticated == 1)
                                {
                                    printf("Data privateChatWith: %s\n", clients[j].privateChatWith);
                                    // Kiểm tra xem tin nhắn có phải là tin nhắn riêng không
                                    if ((strcmp(clients[j].name, clients[i].privateChatWith) == 0 || strcmp(clients[i].name, clients[j].privateChatWith) == 0) &&
                                        clients[j].privateChatWith[0] != '\0')
                                    {

                                        // Tạo chuỗi tin nhắn để gửi
                                        outBuffer[0] = '\0';
                                        strcat(outBuffer, "[Private] ");
                                        strcat(outBuffer, clients[i].name);
                                        strcat(outBuffer, ": ");
                                        strcat(outBuffer, inBuffer);
                                        strcat(outBuffer, "\n");

                                        // Gửi tin nhắn đến client đích
                                        write(destSockfd, outBuffer, strlen(outBuffer));
                                    }
                                    else if (clients[j].privateChatWith[0] == '\0')
                                    {
                                        if (privateChat == 0)
                                        {
                                            // Tin nhắn không phải là riêng, và client không đang trong cuộc trò chuyện riêng
                                            // Tạo chuỗi tin nhắn để gửi
                                            outBuffer[0] = '\0';
                                            strcat(outBuffer, "[");
                                            strcat(outBuffer, timeStr);
                                            strcat(outBuffer, "] ");
                                            strcat(outBuffer, clients[i].name);
                                            strcat(outBuffer, ": ");
                                            strcat(outBuffer, inBuffer);
                                            strcat(outBuffer, "\n");

                                            // Gửi tin nhắn đến client đích
                                            write(destSockfd, outBuffer, strlen(outBuffer));
                                        }
                                    }
                                }
                            }
                            sem_post(&sem);
                        }
                    }
                }

                memset(inBuffer, 0, sizeof(inBuffer));
            }
        }
    }
    // giải phóng semaphore
    sem_destroy(&sem);

    return 0;
}
