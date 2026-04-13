#include "main.h"

int main(int argc, char* argv[]) {

    t_kernel_memory* kernel_memory = iniciar_kernelMemory(argv);

    verificarKernelMemory(kernel_memory);

    int master_fd = iniciar_servidor(kernel_memory -> puerto_escucha);
    log_debug(kernel_memory->logger, "Servidor listo para recibir una conexion - FD: %i / puerto: %s" , master_fd, kernel_memory -> puerto_escucha);

    

    return 0;
}

t_kernel_memory* iniciar_kernelMemory(char* argv[]){
    t_kernel_memory* kernelMemory = malloc(sizeof(t_kernel_memory));

    t_log* logger_temp= iniciar_logger("kernelMemory.log", "[KERNEL_MEMORY_INIT]", true, LOG_LEVEL_INFO);

    kernelMemory -> config = iniciar_config(logger_temp, argv[1]);

    kernelMemory -> log_level = stdrup(config_get_string_value(kernelMemory -> config, "LOG_LEVEL"));
    t_log_level nivel = obtener_log_level(kernelMemory->log_level);

    kernelMemory -> logger = iniciar_logger("kernelMemory.log", "[KERNEL_MEMORY]", true, nivel);

    log_destroy(logger_temp);

    kernelMemory -> puerto_escucha = config_get_string_value(kernelMemory->config, "PUERTO_ESCUCHA");
    kernelMemory -> allocation_strategy = config_get_string_value(kernelMemory->config, "ALLOCATION_STRATEGY");
    kernelMemory -> scripts_basePath = config_get_string_value(kernelMemory->config, "SCRIPTS_BASEPATH");
    kernelMemory -> segment_max_size = config_get_int_value(kernelMemory->config, "SEGMENT_MAX_SIZE");
    kernelMemory -> instruction_delay = config_get_int_value(kernelMemory->config, "INSTRUCTION_DELAY");
    kernelMemory -> compaction_delay = config_get_int_value(kernelMemory->config, "COMPACTION_DELAY");
    
    return kernelMemory;
    
}

void verificarKernelMemory(t_kernel_memory* kernelMemory){
    log_debug(kernelMemory->logger, "Kernel Memory inicializado correctamente");
    log_debug(kernelMemory->logger, "SEGMENT_MAX_SIZE; %d", kernelMemory -> segment_max_size;
    log_debug(kernelMemory->logger, "ALLOCATION_STRATEGY; %s", kernelMemory -> allocation_strategy;
    log_debug(kernelMemory->logger, "INSTRUCTION_DELAY; %d", kernelMemory -> instruction_delay;
    log_debug(kernelMemory->logger, "COMPACTION_DELAY; %d", kernelMemory -> compaction_delay;
    log_debug(kernelMemory->logger, "SCRIPTS_BASEPATH; %s", kernelMemory -> scripts_basePath;
}