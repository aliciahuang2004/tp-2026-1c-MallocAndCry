#ifndef COMPACTACION_H
#define COMPACTACION_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "estructuras.h"
#include "kernel_memory.h"

bool ordenar_por_base_global(void* a, void* b);
t_list* obtener_lista_temp_ord(t_list* lista_segmentos_temp, t_log* logger);
int iniciar_compactacion(t_log* logger,t_kernel_memory* km);
void avisar_compactacion(t_kernel_memory* km, t_log* logger);

#endif
