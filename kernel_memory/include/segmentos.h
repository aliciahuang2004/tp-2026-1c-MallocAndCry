#ifndef SEGMENTOS_H
#define SEGMENTOS_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "kernel_memory.h"
#include "estructuras.h"

/*
buscar_segmento();
actualizar_segmento();--esto creo que no puede ocurrir
*/

int crear_segmento(int pid, int id_segmento, uint32_t tamano,t_log* logger,t_kernel_memory* km);
void loguear_segmentos_proceso(t_proceso* proceso, t_log* logger);
int eliminar_segmento(int pid, int id_segmento, t_log* logger);

#endif 