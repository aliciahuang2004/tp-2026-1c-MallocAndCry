#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "kernel_memory.h"

void* atender_conexion(void* arg);
void esperarConexiones(t_kernel_memory* kernelMemory, int kernel_memory_fd);
void agregar_cpu_conectada(int cpu_id,int socket_cliente);
void avisar_cpus_conectadas(int id,char* puerto, char* ip, t_log* logger, t_resultado_hueco resultado);
void remover_cpu_conectada(int socket_cliente, t_log* logger);

int buscar_bloques_libres_swap(int bloques_necesarios);
bool escribir_segmento_en_swap(void* contenido, uint32_t tam_seg, int bloque_inicio, int bloques_necesarios, t_log* logger);
void* leer_segmento_de_swap(int bloque_inicio, int bloques_necesarios, uint32_t tam_seg, t_log* logger);

int suspender_proceso_memoria(int pid, t_kernel_memory* km, t_log* logger);
int desuspender_proceso_memoria(int pid, t_kernel_memory* km, t_log* logger);
t_hueco* buscar_hueco_silencioso(uint32_t tamano, t_kernel_memory* km);
