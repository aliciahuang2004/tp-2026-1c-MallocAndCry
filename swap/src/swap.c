#include "swap.h"

t_swap* inicializar_swap(int argc, char* argv[] ) {

    t_swap* sp= malloc(sizeof(t_swap));

    t_log* logger_temp= iniciar_logger("swap.log", "[SWAP_INIT]", true, LOG_LEVEL_INFO);

    sp-> config= iniciar_config(logger_temp, argv[1]);

    sp-> log_level= stdrup(config_get_string_value(sp->config, "LOG_LEVEL"));
    t_log_level nivel = obtener_log_level(sp->log_level);

    sp-> logger= iniciar_logger("swap.log", "[SWAP]", true, nivel);

    log_destroy(logger_temp);

    sp-> swap_file_path= config_get_string_value(sp->config, "SWAP_FILE_PATH");
    sp-> swap_file_size= config_get_int_value(sp->config, "SWAP_FILE_SIZE");
    sp-> block_size= config_get_int_value(sp->config, "BLOCK_SIZE");
    sp-> ip_kernel_memory= config_get__value(sp->config, "IP_KERNEL_MEMORY");
    sp-> puerto_kernel_memory= config_get_int_value(sp->config, "PUERTO_KERNEL_MEMORY");
    sp-> socket_kernel_memory= -1;
    return sp;
}

void verificar_swap(t_swap* sp){
    log_debug(sp->loger, "SWAP inicializado correctamente");
    log_debug(sp->logger, "Archivo de swap: %s", sp->swap_file_path);
    log_debug(sp->logger, "Tamaño del swap: %d bytes", sp->swap_file_size);
    log_debug(sp->logger, "Tamaño del bloque: %d bytes", sp->block_size);
    log_debug(sp->logger, "IP del Kernel Memory: %s", sp->ip_kernel_memory);
    log_debug(sp->logger, "Puerto del Kernel Memory: %s", sp->puerto_kernel_memory);
}

void liberar_swap(t_swap* sp){
    if(!sp) return;

    if(sp->logger) log_destroy(sp->logger);
    if(sp->config) config_destroy(sp->config);
    if(sp->swap_file_path) free(sp->swap_file_path);
    if(sp->ip_kernel_memory) free(sp->ip_kernel_memory);
    if(sp->log_level) free(sp->log_level);
    free(sp);
}

int conectar_a_kernel_memory(t_swap* sp){
    char* puerto_str = string_itoa(sp->puerto_kernel_memory);

    sp->socket_kernel_memory = crear_conexion(sp->logger, sp->ip_kernel_memory, puerto_str);

    free(puerto_str);

    if(sp->socket_kernel_memory != -1){
        log_info(sp->logger, "Conexión exitosa a Kernel Memory en %s:%d", sp->ip_kernel_memory, sp->puerto_kernel_memory);
        return 0;
    } else {
        log_error(sp->logger, "Error al conectar a Kernel Memory en %s:%d", sp->ip_kernel_memory, sp->puerto_kernel_memory);
        return -1;
      
    }
}

void enviar_handshake(t_swap* sp){
    t_paquete* paquete = crear_paquete(SWAP_HANDSHAKE,crear_buffer());
    
    enviar_paquete(paquete, sp->socket_kernel_memory, sp->logger);
    eliminar_paquete(paquete);
    log_debug(sp->logger, "HANSHAKE KERNEL MEMORY ENVIADO");
}

void enviar_tamanio_bloque(t_swap* sp){
    t_buffer* buffer = crear_buffer();

    t_paquete* paquete =crear_paquete(SWAP_REQUEST, buffer);

    int tamanio_bloque=sp->block_size;
    int tamanio_total=1; //VER COMO CALCULO EL TAMAÑO TOTAL 
    
    agregar_a_paquete(paquete, &tamanio_bloque, sizeof(int));
    agregar_a_paquete(paquete, &tamanio_total, sizeof(int));

    int resultado = enviar_paquete(paquete, sp->socket_kernel_memory, sp->logger);

    if (resultado == 0) {
        log_info(sp->logger, "Tamaño de bloque y total enviados correctamente a Kernel Memory");
    } else {
        log_error(sp->logger, "Error al enviar tamaño de bloque y total a Kernel Memory");
    }

    eliminar_paquete(paquete);
}