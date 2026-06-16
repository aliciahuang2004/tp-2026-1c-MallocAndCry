#ifndef COMPACTACION_H
#define COMPACTACION_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "estructuras.h"
#include "kernel_memory.h"



void avisar_compactacion(t_kernel_memory* km, t_log* logger);
#endif