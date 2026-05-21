#ifndef CPU_H_
#define CPU_H_

#include "../../utils/include/utils.h"
#include <commons/config.h>
#include <stdbool.h>


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
typedef enum {
    INST_NOOP, 
    INST_SET, 
    INST_MOV_IN, 
    INST_MOV_OUT, 
    INST_SUM, 
    INST_SUB, 
    INST_JNZ, 
    INST_COPY_MEM,
    INST_MUTEX_CREATE, 
    INST_MUTEX_LOCK, 
    INST_MUTEX_UNLOCK,
    INST_MEM_ALLOC, 
    INST_MEM_FREE, 
    INST_SLEEP,
    INST_STDOUT, 
    INST_STDIN, 
    INST_INIT_PROC, 
    INST_EXIT,
    INST_DESCONOCIDA 
} t_codigo_instruccion;

typedef struct {
    t_codigo_instruccion identificador_operacion;
    char* nombre_operacion;
    char* argumento_operando_destino;  
    char* argumento_operando_origen;   
} t_instruccion_decodificada;

t_instruccion_decodificada decodificar_instruccion(t_cpu* cpu, char* cadena_instruccion_texto);

t_cpu* iniciar_cpu(char* path_config, char* id_cpu);
int conectar_kernel_memory(t_cpu* cpu);
int conectar_kernel_scheduler(t_cpu* cpu);
int conectar_memory_stick(t_cpu* cpu, char* ip, char* puerto);
void liberar_cpu(t_cpu* cpu);

void esperar_proceso(t_cpu* cpu);
t_contexto* solicitar_contexto(t_cpu* cpu, int pid);
void ciclo_de_instruccion(t_cpu *cpu,t_contexto* contexto);

char* fetch_instruccion(t_cpu* cpu, t_contexto* contexto);
bool hay_interrupcion_pendiente(int socket_fd);
void enviar_contexto_a_memoria(t_cpu* cpu, t_contexto* contexto);
void devolver_proceso_interrumpido(t_cpu* cpu, int pid, op_code motivo_desalojo);
#endif