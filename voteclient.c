/*
Bruno Antico Galin 10417318
Gustavo Fugulin Soares da Silva 10418552
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF 256

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8080);
    serv.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock, (struct sockaddr*)&serv, sizeof(serv));

    char buf[BUF];
    recv(sock, buf, BUF-1, 0);  // WELCOME
    printf("%s", buf);

    int id;
    printf("Digite seu VOTER_ID: ");
    scanf("%d", &id);

    sprintf(buf, "HELLO %d\n", id);
    send(sock, buf, strlen(buf), 0);
    recv(sock, buf, BUF-1, 0);
    printf("%s", buf);

    while (1) {
        printf("\nComandos: LIST | VOTE <op> | SCORE | BYE | ADMIN CLOSE\n> ");
        scanf(" %[^\n]", buf);

        strcat(buf, "\n");
        send(sock, buf, strlen(buf), 0);

        int n = recv(sock, buf, BUF-1, 0);
        if (n <= 0) break;
        buf[n] = 0;
        printf("%s", buf);

        if (strncmp(buf, "BYE", 3) == 0) break;
    }

    close(sock);
    return 0;
}
