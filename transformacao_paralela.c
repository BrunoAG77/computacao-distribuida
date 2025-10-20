/*
Bruno Antico Galin 10417318
Gustavo Fugulin Soares da Silva 10418552
*/
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define DATA_SIZE 100
#define NUM_PROC 5

int transform_data(int x) {
    return x * x;
}

int main() {
    int id, P;
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    if (P != NUM_PROC) {
        if (id == 0) printf("Erro: este programa deve ser executado com exatamente %d processos.\n", NUM_PROC);
        MPI_Finalize();
        return 0;
    }

    int local_data[DATA_SIZE / P];
    int* original_data = NULL;
    int* gathered_data = NULL;
    double start;

    if (id == 0) {
        original_data = (int *)malloc(sizeof(int) * DATA_SIZE);
        for (int i = 0; i < DATA_SIZE; i++) original_data[i] = i + 1;
        gathered_data = (int *)malloc(sizeof(int) * DATA_SIZE);
        start = MPI_Wtime();
    }

    MPI_Scatter(original_data,DATA_SIZE / P,MPI_INT,local_data,DATA_SIZE / P,MPI_INT,0,MPI_COMM_WORLD);

    printf("Processo %d: ", id);
    for (int i = 0; i < DATA_SIZE / P; i++) {printf("%d ", local_data[i]);}
    printf("\n");

    for (int i = 0; i < DATA_SIZE / P; i++) {local_data[i] = transform_data(local_data[i]);}

    MPI_Gather(local_data,DATA_SIZE / P,MPI_INT,gathered_data,DATA_SIZE / P,MPI_INT,0,MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    if (id == 0) {
        double end = MPI_Wtime();
        double time = end - start;
        printf("\nVetor Original: ");
        for (int i = 0; i < DATA_SIZE; i++) printf("%d ", original_data[i]);
        printf("\n");

        printf("Vetor Transformado: ");
        for (int i = 0; i < DATA_SIZE; i++) printf("%d ", gathered_data[i]);
        printf("\n");

        printf("Tempo de execução: %.4f segundos\n", time);

        free(original_data);
        free(gathered_data);
    }

    MPI_Finalize();
    return 0;
}
