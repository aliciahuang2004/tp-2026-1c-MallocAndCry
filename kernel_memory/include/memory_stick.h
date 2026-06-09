#ifndef MEMORY_STICK_H
#define MEMORY_STICK_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "estructuras.h"
#include <commons/log.h>
#include "kernel_memory.h"

void manejar_desconexion_memory_stick(t_ms_info* ms, t_kernel_memory* km, t_log* logger);
int nuevo_memory_stick(int ms_id, int ms_tamano, int socket_cliente);
t_ms_info* buscar_ms_por_socket(int socket);
void agregar_posicion_ms(t_resultado_hueco r,int ms_id,t_log* logger);
#endif 