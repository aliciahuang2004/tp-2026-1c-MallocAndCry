#ifndef CONTEXTOS_H
#define CONTEXTOS_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "estructuras.h"
#include "kernel_memory.h"
t_registros* crear_registros();
t_contexto* crear_contexto();
t_registros* solicitud_contexto(int pid);
#endif