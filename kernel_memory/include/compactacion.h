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
int iniciar_compactacion(t_log* logger,t_kernel_memory* km,int pid, int id_segmento, uint32_t tamano);
void avisar_compactacion(t_kernel_memory* km, t_log* logger);

//voy a utilizar para crear el segmento apenas finalice la compactacion
void Avisar_Compactacion_Ks(t_kernel_memory* km, t_log* logger,uint32_t tamano,int pid, int id_segmento);
void loguear_tablas_segmentos(t_log* logger);

#endif
