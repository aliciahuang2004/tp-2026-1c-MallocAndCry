#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H


#include "../../utils/include/utils.h"
#include <commons/config.h>
#include <commons/collections/dictionary.h>
#include <commons/string.h>
#include <string.h>


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
    t_dictionary* paths_por_pid;
} t_kernel_memory;

typedef struct {
    t_log* logger;
    int socket_conexion;
    int id;
    t_kernel_memory* km;
} t_hacerConnect;

t_kernel_memory* iniciar_kernelMemory(char* argv);
void verificarKernelMemory(t_kernel_memory* kernelMemory);
void* atender_conexion(void* arg);
int recibir_operacion(int socket_cliente);
void inicializar_proceso_memoria(int pid, char* path_relativo, t_kernel_memory* km);
char* obtener_instruccion(int pid, int pc, t_kernel_memory* km);

#endif /* KERNEL_MEMORY_H*/


