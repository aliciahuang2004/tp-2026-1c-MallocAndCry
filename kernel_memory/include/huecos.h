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

void agregar_hueco_libre(uint32_t base, uint32_t tamano);
t_hueco* buscar_hueco_best_fit(uint32_t tamano,t_log* logger);
t_hueco* buscar_hueco_worst_fit(uint32_t tamano,t_log* logger);
t_hueco* buscar_hueco(uint32_t tamano,t_log* logger);

void consumir_hueco(t_hueco* hueco, uint32_t tamano);
void loguear_huecos(t_log* logger);

#endif 