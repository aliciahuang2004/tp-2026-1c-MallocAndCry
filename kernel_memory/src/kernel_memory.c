#define _GNU_SOURCE

#include "kernel_memory.h"
#include <unistd.h>

int main(int argc, char* argv[]) {

    if (argc != 2 ){
        printf("Uso: ./bin/kernel_memory [Archivo Config]\n");
        return EXIT_FAILURE;
    }

    //INICIA LOGGER TEMPORAL, CONFIG Y LOGGER
    t_kernel_memory* kernel_memory = iniciar_kernelMemory(argv[1]);

    //VERIFICA LOS DATOS CARGADOS
    verificarKernelMemory(kernel_memory);
    
    //INICIA SERVIDOR
    int kernel_memory_fd = iniciar_servidor(kernel_memory->puerto_escucha);
    log_debug(kernel_memory->logger, "Servidor listo para recibir una conexion - FD: %i / puerto: %s" , kernel_memory_fd, kernel_memory->puerto_escucha);

    while (1) {

        pthread_t thread;

        int fd_conexion_kernel_memory = esperar_cliente(kernel_memory_fd);
        log_debug(kernel_memory->logger, "Esperando que se conecte un cliente");

        t_hacerConnect* datosConexion = malloc(sizeof(t_hacerConnect));
        datosConexion->logger = kernel_memory->logger;

        datosConexion->socket_conexion = fd_conexion_kernel_memory;

        int err= pthread_create(&thread,
                        NULL,
                        atender_conexion, // va a llamar a atender conexion pasando parametros
                        datosConexion); // de este struct
        if (err != 0){
            log_debug(kernel_memory->logger, "Hubo un problema al crear el hilo");
        }
        pthread_detach(thread);
        
    }

    return 0;
}

void* atender_conexion(void* arg) {
    t_hacerConnect* datos = (t_hacerConnect*) arg;

    int socket_cliente = datos->socket_conexion;
    t_log* logger = datos->logger;

    log_info(logger, "Nuevo hilo atendiendo conexión en socket %d", socket_cliente);

    // Bucle principal de atención: mientras el cliente esté conectado
    while (1) {

        t_list* paquete = recibir_paquete(socket_cliente);
        if (!paquete) {
            log_error(logger, "Error al recibir paquete en socket %d. Cerrando conexión.", socket_cliente);
            break; // salimos del bucle en caso de fallar
        }

        int codigo_operacion = *(int*) list_get(paquete, 0);

        switch (codigo_operacion) {
            case CPU_HANDSHAKE:
                log_debug(logger, "[Socket %d] Operación CPU recibida", socket_cliente);
                // CODIGO CPU
                break;

            case MEMORY_STICK_HANDSHAKE:
                log_debug(logger, "[Socket %d] Operación MMEMORY STICK recibida", socket_cliente);
                // CODIGO MEMORY STICK
                break;

            case KERNEL_SCHEDULER_HANDSHAKE:
                log_debug(logger, "[Socket %d] Operación KERNEL SCHEDULER recibida", socket_cliente);
                 // CODIGO KERNEL SCHEDULER

            case SWAP_HANDSHAKE: // ejemplo: operación enviada por el SWAP
                log_debug(logger, "[Socket %d] Operación SWAP recibida", socket_cliente);
                // CODIGO SWAP
                break;

            default:
                log_error(logger, "[Socket %d] Código de operación desconocido: %d", socket_cliente, codigo_operacion);
                break;
        }

        // Liberar memoria del paquete
        list_destroy_and_destroy_elements(paquete, free);
    }

    // Cerrar el socket por un error o cliente se desconecta
    close(socket_cliente);
    log_info(logger, "Conexión cerrada en socket %d", socket_cliente);

    // Liberar estructura de datos de conexion
    free(datos);

    return NULL;
}

t_kernel_memory* iniciar_kernelMemory(char* argv){

    t_kernel_memory* kernelMemory = malloc(sizeof(t_kernel_memory));

    t_log* logger_temp= iniciar_logger("kernelMemory.log", "[KERNEL_MEMORY_INIT]", true, LOG_LEVEL_INFO);

    kernelMemory -> config = iniciar_config(logger_temp, argv);

    kernelMemory -> log_level = strdup(config_get_string_value(kernelMemory -> config, "LOG_LEVEL"));
    t_log_level nivel = obtener_log_level(kernelMemory->log_level);

    kernelMemory -> logger = iniciar_logger("kernelMemory.log", "[KERNEL_MEMORY]", true, nivel);

    log_destroy(logger_temp);

    kernelMemory -> puerto_escucha = config_get_string_value(kernelMemory->config, "PUERTO_ESCUCHA");
    kernelMemory -> allocation_strategy = config_get_string_value(kernelMemory->config, "ALLOCATION_STRATEGY");
    kernelMemory -> scripts_basePath = config_get_string_value(kernelMemory->config, "SCRIPTS_BASEPATH");
    kernelMemory -> segment_max_size = config_get_int_value(kernelMemory->config, "SEGMENT_MAX_SIZE");
    kernelMemory -> instruction_delay = config_get_int_value(kernelMemory->config, "INSTRUCTION_DELAY");
    kernelMemory -> compaction_delay = config_get_int_value(kernelMemory->config, "COMPACTION_DELAY");
    
    log_debug(kernelMemory->logger, "Kernel Memory inicializado correctamentew");
    
    return kernelMemory;
    
}

void verificarKernelMemory(t_kernel_memory* kernelMemory){
    log_debug(kernelMemory->logger, "Kernel Memory cargado con los siguientes datos");
    log_debug(kernelMemory->logger, "SEGMENT_MAX_SIZE; %d", kernelMemory -> segment_max_size);
    log_debug(kernelMemory->logger, "ALLOCATION_STRATEGY; %s", kernelMemory -> allocation_strategy);
    log_debug(kernelMemory->logger, "INSTRUCTION_DELAY; %d", kernelMemory -> instruction_delay);
    log_debug(kernelMemory->logger, "COMPACTION_DELAY; %d", kernelMemory -> compaction_delay);
    log_debug(kernelMemory->logger, "SCRIPTS_BASEPATH; %s", kernelMemory -> scripts_basePath);
}

