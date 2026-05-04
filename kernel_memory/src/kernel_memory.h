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
    int socket_kernel_scheduler; //***PREGUNTAR SI QUEDA ACÁ O DEBERÍA MOVERLO A UN NUEVO STRUCT?
} t_kernel_memory;

typedef struct {
    t_log* logger;
    int socket_conexion;
    int id;
    t_kernel_memory* km;
} t_hacerConnect;

typedef struct {
    int pid;
    uint32_t PC; //4 bytes
    uint8_t AX; //1 byte
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t SI;
    uint32_t DI;
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
