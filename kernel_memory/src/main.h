#ifndef MAIN_H
#define MAIN_H

#include "../../utils/include/utils.h"
#include <commons/config.h>

typedef struct {
    t_log* logger;
    t_config* config;
    char* log_level;
    int segment_max_size;
    char* allocation_strategy;
    int instruction_delay;
    int compaction_delay;
    char* scripts_basePath;
    char* puerto_escucha;
} t_kernel_memory;


t_kernel_memory* iniciar_kernelMemory(char* argv[]);
void verificarKernelMemory(t_kernel_memory* kernelMemory);


#endif /* MAIN_H*/
