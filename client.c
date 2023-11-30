#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>

struct Credentials
{
    char name[50];
    char password[20];
};
// Hàm để lấy thời gian hiện tại dưới dạng chuỗi
void getCurrentTime(char *timeStr)
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(timeStr, 20, "%Y-%m-%d %H:%M:%S", timeinfo);
}

// hàm này đọc dữ liệu từ stdin cho đến khi gặp ký tự eoc hoặc đạt đến giới hạn maxchars.
int readline(char *buffer, int maxchars, char eoc)
{
    int n = 0;
    while (n < maxchars)
    {
        buffer[n] = getc(stdin);
        if (buffer[n] == eoc)
            break;
        n++;
    }
    return n;
}

int main(int argc, char const *argv[])
{
    int sockfd;
    struct sockaddr_in serv_addr;
    char sendline[1024], recvline[1024];
    char loginData[100];

    // tạo socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        perror("error while creating socket...");
        exit(1);
    }

    // thiết lập địa chỉ của server
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        perror("address conversion error...");
        exit(-1);
    }

    // kết nối đến server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof serv_addr) < 0)
    {
        perror("connect error...");
        exit(1);
    }

    // Login
    struct Credentials credentials;

    printf("Enter your name: ");
    fgets(credentials.name, sizeof(credentials.name), stdin);
    credentials.name[strcspn(credentials.name, "\n")] = '\0';

    printf("Enter password: ");
    fgets(credentials.password, sizeof(credentials.password), stdin);
    credentials.password[strcspn(credentials.password, "\n")] = '\0';;

    sprintf(loginData, "%s;%s", credentials.name, credentials.password);

    // Send the login data to the server
    ssize_t dataBytesWritten = send(sockfd, loginData, strlen(loginData), 0);

    if (dataBytesWritten <= 0)
    {
        perror("send");
        exit(1);
    }

    fd_set waitfds;
    int readyfds;
    while (1)
    {
        FD_ZERO(&waitfds);

        // thêm socket và stdin vào set waitfds
        FD_SET(sockfd, &waitfds);
        FD_SET(0, &waitfds);

        // khởi tạo lại các buffer
        memset(recvline, 0, 1024);
        memset(sendline, 0, 1024);

        // kiểm tra sự sẵn có của socket và stdin bằng hàm select
        readyfds = select(sockfd + 1, &waitfds, NULL, NULL, NULL);
        if ((readyfds < 0) && (errno != EINTR))
        {
            perror("select error");
            exit(1);
        }

        // nếu stdin sẵn có, đọc dữ liệu và gửi đi
        if (FD_ISSET(0, &waitfds))
        {
            readline(sendline, 1024, '\n');
            write(sockfd, sendline, strlen(sendline));
        }

        // nếu socket sẵn có, đọc dữ liệu và in ra màn hình
        if (FD_ISSET(sockfd, &waitfds))
        {
            read(sockfd, recvline, 1024);

            // Lấy thời gian hiện tại
            char timeStr[20];
            getCurrentTime(timeStr);

            // In thời gian và dòng tin nhắn ra màn hình
            fprintf(stdout, "[%s] %s", timeStr, recvline);
        }
    }

    return 0;
}
