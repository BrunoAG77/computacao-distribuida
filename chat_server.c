/*
    chat_server.c
    Servidor do mini chat multi clientes usando select()
    Compilação manual: gcc -Wall -Wextra -02 -o chat_server chat_server.c
    Execução: ./chat_server <port>

    Fluxo da execução:
     1) socket(AF_INEF, SOCK_STEAM, 0)
     2) bind(...)
     3) listen(...)
     4) loop: select(...) -> accept para novos clientes, recv() para mensagens
*/ 


#include <stdio.h>      // operações de i/o
#include <stdlib.h>     //exit, atoi...
#include <string.h>     //textos (memset, strncpy, strnlen, ...)
#include <unistd.h>     //close, read, wirte
#include <errno.h>      //códiggos globais
#include <sys/types.h>  //tipos básicos para o socket
#include <sys/socket.h> //socket
#include <netinet/in.h> //struct para htons, sockaddr_in
#include <arpa/inet.h>  //inet

// definir o número máx. de clients controlado pelo select
#define MAX_CLIENTS     FD_SETSIZE
#define BUF_SIZE        1024

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// função para enviar para todos os clientes conectados uma mensagem (exceto client_fd, que é quem enviou a mensagem)
static void broadcast(int *clients, int max_i, int sender_fd, const char *msg, size_t len) {
    for(int i = 0; i <= max_i; i++) {
        int fd = clients[i];
        if (fd >= 0 && fd != sender_fd) {
            // sockfd (descritor do socket destino), buf (ponteiro dos dados), tamanho da msg, flags
            ssize_t n = send(fd, msg, len, 0);
            if (fd < 0) { //se o client cai ou desconecta durante o processamento do broadcast
                perror("send");
            }
        }
    }
}

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stderr, "Use %s <porta>\nEx: %s 5001\n", argv[0], argv[0]);
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Porta inválida!!\n");
        return EXIT_FAILURE;
    }

    // criação do socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) die("socket");

    int yes = 1; // para habilitar o reuso da porta após o fechamento do servidor

    //o socket, level (nível de opção), otpname (reusar o não o endereço), optval, optlen
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) 
        die("setsockopt(SO_REUSEADDR)");


    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        die("bind");
    
    // listen
    if (listen(listen_fd, 8) < 0) //número máximo de conns pendentes
        die("listen");
    
    printf("\nServidor conectado e ouvindo em 0.0.0.0: %d ...\n", port);

    // vetor de clientes para guards os FDs
    int clients[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = -1; //aquele "slot" está livre
    }

    fd_set allset, rset;
    FD_ZERO(&allset);
    FD_SET(listen_fd, &allset);
    int maxfd = listen_fd;
    int max_i = -1;

    char buf[BUF_SIZE];

    for (;;) {
        rset = allset; // cópia (select modifica o set)
        // select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
        //  - nfds:     1 + maior descritor monitorado (maxfd + 1)
        //  - readfds:  conjunto de FDs para verificação de leitura pronta
        //  - writefds: (não usado aqui) NULL
        //  - exceptfds:(não usado) NULL
        //  - timeout:  (bloqueante) NULL para esperar indefinidamente
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready < 0) {
            if (errno == EINTR) continue; // interrompido por sinal
            die("select");
        }

        // Novo cliente chegando?
        if (FD_ISSET(listen_fd, &rset)) {
            struct sockaddr_in cliaddr;
            socklen_t clilen = sizeof(cliaddr);
            // accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
            //  - sockfd: socket em listen
            //  - addr:   (saída) endereço do cliente conectado (pode ser NULL)
            //  - addrlen:(entrada/saída) tamanho do addr; ajustado pelo kernel
            int connfd = accept(listen_fd, (struct sockaddr*)&cliaddr, &clilen);
            if (connfd < 0) {
                perror("accept");
            } else {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));
                printf("Novo cliente conectado %s:%d (fd=%d)\n", ip, ntohs(cliaddr.sin_port), connfd);

                // Adiciona na lista de clientes
                int i;
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i] < 0) {
                        clients[i] = connfd;
                        break;
                    }
                }
                if (i == MAX_CLIENTS) {
                    fprintf(stderr, "Muitos clientes, recusando.\n");
                    close(connfd);
                } else {
                    FD_SET(connfd, &allset);
                    if (connfd > maxfd) maxfd = connfd;
                    if (i > max_i) max_i = i;

                    const char *welcome = "Bem-vindo à calculadora!\n";
                    send(connfd, welcome, strlen(welcome), 0);
                }
            }
            if (--nready <= 0) continue; // nada mais pronto
        }

        // Verifica dados vindos dos clientes existentes
        for (int i = 0; i <= max_i; i++) {
            int fd = clients[i];
            if (fd < 0) continue;
            if (FD_ISSET(fd, &rset)) {
                ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
                // recv(int sockfd, void *buf, size_t len, int flags)
                //  - sockfd: socket do qual receber
                //  - buf:    buffer destino
                //  - len:    bytes máximos a ler
                //  - flags:  0 normal (sem flags)
                if (n <= 0) {
                    if (n < 0) perror("recv");
                    printf("Cliente fd=%d desconectou.\n", fd);
                    close(fd);
                    FD_CLR(fd, &allset);
                    clients[i] = -1;
                } else {
                    buf[n] = '\0'; // garantir string
                    // Simples broadcast
                   char operator[8];
                   double x, y, res;
                   char response[BUF_SIZE];

                   if (sscanf(buf,"%s %lf %lf",operator,&x,&y) == 3){
                    if (strcmp(operator, "ADD") == 0){
                        res = x + y;
                        snprintf(response, sizeof(response), "OK %.2f\n", res);
                    }
                    else if (strcmp(operator, "SUB") == 0){
                        res = x - y;
                        snprintf(response, sizeof(response), "OK %.2f\n", res);
                    }
                    else if (strcmp(operator, "MUL") == 0){
                        res = x * y;
                        snprintf(response, sizeof(response), "OK %.2f\n", res);
                    }
                    else if (strcmp(operator, "DIV") == 0){
                        if (y == 0) snprintf(response, sizeof(response), "ERR EZDV divisao_por_zero\n");
                        else{
                            res = x / y;
                            snprintf(response, sizeof(response), "OK %.2f\n", res);
                        }
                    } else snprintf(response, sizeof(response), "ERR operador_invalido\n");
                        
                   } else snprintf(response, sizeof(response), "ERR sintaxe_invalida\n");
                   send(fd, response, strlen(response), 0);

                }
                if (--nready <= 0) break; // nenhum FD restante pronto
            }
        }
    }

    close(listen_fd);
    return 0;
}