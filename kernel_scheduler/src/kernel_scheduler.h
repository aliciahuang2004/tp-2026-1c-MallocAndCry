#include "../../utils/include/utils.h"

typedef struct {
    t_log* logger;
    t_config* config;
    char* log_level;   
    int puerto_escucha;
    char* ip_kernel_memory;
    int puerto_kernel_memory;
} t_kernel_scheduler;