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
        while (1)
        {

            FD_ZERO(&waitfds);

            FD_SET(sockfd, &waitfds);
            FD_SET(0, &waitfds);

            memset(recvline, 0, sizeof(recvline));
            memset(sendline, 0, sizeof(sendline));

            readyfds = select(sockfd + 1, &waitfds, NULL, NULL, NULL);
            if ((readyfds < 0) && (errno != EINTR))
            {
                perror("select error");
                exit(1);
            }
            // Nếu có sẵn dữ liệu vào thì đọc và gửi đi
            if (FD_ISSET(0, &waitfds))
            {

                if (fgets(sendline, sizeof(sendline), stdin) != NULL)
                {
                    sendline[strcspn(sendline, "\n")] = '\0';
                    write(sockfd, sendline, strlen(sendline));

                    // Kiểm tra nếu người dùng gõ "quit" thì thoát khỏi vòng lặp
                    if (strcmp(sendline, "quit") == 0)
                    {
                        break;
                    }
                }
            }

            // Nếu có săn dữ liệu từ server gửi tới thì đọc và in ra màn hình
            if (FD_ISSET(sockfd, &waitfds))
            {
                ssize_t bytesRead = read(sockfd, recvline, sizeof(recvline));
                if (bytesRead <= 0)
                {
                    perror("read");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
                printf("%s\n", recvline);
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