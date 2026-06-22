#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "kernel_memory.h"

void* atender_conexion(void* arg);
void esperarConexiones(t_kernel_memory* kernelMemory, int kernel_memory_fd);
void agregar_cpu_conectada(int cpu_id,int socket_cliente);
void avisar_cpus_conectadas(int ms_id,char* ms_puerto,char* ms_ip,t_log* logger); 
void remover_cpu_conectada(int socket_cliente, t_log* logger);
