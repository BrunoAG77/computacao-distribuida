/*
Bruno Antico Galin 10417318
Gustavo Fugulin Soares da Silva 10418552
*/
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int rank, size;
    int N = 40;
    int *dados = NULL;
    int *subvetor;
    int i, n_local;
    int soma_local = 0, soma_global = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    n_local = N / size; // quantos elementos cada processo recebe
    subvetor = (int*) malloc(n_local * sizeof(int));

    if (rank == 0) {
        dados = (int*) malloc(N * sizeof(int));  // cria vetor de 1 até N
        for (i = 0; i < N; i++) dados[i] = i + 1;
    }

    // distribui os blocos para cada processo
    MPI_Scatter(dados, n_local, MPI_INT, subvetor, n_local, MPI_INT, 0, MPI_COMM_WORLD);

    // imprime o que cada processo recebeu
    printf("Processo %d recebeu:", rank);
    for (i = 0; i < n_local; i++) {
        printf(" %d", subvetor[i]);
        soma_local += subvetor[i] * subvetor[i];
    }
    printf("\n");

    // imprime soma local
    printf("Processo %d: soma local dos quadrados = %d\n", rank, soma_local);

    // reduz todas as somas no processo 0
    MPI_Reduce(&soma_local, &soma_global, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        int soma_seq = 0;
        for (i = 1; i <= N; i++) soma_seq += i * i;
        printf("\nProcesso 0: soma paralela dos quadrados = %d\n", soma_global);
        printf("Processo 0: soma sequencial esperada = %d\n", soma_seq);
        if (soma_seq == soma_global) printf("\n✅ Os valores conferem!\n");
    }

    if (rank == 0) free(dados);
    free(subvetor);
    MPI_Finalize();
    return 0;
}
