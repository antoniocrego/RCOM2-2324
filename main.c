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
    char *port;
    char *path;
};

int url_parser(char *str, struct url *url) {
    regex_t regex_user;
    regex_t regex_nouser;
    char *nouserpattern = "(ftp)://([^/]*)(/.*)";
    char *userpattern = "(ftp)://([^:]*):([^@]*)@([^/]*)(/.*)";
    regmatch_t match[6];
    if (regcomp(&regex_user, userpattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Failed to compile regex pattern\n");
        return 1;
    }
    if (regcomp(&regex_nouser, nouserpattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Failed to compile regex pattern\n");
        return 1;
    }
    if (regexec(&regex_user, str, 6, match, 0) == 0){
        printf("Match with user\n");
        url->user = malloc(sizeof(char) * (match[2].rm_eo - match[2].rm_so));
        url->password = malloc(sizeof(char) * (match[3].rm_eo - match[3].rm_so));
        url->host = malloc(sizeof(char) * (match[4].rm_eo - match[4].rm_so));
        url->path = malloc(sizeof(char) * (match[5].rm_eo - match[5].rm_so));
        strncpy(url->user, str + match[2].rm_so, match[2].rm_eo - match[2].rm_so);
        strncpy(url->password, str + match[3].rm_so, match[3].rm_eo - match[3].rm_so);
        strncpy(url->host, str + match[4].rm_so, match[4].rm_eo - match[4].rm_so);
        strncpy(url->path, str + match[5].rm_so, match[5].rm_eo - match[5].rm_so);
    }
    else if (regexec(&regex_nouser, str, 4, match, 0) == 0){
        printf("Match without user\n");
        url->host = malloc(sizeof(char) * (match[2].rm_eo - match[2].rm_so));
        url->path = malloc(sizeof(char) * (match[3].rm_eo - match[3].rm_so));
        strncpy(url->host, str + match[2].rm_so, match[2].rm_eo - match[2].rm_so);
        strncpy(url->path, str + match[3].rm_so, match[3].rm_eo - match[3].rm_so);
        // path is acting weird when its just /
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
    while(byte != '\n'){
        if(read(sockfd, &byte, 1) < 0){
            perror("read()");
            exit(-1);
        }
        buffer[i] = byte;
        i++;
    }
    if(buffer[3] == '-'){
        while(buffer[3] == '-'){
            i = 0;
            while(byte != '\n'){
                if(read(sockfd, &byte, 1) < 0){
                    perror("read()");
                    exit(-1);
                }
                buffer[i] = byte;
                i++;
            }
        }
    }
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

    char *user;
    char *password;
    strcat(user, "USER ");
    strcat(user, url.user);
    strcat(user, "\r\n");
    strcat(password, "PASS ");
    strcat(password, url.password);
    strcat(password, "\r\n");


    struct hostent *h;

    if ((h = gethostbyname(argv[1])) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    char *ip = inet_ntoa(*((struct in_addr *) h->h_addr));
    
    int command_socket;

    socket_create(ip, 21, &command_socket);
    
    //USER
    bytes = write(command_socket,&user,sizeof(&user));
    if(bytes < 0){
        perror("write()");
        exit(-1);
    }

    if(read_response(command_socket, buffer) != 0){
        printf("Error reading response\n");
        return 1;
    }
    if(buffer[0] != '3' || buffer[1] != '3' || buffer[2] != '1'){
        printf("Error logging in\n");
        return 1;
    }

    //PASSWORD
    bytes = write(command_socket,&password,sizeof(&password));
    if(bytes < 0){
        perror("write()");
        exit(-1);
    }
    if(read_response(command_socket, buffer) != 0){
        printf("Error reading response\n");
        return 1;
    }
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
    if(buffer[0] != '2' || buffer[1] != '2' || buffer[2] != '7'){
        printf("Error entering passive mode\n");
        return 1;
    }

    int ip1, ip2, ip3, ip4, port1, port2;

    sscanf(buffer, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2);

    int port = port1 * 256 + port2;

    int read_socket;

    ip = "";
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

    socket_create(ip, port, &read_socket);

    //RETR
    char *retr;
    strcat(retr, "RETR ");
    strcat(retr, url.path);
    strcat(retr, "\r\n");
    bytes = write(command_socket,&retr,sizeof(&retr));
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
    file = fopen("url.file.txt", "w");
    if(file == NULL){
        printf("Error opening file\n");
        return 1;
    }
    while((bytes = read(read_socket, buffer, 1024)) > 0){
        if(fwrite(buffer, 1, bytes, file) < 0){
            printf("Error writing to file\n");
            return 1;
        }
    }
    if(bytes < 0){
        perror("read()");
        exit(-1);
    }
    fclose(file);

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