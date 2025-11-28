/*
Bruno Antico Galin 10417318
Gustavo Fugulin Soares da Silva 10418552
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#define MAX_CLIENTS 50
#define MAX_OPTIONS 10
#define BUF 256

// Inicialização de variáveis
char *opcoes[] = {"A", "B", "C"};
int votos[MAX_OPTIONS] = {0};
int n_opcoes = 3;

int votou_id[MAX_CLIENTS];
int total_ids = 0;

int encerrada = 0;

pthread_mutex_t lock;
pthread_mutex_t loglock;

FILE *logfile;


// Registro de comandos no log
void registrar_log(const char *msg) {
    pthread_mutex_lock(&loglock);

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char stamp[64];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", tm);

    fprintf(logfile, "[%s] %s\n", stamp, msg);
    fflush(logfile);

    pthread_mutex_unlock(&loglock);
}

int ja_votou(int id) {
    for (int i = 0; i < total_ids; i++)
        if (votou_id[i] == id)
            return 1;
    return 0;
}

void registrar_id(int id) {
    votou_id[total_ids++] = id;
}

void salvar_resultado_final() {
    FILE *f = fopen("resultado_final.txt", "w");
    fprintf(f, "Resultado Final:\n");
    for (int i = 0; i < n_opcoes; i++)
        fprintf(f, "%s: %d\n", opcoes[i], votos[i]);
    fclose(f);
}

void enviar_score(int sock) {
    char resp[BUF] = {0};
    sprintf(resp, "SCORE %d ", n_opcoes);

    for (int i = 0; i < n_opcoes; i++) {
        char temp[32];
        sprintf(temp, "%s:%d ", opcoes[i], votos[i]);
        strcat(resp, temp);
    }

    strcat(resp, "\n");
    send(sock, resp, strlen(resp), 0);
}


// Thread do cliente
void *client_handler(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char buf[BUF];
    int voter_id = -1;

    send(sock, "WELCOME\n", 8, 0);

    while (1) {
        int n = recv(sock, buf, BUF - 1, 0);
        if (n <= 0) break;

        buf[n] = '\0';

        // HELLO <id>
        if (strncmp(buf, "HELLO", 5) == 0) {
            sscanf(buf, "HELLO %d", &voter_id);

            char msg[64];
            sprintf(msg, "WELCOME %d\n", voter_id);
            send(sock, msg, strlen(msg), 0);

            char logmsg[64];
            sprintf(logmsg, "VOTER %d conectado", voter_id);
            registrar_log(logmsg);
        }

        // LIST
        else if (strncmp(buf, "LIST", 4) == 0) {
            char msg[BUF] = {0};
            sprintf(msg, "OPTIONS %d ", n_opcoes);

            for (int i = 0; i < n_opcoes; i++) {
                strcat(msg, opcoes[i]);
                strcat(msg, " ");
            }

            strcat(msg, "\n");
            send(sock, msg, strlen(msg), 0);
        }

        // VOTE <op>
        else if (strncmp(buf, "VOTE", 4) == 0) {
            if (encerrada) {
                send(sock, "ERR CLOSED\n", 11, 0);
                continue;
            }

            char op[32];
            sscanf(buf, "VOTE %s", op);

            pthread_mutex_lock(&lock);

            if (ja_votou(voter_id)) {
                send(sock, "ERR DUPLICATE\n", 14, 0);
            } else {
                int ok = 0;
                for (int i = 0; i < n_opcoes; i++) {
                    if (strcmp(op, opcoes[i]) == 0) {
                        votos[i]++;
                        ok = 1;
                        registrar_id(voter_id);

                        char logmsg[64];
                        sprintf(logmsg, "VOTER %d votou em %s", voter_id, op);
                        registrar_log(logmsg);
                        break;
                    }
                }

                if (ok) {
                    char msg[64];
                    sprintf(msg, "OK VOTED %s\n", op);
                    send(sock, msg, strlen(msg), 0);
                } else {
                    send(sock, "ERR INVALID_OPTION\n", 20, 0);
                }
            }

            pthread_mutex_unlock(&lock);
        }

        // SCORE
        else if (strncmp(buf, "SCORE", 5) == 0) {
            pthread_mutex_lock(&lock);
            enviar_score(sock);
            pthread_mutex_unlock(&lock);
        }

        // ADMIN CLOSE
        else if (strncmp(buf, "ADMIN CLOSE", 11) == 0) {
            pthread_mutex_lock(&lock);

            if (!encerrada) {
                encerrada = 1;
                registrar_log("ELEICAO ENCERRADA PELO ADMIN");
                salvar_resultado_final();
            }

            pthread_mutex_unlock(&lock);

            send(sock, "CLOSED FINAL\n", 13, 0);
        }

        // BYE
        else if (strncmp(buf, "BYE", 3) == 0) {
            send(sock, "BYE\n", 4, 0);
            break;
        }
    }

    close(sock);
    return NULL;
}


// main
int main() {
    logfile = fopen("eleicao.log", "a");

    memset(votou_id, -1, sizeof(votou_id));

    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&loglock, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, MAX_CLIENTS);

    printf("Servidor iniciado na porta 8080...\n");
    registrar_log("Servidor iniciado");

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        int *sock_ptr = malloc(sizeof(int));
        *sock_ptr = client_fd;

        pthread_t t;
        pthread_create(&t, NULL, client_handler, sock_ptr);
        pthread_detach(t);
    }

    fclose(logfile);
    return 0;
}
