#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define MAX_SIZE 65535
#define swap16(x) ((((x)&0xFF) << 8) | (((x) >> 8) & 0xFF))

char buf[MAX_SIZE+1];

void print_response(int s_fd)
{
    int r_size = 0;
    if ((r_size = recv(s_fd, buf, MAX_SIZE, 0)) == -1){
        perror("response");
        exit(EXIT_FAILURE);
    }
    buf[r_size] = '\0';
    printf("%s", buf);
}

void send_print(int s_fd)
{
    printf("%s", buf);
    send(s_fd, buf, strlen(buf), 0);
    print_response(s_fd);
}

void recv_mail()
{
    const char* host_name = "pop.qq.com"; // TODO: Specify the mail server domain name
    const unsigned short port = 110; // POP3 server port
    const char* user = "*********@qq.com"; // TODO: Specify the user
    const char* pass = "****************"; // TODO: Specify the password
    char dest_ip[16];
    int s_fd; // socket file descriptor
    struct hostent *host;
    struct in_addr **addr_list;
    int i = 0;
    int r_size;

    // Get IP from domain name
    if ((host = gethostbyname(host_name)) == NULL)
    {
        herror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    addr_list = (struct in_addr **) host->h_addr_list;
    while (addr_list[i] != NULL)
        ++i;
    strcpy(dest_ip, inet_ntoa(*addr_list[i-1]));

    // TODO: Create a socket,return the file descriptor to s_fd, and establish a TCP connection to the POP3 server
    if((s_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        herror("create");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = swap16(port);
    addr_in.sin_addr.s_addr = inet_addr(dest_ip);
    memset(addr_in.sin_zero, 0, sizeof(addr_in.sin_zero));

    if(connect(s_fd, (struct sockaddr *)&addr_in, sizeof(addr_in)) == -1) {
        herror("connect");
        exit(EXIT_FAILURE);
    }

    // Print welcome message
    if ((r_size = recv(s_fd, buf, MAX_SIZE, 0)) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    buf[r_size] = '\0'; // Do not forget the null terminator
    printf("%s", buf);

    // TODO: Send user and password and print server response
    strcpy(buf, "user ");
    strcat(buf, user);
    strcat(buf, "\r\n");
    send_print(s_fd);

    strcpy(buf, "pass ");
    strcat(buf, pass);
    strcat(buf, "\r\n");
    send_print(s_fd);

    // TODO: Send STAT command and print server response
    strcpy(buf, "stat\r\n");
    send_print(s_fd);

    // TODO: Send LIST command and print server response
    strcpy(buf, "list\r\n");
    send_print(s_fd);

    // TODO: Retrieve the first mail and print its content
    strcpy(buf, "retr 1\r\n");
    send_print(s_fd);

    // TODO: Send QUIT command and print server response
    strcpy(buf, "quit\r\n");
    send_print(s_fd);

    close(s_fd);
}

int main(int argc, char* argv[])
{
    recv_mail();
    exit(0);
}
