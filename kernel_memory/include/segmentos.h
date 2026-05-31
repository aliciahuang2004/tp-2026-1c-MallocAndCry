#ifndef SEGMENTOS_H
#define SEGMENTOS_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "kernel_memory.h"
#include "estructuras.h"

/*
crear_segmento(); en proceso,pruebas
eliminar_segmento();
buscar_segmento();
actualizar_segmento();
*/

int crear_segmento(int pid, int id_segmento, uint32_t tamano,t_log* logger);
void loguear_segmentos_proceso(t_proceso* proceso, t_log* logger);

int eliminar_segmento(int pid, int id_segmento, t_log* logger);
#endif 