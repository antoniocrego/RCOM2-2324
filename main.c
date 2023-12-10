#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

struct url {
    char *user;
    char *password;
    char *host;
    char *path;
    char *file;
    int user_size;
    int password_size;
    int host_size;
    int path_size;
    int file_size;
};

int url_parser(char *str, struct url *url) {
    regex_t regex_user;
    regex_t regex_nouser;
    char *nouserpattern = "(ftp)://([^/]*)/(.*/)*(.*)";
    char *userpattern = "(ftp)://([^:]*):([^@]*)@([^/]*)/(.*/)*(.*)";
    regmatch_t match[7];
    if (regcomp(&regex_user, userpattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Failed to compile regex pattern\n");
        return 1;
    }
    if (regcomp(&regex_nouser, nouserpattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Failed to compile regex pattern\n");
        return 1;
    }
    if (regexec(&regex_user, str, 7, match, 0) == 0){
        printf("Match with user\n");
        url->user_size = match[2].rm_eo - match[2].rm_so;
        url->password_size = match[3].rm_eo - match[3].rm_so;
        url->host_size = match[4].rm_eo - match[4].rm_so;
        url->path_size = match[5].rm_eo - match[5].rm_so;
        url->file_size = match[6].rm_eo - match[6].rm_so;
        url->user = (char*) malloc(url->user_size+1);
        url->password = (char*) malloc(url->password_size+1);
        url->host = (char*) malloc(url->host_size+1);
        url->path = (char*) malloc(url->path_size+1);
        url->file = (char*) malloc(url->file_size+1);
        memset(url->user, '\0', url->user_size+1);
        memset(url->password, '\0', url->password_size+1);
        memset(url->host, '\0', url->host_size+1);
        memset(url->path, '\0', url->path_size+1);
        memset(url->file, '\0', url->file_size+1);
        strncpy(url->user, str + match[2].rm_so, url->user_size);
        strncpy(url->password, str + match[3].rm_so, url->password_size);
        strncpy(url->host, str + match[4].rm_so, url->host_size);
        strncpy(url->path, str + match[5].rm_so, url->path_size);
        strncpy(url->file, str + match[6].rm_so, url->file_size);
    }
    else if (regexec(&regex_nouser, str, 5, match, 0) == 0){
        printf("Match without user\n");
        url->user_size = 9;
        url->password_size = 8;
        url->host_size = match[2].rm_eo - match[2].rm_so;
        url->path_size = match[3].rm_eo - match[3].rm_so;
        url->file_size = match[4].rm_eo - match[4].rm_so;
        url->user = (char*) malloc(10);
        url->password = (char*) malloc(9);
        url->host = (char*) malloc(url->host_size+1);
        url->path = (char*) malloc(url->path_size+1);
        url->file = (char*) malloc(url->file_size+1);
        memset(url->host, '\0', url->host_size+1);
        memset(url->path, '\0', url->path_size+1);
        memset(url->file, '\0', url->file_size+1);
        strcpy(url->user, "anonymous");
        strcpy(url->password, "password");
        strncpy(url->host, str + match[2].rm_so, url->host_size);
        strncpy(url->path, str + match[3].rm_so, url->path_size);
        strncpy(url->file, str + match[4].rm_so, url->file_size);
    }
    else {
        printf("No match\n");
        regfree(&regex_user);
        regfree(&regex_nouser);
        return 1;
    }
    regfree(&regex_user);
    regfree(&regex_nouser);
    return 0;
}

int socket_create(char *ip, int port, int *sockfd) {

    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(*sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return 0;
}

int read_response(int sockfd, char *buffer){
    int i = 0;
    char byte = 0;
    printf("Reading response\n");
    while(byte != '\n'){
        int b = read(sockfd, &byte, 1);
        if(b < 0){
            perror("read()");
            exit(-1);
        }
        if(b == 0){
            printf("Connection closed\n");
            break;
        }
        printf("%c", byte);
        buffer[i] = byte;
        i++;
    }
    if(buffer[3] == '-'){
        printf("Multiline response\n");
        while(buffer[3] == '-'){
            byte = 0;
            i = 0;
            while(byte != '\n'){
                if(read(sockfd, &byte, 1) < 0){
                    perror("read()");
                    exit(-1);
                }
                buffer[i] = byte;
                printf("%c", byte);
                i++;
            }
        }
    }
    buffer[i] = '\0';
    return 0;
}

int main(int argc, char *argv[]) {
    int bytes = 0;
    struct url url;
    char buffer[1024];
    if(argc != 2) {
        printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        return 1;
    }
    if(url_parser(argv[1], &url) != 0){
        printf("Error parsing url\n");
        return 1;
    }
    
    char* user = (char*) malloc(sizeof(char)*(url.user_size+5+3));
    char* password = (char*) malloc(sizeof(char)*(url.password_size+5+3));
    strcpy(user, "USER ");
    strcpy(password, "PASS ");
    strcat(user, url.user);
    strcat(user, "\r\n");
    strcat(password, url.password);
    strcat(password, "\r\n");

    struct hostent *h;

    if ((h = gethostbyname(url.host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    char *ip = inet_ntoa(*((struct in_addr *) h->h_addr));
    
    int command_socket;

    socket_create(ip, 21, &command_socket);

    if(read_response(command_socket, buffer) != 0){
        printf("Error reading response\n");
        return 1;
    }

    printf("Response: %s\n", buffer);
    
    //USER
    bytes = write(command_socket, user, url.user_size+5+2);
    if(bytes < 0){
        perror("write()");
        exit(-1);
    }

    printf("%s\n", user);
    
    if(read_response(command_socket, buffer) != 0){
        printf("Error reading response\n");
        return 1;
    }

    printf("Response: %s\n", buffer);

    if(buffer[0] != '3' || buffer[1] != '3' || buffer[2] != '1'){
        printf("Error logging in\n");
        return 1;
    }

    printf("%s\n", password);

    //PASSWORD
    bytes = write(command_socket, password, url.password_size+5+2);
    if(bytes < 0){
        perror("write()");
        exit(-1);
    }
    if(read_response(command_socket, buffer) != 0){
        printf("Error reading response\n");
        return 1;
    }

    printf("Response: %s\n", buffer);

    if(buffer[0] != '2' || buffer[1] != '3' || buffer[2] != '0'){
        printf("Error logging in\n");
        return 1;
    }

    //PASSIVE MODE
    bytes = write(command_socket,"PASV\r\n",6);
    if(bytes < 0){
        perror("write()");
        exit(-1);
    }
    if(read_response(command_socket, buffer) != 0){
        printf("Error reading response\n");
        return 1;
    }

    printf("Response: %s\n", buffer);

    if(buffer[0] != '2' || buffer[1] != '2' || buffer[2] != '7'){
        printf("Error entering passive mode\n");
        return 1;
    }

    regex_t ipmatch;
    regmatch_t match[7];
    char* pattern = "227 Entering Passive Mode \\((.+),(.+),(.+),(.+),(.+),(.+)\\)";

    if (regcomp(&ipmatch, pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Failed to compile regex pattern\n");
        return 1;
    }

    char *iprcv = (char*) malloc(sizeof(char)*16);
    char *port1 = (char*) malloc(sizeof(char)*4);
    char *port2 = (char*) malloc(sizeof(char)*4);

    printf("Buffer: %s\n", buffer);

    if (regexec(&ipmatch, buffer, 7, match, 0) == 0){
        printf("Match with IP and Port\n");
        strncpy(iprcv, buffer + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
        iprcv[match[1].rm_eo-match[1].rm_so] = '.';
        strncpy(iprcv + match[1].rm_eo-match[1].rm_so+1, buffer + match[2].rm_so, match[2].rm_eo - match[2].rm_so);
        iprcv[match[2].rm_eo-match[1].rm_so] = '.';
        strncpy(iprcv + match[2].rm_eo-match[1].rm_so+1, buffer + match[3].rm_so, match[3].rm_eo - match[3].rm_so);
        iprcv[match[3].rm_eo-match[1].rm_so] = '.';
        strncpy(iprcv + match[3].rm_eo-match[1].rm_so+1, buffer + match[4].rm_so, match[4].rm_eo - match[4].rm_so);
        iprcv = realloc(iprcv, sizeof(char)*(match[4].rm_eo-match[1].rm_so)+1);
        strncpy(port1, buffer + match[5].rm_so, match[5].rm_eo - match[5].rm_so);
        strncpy(port2, buffer + match[6].rm_so, match[6].rm_eo - match[6].rm_so);
    }
    else {
        printf("No match\n");
        regfree(&ipmatch);
        return 1;
    }
    regfree(&ipmatch);

    int port = atoi(port1) * 256 + atoi(port2);

    printf("IP: %s\n", iprcv);
    printf("Port: %d\n", port);

    int read_socket;

    socket_create(iprcv, port, &read_socket);

    //RETR
    char *retr = (char*) malloc(sizeof(char)*(url.file_size+url.path_size+5+3));
    strcpy(retr, "RETR ");
    strcat(retr, url.path);
    strcat(retr, url.file);
    strcat(retr, "\r\n");

    printf("%s\n", retr);

    bytes = write(command_socket, retr,url.file_size+url.path_size+5+2);
    if(bytes < 0){
        perror("write()");
        exit(-1);
    }
    if(read_response(command_socket, buffer) != 0){
        printf("Error reading response\n");
        return 1;
    }
    if(buffer[0] != '1' || buffer[1] != '5' || buffer[2] != '0'){
        printf("Error\n");
        return 1;
    }
    //READ FILE
    FILE *file;
    file = fopen(url.file, "w");
    if(file == NULL){
        printf("Error opening file\n");
        return 1;
    }
    while((bytes = read(read_socket, buffer, 1024)) > 0){
        if(fwrite(buffer, 1, bytes, file) == 0){
            printf("Error writing to file\n");
            return 1;
        }
    }
    if(bytes < 0){
        perror("read()");
        exit(-1);
    }
    fclose(file);
    close(read_socket);

    if(read_response(command_socket, buffer) != 0){
        printf("Error reading response\n");
        return 1;
    }

    if(buffer[0] != '2' || buffer[1] != '2' || buffer[2] != '6'){
        printf("Error\n");
        return 1;
    }

    //QUIT
    bytes = write(command_socket,"QUIT\r\n",6);
    if(bytes < 0){
        perror("write()");
        exit(-1);
    }
    if(read_response(command_socket, buffer) != 0){
        printf("Error reading response\n");
        return 1;
    }
    if(buffer[0] != '2' || buffer[1] != '2' || buffer[2] != '1'){
        printf("Error quitting\n");
        return 1;
    }

    return 0;
}