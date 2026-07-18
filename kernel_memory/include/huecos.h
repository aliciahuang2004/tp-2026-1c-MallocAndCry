#ifndef HUECOS_H
#define HUECOS_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "kernel_memory.h"
/*
fusionar_huecos();
*/

t_resultado_hueco agregar_hueco_libre(uint32_t base, uint32_t tamano);
void agregar_hueco_ms(uint32_t base, uint32_t tamano,t_log* logger);
t_resultado_hueco agregar_hueco_sinmtx(uint32_t base, uint32_t tamano);
t_hueco* buscar_hueco_best_fit(uint32_t tamano,t_kernel_memory* km,t_log* logger,int pid, int id_segmento);
t_hueco* buscar_hueco_worst_fit(uint32_t tamano,t_kernel_memory* km,t_log* logger,int pid, int id_segmento);
t_hueco* buscar_hueco(uint32_t tamano,t_kernel_memory* km,t_log* logger,int pid, int id_segmento);
void quitar_y_liberar_hueco(int index);

void vaciar_lista_de_huecos(t_log* logger);
void consumir_hueco(t_hueco* hueco, uint32_t tamano);
void loguear_huecos(t_log* logger);

#endif 