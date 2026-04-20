#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#define _GNU_SOURCE

#include "../../utils/include/utils.h"
#include <commons/config.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

typedef struct {
    t_log* logger;
    t_config* config;
    char* log_level;
    int segment_max_size;
    char* allocation_strategy;
    int instruction_delay;
    int compaction_delay;
    char* scripts_basePath;
    char* puerto_escucha;
} t_kernel_memory;

typedef struct {
    t_log* logger;
    int socket_conexion;
    int id;
} t_hacerConnect;

t_kernel_memory* iniciar_kernelMemory(char* argv);
void verificarKernelMemory(t_kernel_memory* kernelMemory);
void* atender_conexion(void* arg);
int recibir_operacion(int socket_cliente);
void esperarConexiones(t_kernel_memory* kernelMemory, int kernel_memory_fd);

#endif /* KERNEL_MEMORY_H */
