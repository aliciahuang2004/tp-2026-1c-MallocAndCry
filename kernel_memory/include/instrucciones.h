#ifndef INSTRUCCIONES_H
#define INSTRUCCIONES_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "kernel_memory.h"



void inicializar_proceso_memoria(int pid, char* path_relativo, t_kernel_memory* km);
char* obtener_instruccion(int pid, int pc, t_kernel_memory* km);


#endif