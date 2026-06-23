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
#include "estructuras.h"
#include <commons/bitarray.h>

extern t_bitarray* bitmap_swap;
extern int swap_block_size;
extern int km_socket_swap;

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
    t_dictionary* paths_por_pid;//
    int socket_kernel_scheduler; //***PREGUNTAR SI QUEDA ACÁ O DEBERÍA MOVERLO A UN NUEVO STRUCT?
} t_kernel_memory;

typedef struct {
    t_log* logger;
    int socket_conexion;
    int id;
    t_kernel_memory* km;
} t_hacerConnect;

//GLOBALES
extern t_dictionary* procesos;

extern t_list* lista_huecos_libres;

extern t_list* lista_ms;

extern t_list* bloques_swap;

extern t_dictionary* tabla_contextos;

extern uint32_t memoria_total;

extern t_list* lista_dir_global_ms;

extern t_list* cpus_conectadas;


//MUTEX
extern pthread_mutex_t mutex_procesos;

extern pthread_mutex_t mutex_huecos;

extern pthread_mutex_t mutex_lista_ms;

extern pthread_mutex_t mutex_swap;

extern pthread_mutex_t mutex_tabla_contextos;

extern pthread_mutex_t mutex_memoria_total;

extern pthread_mutex_t mutex_lista_dir_global_ms;

extern pthread_mutex_t mutex_cpus_conectadas;


t_kernel_memory* iniciar_kernelMemory(char* argv);
void verificarKernelMemory(t_kernel_memory* kernelMemory);
int recibir_operacion(int socket_cliente);
void enviar_operacion(int socket_cliente, op_code codigo);
uint32_t aumentar_memoria_total(uint32_t tamano);

#endif /* KERNEL_MEMORY_H */
