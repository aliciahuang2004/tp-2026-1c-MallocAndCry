#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#define _GNU_SOURCE

#include "../../utils/include/utils.h"
#include <commons/config.h>
#include <commons/collections/dictionary.h>
#include <commons/string.h>
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
    t_dictionary* paths_por_pid;
} t_kernel_memory;

typedef struct {
    t_log* logger;
    int socket_conexion;
    int id;
    t_kernel_memory* km;
} t_hacerConnect;

typedef struct {
    int id;
    int pc;
    char estado;
    int seginicio;
    int seglimite;
    //VEO SI ES NECESARIO AGREGAR MAS DATOS
} Contexto;

typedef struct 
{
    int id;
    int tamano;
    int socket;
}t_ms_info;

t_kernel_memory* iniciar_kernelMemory(char* argv);
void verificarKernelMemory(t_kernel_memory* kernelMemory);
void* atender_conexion(void* arg);
int recibir_operacion(int socket_cliente);
void inicializar_proceso_memoria(int pid, char* path_relativo, t_kernel_memory* km);
char* obtener_instruccion(int pid, int pc, t_kernel_memory* km);
void enviar_operacion(int socket_cliente, op_code codigo);
int crear_CTX(int pid);
void esperarConexiones(t_kernel_memory* kernelMemory, int kernel_memory_fd);
void debug_lista_contextos(t_list* lista);

extern t_list *lista_contextos;
extern pthread_mutex_t mutex_lista_contextos;
extern t_list* lista_ms;

#endif /* KERNEL_MEMORY_H */
