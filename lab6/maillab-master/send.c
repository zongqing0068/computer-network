#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <getopt.h>
#include "base64_utils.h"

#define MAX_SIZE 4095
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

// receiver: mail address of the recipient
// subject: mail subject
// msg: content of mail body or path to the file containing mail body
// att_path: path to the attachment
void send_mail(const char* receiver, const char* subject, const char* msg, const char* att_path)
{
    const char* end_msg = "\r\n.\r\n";
    const char* host_name = "smtp.qq.com"; // TODO: Specify the mail server domain name
    const unsigned short port = 25; // SMTP server port
    const char* user = "*********@qq.com"; // TODO: Specify the user
    const char* pass = "****************"; // TODO: Specify the password
    const char* from = "*********@qq.com"; // TODO: Specify the mail address of the sender
    char dest_ip[16]; // Mail server IP address
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

    // TODO: Create a socket, return the file descriptor to s_fd, and establish a TCP connection to the mail server
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

    // Send EHLO command and print server response
    // TODO: Print server response to EHLO command
    // TODO: Enter EHLO command here
    strcpy(buf, "ehlo qq.com\r\n");
    send_print(s_fd);

    // TODO: Authentication. Server response should be printed out.
    strcpy(buf, "AUTH login\r\n");
    send_print(s_fd);

    strcpy(buf, encode_str(user));
    strcat(buf, "\r\n");
    send_print(s_fd);

    strcpy(buf, encode_str(pass));
    strcat(buf, "\r\n");
    send_print(s_fd);

    // TODO: Send MAIL FROM command and print server response
    strcpy(buf, "mail from:<");
    strcat(buf, user);
    strcat(buf, ">\r\n");
    send_print(s_fd);

    // TODO: Send RCPT TO command and print server response
    strcpy(buf, "rcpt to:<");
    strcat(buf, receiver);
    strcat(buf, ">\r\n");
    send_print(s_fd);
    
    // TODO: Send DATA command and print server response
    strcpy(buf, "data\r\n");
    send_print(s_fd);

    // TODO: Send message data
    strcpy(buf, "From:");
    strcat(buf, from);  
    strcat(buf, "\r\nTo:");
    strcat(buf, receiver);
    strcat(buf, "\r\nMIME-Version: 1.0\r\n");  
    strcat(buf, "Content-Type: multipart/mixed; ");
    strcat(buf, "boundary=qwertyuiopasdfghjklzxcvbnm\r\n");
    strcat(buf, "Subject:");
    strcat(buf, subject);   
    strcat(buf, "\r\n");   
  
    send(s_fd, buf, strlen(buf), 0);
    printf("%s", buf);

    if(msg) {
        strcpy(buf, "\r\n--qwertyuiopasdfghjklzxcvbnm\r\n");
        strcat(buf, "Content-Type: text/plain\r\n\r\n");
        send(s_fd, buf, strlen(buf), 0);
        printf("%s", buf);
        FILE *file_mail_body = fopen(msg, "r");
        if(file_mail_body) {
            // msg = path to the file containing mail body
            int mail_size = fread(buf, 1, MAX_SIZE, file_mail_body);
            buf[mail_size] = '\0';
            strcat(buf, "\r\n");
            send(s_fd, buf, strlen(buf), 0);
            printf("%s", buf);
            fclose(file_mail_body);
        } else {
            // msg = content of mail body
            strcpy(buf, msg);
            strcat(buf, "\r\n");
            send(s_fd, buf, strlen(buf), 0);
            printf("%s", buf);
        }
    }

    if(att_path) {
        // path to the attachment exist
        strcpy(buf, "\r\n--qwertyuiopasdfghjklzxcvbnm\r\n");
        strcat(buf, "Content-Type: application/octet-stream\r\n");
        strcat(buf, "Content-Disposition: attachment; name=");
        strcat(buf, att_path);
        strcat(buf, "\r\n");
        strcat(buf, "Content-Transfer-Encoding: base64\r\n\r\n");
        send(s_fd, buf, strlen(buf), 0);
        printf("%s", buf);

        // encode the att_file
        FILE *raw_file = fopen(att_path, "rb");
        FILE *base64_file = tmpfile();
        encode_file(raw_file, base64_file);
        fclose(raw_file);
        
        // get size of the att_file
        fseek(base64_file, 0, SEEK_END);
        int att_size = ftell(base64_file);
        char* att_content = (char*) malloc((att_size + 1) * sizeof(char));
        rewind(base64_file);

        // read the att_file content
        fread(att_content, 1, att_size, base64_file);
        att_content[att_size] = '\0';
        fclose(base64_file);
        
        send(s_fd, att_content, strlen(att_content), 0);
    }

    // TODO: Message ends with a single period
    strcpy(buf, end_msg);
    send_print(s_fd);

    // TODO: Send QUIT command and print server response
    strcpy(buf, "quit\r\n");
    send_print(s_fd);

    close(s_fd);
}

int main(int argc, char* argv[])
{
    int opt;
    char* s_arg = NULL;
    char* m_arg = NULL;
    char* a_arg = NULL;
    char* recipient = NULL;
    const char* optstring = ":s:m:a:";
    while ((opt = getopt(argc, argv, optstring)) != -1)
    {
        switch (opt)
        {
        case 's':
            s_arg = optarg;
            break;
        case 'm':
            m_arg = optarg;
            break;
        case 'a':
            a_arg = optarg;
            break;
        case ':':
            fprintf(stderr, "Option %c needs an argument.\n", optopt);
            exit(EXIT_FAILURE);
        case '?':
            fprintf(stderr, "Unknown option: %c.\n", optopt);
            exit(EXIT_FAILURE);
        default:
            fprintf(stderr, "Unknown error.\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        fprintf(stderr, "Recipient not specified.\n");
        exit(EXIT_FAILURE);
    }
    else if (optind < argc - 1)
    {
        fprintf(stderr, "Too many arguments.\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        recipient = argv[optind];
        send_mail(recipient, s_arg, m_arg, a_arg);
        exit(0);
    }
}
