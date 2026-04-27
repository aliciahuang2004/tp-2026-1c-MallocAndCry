#ifndef CPU_H_
#define CPU_H_

#include "../../utils/include/utils.h"
#include <commons/config.h>


typedef struct {
    char* id;
    t_log* logger;
    t_config* config;
    char* log_level;
    char* ip_kernel_scheduler;
    char* puerto_kernel_scheduler;
    char* ip_kernel_memory;
    char* puerto_kernel_memory;
    char* ip_memory_stick_inicial;
    char* puerto_memory_stick_inicial;
    int socket_kernel_scheduler;
    int socket_kernel_memory;
    t_list* sockets_memory_sticks; // Para manejar múltiples sticks dinámicos
    
} t_cpu;


//estructura de registros
typedef struct {
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

}t_registros;

//estructura de contexto
typedef struct {// consultar 
    int pid;
    t_registros registros;
} t_contexto;

t_cpu* iniciar_cpu(char* path_config, char* id_cpu);
int conectar_kernel_memory(t_cpu* cpu);
int conectar_kernel_scheduler(t_cpu* cpu);
int conectar_memory_stick(t_cpu* cpu, char* ip, char* puerto);
void liberar_cpu(t_cpu* cpu);

void esperar_proceso(t_cpu* cpu);
t_contexto* solicitar_contexto(t_cpu* cpu, int pid);
void ciclo_de_instruccion(t_cpu *cpu,t_contexto* contexto);

#endif