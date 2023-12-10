#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>
#include <termios.h>
#include <pthread.h>

void getCurrentTime(char *timeStr)
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(timeStr, 20, "%Y-%m-%d %H:%M:%S", timeinfo);
}

void setTerminalEcho(int enable)
{
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if (!enable)
    {
        tty.c_lflag &= ~ECHO;
    }
    else
    {
        tty.c_lflag |= ECHO;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

int readline(char *buffer, int maxchars, char eoc)
{
    int n = 0;
    while (n < maxchars - 1)
    {
        int ch = getc(stdin);
        if (ch == EOF)
        {
            perror("Error reading input");
            exit(1);
        }
        buffer[n] = ch;
        if (buffer[n] == eoc || buffer[n] == '\n')
        {
            buffer[n] = '\0';
            break;
        }
        n++;
    }
    return n;
}

// Hàm xử lý luồng cho việc gửi file
void *sendFileThreadFunction(void *arg)
{
    int sockfd = *((int *)arg);

    // Nhập đường dẫn của file từ người dùng
    fprintf(stdout, "Enter the path of the file to send: ");
    fflush(stdout);

    char filePath[256];
    if (readline(filePath, sizeof(filePath), '\n') <= 0)
    {
        perror("Error reading file path");
        exit(1);
    }

    // Kiểm tra xem file có tồn tại không
    if (access(filePath, F_OK) == -1)
    {
        fprintf(stdout, "File not found: %s\n", filePath);
        pthread_exit(NULL); // Kết thúc luồng nếu file không tồn tại
    }

    // Gửi lệnh "sendfile" đến server
    write(sockfd, "sendfile", strlen("sendfile"));

    // Gửi tên file đến server
    write(sockfd, filePath, strlen(filePath));

    // Mở file để đọc và gửi dữ liệu
    FILE *file = fopen(filePath, "rb");
    if (file == NULL)
    {
        perror("Error opening file for reading");
        exit(1);
    }

    char buffer[1024];
    size_t bytesRead;

    // Đọc từ file và gửi đến server
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        ssize_t bytesSent = write(sockfd, buffer, bytesRead);
        if (bytesSent <= 0)
        {
            perror("Error sending file data");
            fclose(file);
            pthread_exit(NULL);
        }
    }

    // Gửi lệnh "EOF" để thông báo là đã gửi xong file
    write(sockfd, "EOF", strlen("EOF"));

    fclose(file);

    // Kết thúc luồng
    pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{
    char username[50];
    char password[20];
    int authenticationStatus = 0;

    int sockfd;
    struct sockaddr_in serv_addr;
    char userData[1024];
    char sendline[1024], recvline[1024];

    memset(userData, 0, sizeof(userData));

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        perror("error while creating socket...");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        perror("address conversion error...");
        exit(-1);
    }

    printf("Welcome to our application\n");
    printf("Let's login to chat\n");
    printf("Note: Typing 'sendfile' to send file\n");
    printf("      Typing 'private <username>' to chat 1 - 1");
    printf("      Typing 'endprivate <username>' to end chatting private");
    printf("      Typing 'quit' to logout");

    fprintf(stdout, "Enter your name: ");
    fflush(stdout);

    if (readline(username, sizeof(username), '\n') <= 0)
    {
        perror("Error reading username");
        exit(1);
    }

    fprintf(stdout, "Enter your password: ");
    fflush(stdout);
    setTerminalEcho(0); // Tắt hiển thị mật khẩu khi nhập
    if (readline(password, sizeof(password), '\n') <= 0)
    {
        perror("Error reading password");
        exit(1);
    }
    // Kích hoạt lại hiển thị mật khẩu
    setTerminalEcho(1);

    sprintf(userData, "%s %s\n", username, password);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof serv_addr) < 0)
    {
        perror("connect error...");
        exit(1);
    }
    ssize_t dataBytesWritten = send(sockfd, userData, strlen(userData), 0);

    if (dataBytesWritten <= 0)
    {
        perror("send");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    ssize_t bytesRead = read(sockfd, &authenticationStatus, sizeof(authenticationStatus));

    if (bytesRead <= 0)
    {
        perror("read");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (authenticationStatus == 1)
    {
        fprintf(stdout, "\nAuthenticated successfully! Start chatting!\n");

        fd_set waitfds;
        int readyfds;
        struct timeval timeout;

        while (1)
        {
            FD_ZERO(&waitfds);
            FD_SET(sockfd, &waitfds);
            FD_SET(0, &waitfds);

            // Thiết lập thời gian chờ là 1 giây
            timeout.tv_sec = 30;
            timeout.tv_usec = 0;

            memset(recvline, 0, sizeof(recvline));
            memset(sendline, 0, sizeof(sendline));

            readyfds = select(sockfd + 1, &waitfds, NULL, NULL, &timeout);
            if (readyfds < 0)
            {
                perror("select error");
                exit(1);
            }
            else if (readyfds == 0)
            {
                // Không có sự kiện nào xảy ra trong thời gian chờ
                printf("Waiting for input...\n");
            }
            else
            {
                // Nếu có sẵn dữ liệu vào thì đọc và gửi đi
                if (FD_ISSET(0, &waitfds))
                {
                    if (fgets(sendline, sizeof(sendline), stdin) != NULL)
                    {
                        sendline[strcspn(sendline, "\n")] = '\0';
                        // Kiểm tra nếu lệnh là "sendfile" thì thực hiện gửi file
                        if (strcmp(sendline, "sendfile") == 0)
                        {
                            // Tạo một luồng mới để xử lý việc gửi file
                            pthread_t sendFileThread;
                            if (pthread_create(&sendFileThread, NULL, sendFileThreadFunction, (void *)&sockfd) != 0)
                            {
                                perror("pthread_create error");
                                exit(EXIT_FAILURE);
                            }

                            // Đợi luồng gửi file kết thúc
                            pthread_join(sendFileThread, NULL);
                        }
                        else if (strncmp(sendline, "private", 7) == 0)
                        {
                            // Xử lý yêu cầu chat riêng
                            char privateUsername[50];
                            sscanf(sendline, "private %s", privateUsername);
                            write(sockfd, sendline, strlen(sendline));
                        }
                        else if (strncmp(sendline, "endprivate", 10) == 0)
                        {
                            // Xử lý yêu cầu chat riêng
                            char privateUsername[50];
                            sscanf(sendline, "endprivate %s", privateUsername);
                            write(sockfd, sendline, strlen(sendline));
                        }
                        else
                        {
                            // Gửi thông điệp như thông thường
                            write(sockfd, sendline, strlen(sendline));

                            // Kiểm tra nếu người dùng gõ "quit" thì thoát khỏi vòng lặp
                            if (strcmp(sendline, "quit") == 0)
                            {
                                break;
                            }
                        }
                    }
                }

                // Nếu có sẵn dữ liệu từ server gửi tới thì đọc và in ra màn hình
                if (FD_ISSET(sockfd, &waitfds))
                {
                    // Xử lý dữ liệu từ server
                    ssize_t bytesRead = read(sockfd, recvline, sizeof(recvline));
                    if (bytesRead <= 0)
                    {
                        if (bytesRead == 0)
                        {
                            // Server đã đóng kết nối
                            printf("Server has stopped.\n");
                        }
                        else
                        {
                            perror("read");
                        }

                        close(sockfd);
                        exit(EXIT_FAILURE);
                    }
                    
                    printf("%s\n", recvline);
                }
            }
        }
    }
    else
    {
        fprintf(stdout, "Authentication failed. Invalid username or password.\n");
    }

    close(sockfd);
    return 0;
}
