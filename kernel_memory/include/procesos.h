#ifndef PROCESOS_H
#define PROCESOS_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include "estructuras.h"
#include "kernel_memory.h"

int crear_proceso(int pid);
t_proceso* buscar_proceso(int pid);
void finalizar_proceso(int pid);
void iniciar_tabla_procesos(void);
int eliminar_proceso(int pid, t_kernel_memory* km, t_log* logger);
#endif 