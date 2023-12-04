#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

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
    if ((&sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(&sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    struct url url;
    if(argc != 2) {
        printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        return 1;
    }
    if(url_parser(argv[1], &url) != 0){
        printf("Error parsing url\n");
        return 1;
    }

    struct hostent *h;

    if ((h = gethostbyname(argv[1])) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    char *ip = inet_ntoa(*((struct in_addr *) h->h_addr));
    
    int sock21;

    socket_create(ip, 21, &sock21);

    





    return 0;
}