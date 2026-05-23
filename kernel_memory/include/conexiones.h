#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>
#include "kernel_memory.h"

void* atender_conexion(void* arg);
void esperarConexiones(t_kernel_memory* kernelMemory, int kernel_memory_fd);