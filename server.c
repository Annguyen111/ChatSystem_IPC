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
#include <pthread.h>

#define MAX_CLIENTS 10
#define MAX_NAME_LENGTH 50
#define MAX_PASSWORD_LENGTH 20

sem_t sem; // semaphore

struct Client
{
    int sockfd;
    char name[MAX_NAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
};
struct Credentials
{
    char name[MAX_NAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
};

int handleClient(const char *usename, const char *password)
{
    // Mở file
    FILE *file = fopen("account.txt", "r");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[100];
    int matchFound = 0;
    // Đọc file theo từng dòng
    while (fgets(line, 100, file))
    {
        struct Credentials credentials;
        if (sscanf(line, "%49[^,],%49s", credentials.name, credentials.password) == 2)
        {
            if (strcmp(credentials.name, usename) == 0 && strcmp(credentials.password, password) == 0)
            {
                matchFound = 1;
                break;
            }
        }
    }
    fclose(file);
    return matchFound;
}

void getCurrentTime(char *timeStr)
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(timeStr, 20, "%Y-%m-%d %H:%M:%S", timeinfo);
}

int main(int argc, char const *argv[])
{
    int mastersockfd, activeconnections = 0;
    struct Client clients[MAX_CLIENTS];

    char receivedData[1024];

    struct sockaddr_in serv_addr, clientIPs[MAX_CLIENTS];
    int addrlen = sizeof serv_addr;
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
    setsockopt(mastersockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));

    // liên kết master socket với địa chỉ và cổng
    if (bind(mastersockfd, (struct sockaddr *)&serv_addr, addrlen) < 0)
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
            if (clients[i].sockfd != 0)
            {
                FD_SET(clients[i].sockfd, &readfds);
                if (clients[i].sockfd > max_fd)
                    max_fd = clients[i].sockfd;
            }
        }

        // sử dụng hàm select để kiểm tra sự sẵn có của dữ liệu đọc từ các socket
        readyfds = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if ((readyfds < 0) && (errno != EINTR))
        {
            perror("select error");
            exit(1);
        }

        // kiểm tra xem có kết nối mới đến master socket không
        if (FD_ISSET(mastersockfd, &readfds))
        {
            // dợi semaphore trước khi thực hiện thao tác trên dữ liệu
            sem_wait(&sem);

            // chấp nhận kết nối mới
            if ((clients[activeconnections].sockfd = accept(mastersockfd, (struct sockaddr *)&clientIPs[activeconnections], (socklen_t *)&addrlen)) < 0)
            {
                perror("accept error...");
                exit(1);
            }

            fprintf(stdout, "New connection from %s\n", inet_ntoa(clientIPs[activeconnections].sin_addr));

            // nhận tên từ client và lưu vào danh sách
            read(clients[activeconnections].sockfd, receivedData, sizeof(receivedData));
            printf("Received: %s", receivedData);

            // Tách username vs password
            char *tokens[2];
            char *token = strtok(receivedData, ";");
            for (int i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
            {
                tokens[i] = token;
                token = strtok(NULL, ";");
                printf("Token %d: %s\n", i, tokens[i]);
            }
            clients[activeconnections].name[0] = '\0';
            strncat(clients[activeconnections].name, tokens[0], sizeof(clients[activeconnections].name) - strlen(clients[activeconnections].name) - 1);
            printf("name: %s\n", clients[activeconnections].name);

            clients[activeconnections].password[0] = '\0';
            strncat(clients[activeconnections].password, tokens[1], sizeof(clients[activeconnections].password) - strlen(clients[activeconnections].password) - 1);
            printf("password: %s\n", clients[activeconnections].password);
            
            // Kiểm tra đăng nhập
            if (handleClient(clients[activeconnections].name, clients[activeconnections].password))
            {
                printf("Authentication successful\n");
                fprintf(stdout, "Client %s joined\n", clients[activeconnections].name);
                memset(tokens, 0, sizeof(tokens));
            }
            else
            {
                printf("Authentication failed\n");
            }

            activeconnections++;
            // giải phóng semaphore sau khi đã thực hiện xong thao tác trên dữ liệu
            sem_post(&sem);
        }

        // xử lý dữ liệu từ các client kết nối
        for (int i = 0; i < activeconnections; i++)
        {
            if (clients[i].sockfd != 0 && FD_ISSET(clients[i].sockfd, &readfds))
            {
                // dợi semaphore trước khi thực hiện thao tác trên dữ liệu
                sem_wait(&sem);

                // xóa bộ đệm
                memset(inBuffer, 0, 1024);
                memset(outBuffer, 0, 1024);

                // hàm read trả về 0 nếu kết nối đã đóng một cách bình thường
                // và -1 nếu có lỗi
                if (read(clients[i].sockfd, inBuffer, 1024) <= 0)
                {
                    fprintf(stderr, "Client %s disconnected\n", clients[i].name);
                    close(clients[i].sockfd);
                    clients[i].sockfd = 0;

                    // giải phóng semaphore sau khi đã thực hiện xong thao tác trên dữ liệu
                    sem_post(&sem);

                    continue;
                }

                // Lấy thời gian hiện tại
                char timeStr[20];
                getCurrentTime(timeStr);

                // In và log nội dung chat với thời gian
                FILE *logFile = fopen("chatlog.txt", "a");
                if (logFile != NULL)
                {
                    fprintf(logFile, "[%s] %s: %s", timeStr, clients[i].name, inBuffer);
                    fclose(logFile);
                }

                fprintf(stdout, "%s: %s", clients[i].name, inBuffer);

                // ghép tên client và dữ liệu nhận được để gửi đến các client khác
                strcat(outBuffer, clients[i].name);
                strcat(outBuffer, ": ");
                strcat(outBuffer, inBuffer);

                // gửi dữ liệu đến các client khác
                for (int j = 0; j < activeconnections; j++)
                {
                    if (clients[j].sockfd != 0 && i != j)
                    {
                        write(clients[j].sockfd, outBuffer, strlen(outBuffer));
                    }
                }

                // giải phóng semaphore sau khi đã thực hiện xong thao tác trên dữ liệu
                sem_post(&sem);
            }
        }
    }

    // hủy semaphore khi không cần sử dụng nữa
    sem_destroy(&sem);

    return 0;
}
