#include "../../utils/include/utils.h"
#include <netdb.h>		

#define COLOR_VERDE "\033[32m"

typedef struct {
    t_log*    logger;            //
    t_config* config;         //
    char*     log_level;          // 
    int        id;     // 
    char*      memoria;
    int        tamano;
    char* puerto_escucha;       //memory_stick recibe peticiones de las cpus  
    char* ip_kernel_memory;   //memory_stick conexion cliente a kernel_memory
    char* puerto_kernel_memory; //=
    int   kernel_mem_socket;    //socket de la conexion al servidor kernel memory
} t_memory_stick;

typedef struct 
{
  int socket_cliente;
  int cpu_id;  
} t_cpu_context;


int conectar_al_kernelmem(t_memory_stick* ms);
void enviar_handshake (t_memory_stick* ms);
t_memory_stick* iniciar_memory_stick(char* ms_config,int tamano,int id);
void verificar_memory_stick(t_memory_stick* ms);
void destruir_memory_stick(t_memory_stick* ms);
void *rutina_operaciones(void *arg);
void rutina_recepcion(t_memory_stick* ms,int servidor_fd);
#ifndef MEMORY_STICK_H_
#define MEMORY_STICK_H_



#endif /* MEMORY_STICK_H_ */