#ifndef MEMORY_STICK_H_
#define MEMORY_STICK_H_

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
    char* ip_escucha;
} t_memory_stick;

typedef struct 
{
  int socket_cliente;
  int cpu_id;  
  t_memory_stick* ms;
} t_cpu_context;


int conectar_al_kernelmem(t_memory_stick* ms);
void enviar_handshake (t_memory_stick* ms);
t_memory_stick* iniciar_memory_stick(char* ms_config,int tamano,int id);
void verificar_memory_stick(t_memory_stick* ms);
void destruir_memory_stick(t_memory_stick* ms);
void *rutina_operaciones(void *argumentos);
void rutina_recepcion(t_memory_stick* ms,int servidor_fd);
void procesar_lectura(t_memory_stick* ms, int socket_cliente, t_list* paquete);
void procesar_escritura(t_memory_stick* ms, int socket_cliente, t_list* paquete);
void* escuchar_kernel_memory(void* arg);

#endif /* MEMORY_STICK_H_ */