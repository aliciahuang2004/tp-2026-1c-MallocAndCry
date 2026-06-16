#include "io.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

t_io* inicializar_io(char* archivo_config, char* tipo_io) {

    t_io* io= malloc(sizeof(t_io));

    t_log* logger_temp= iniciar_logger("io.log", "[IO_INIT]", true, LOG_LEVEL_INFO);

    io-> config= iniciar_config(logger_temp, archivo_config);

    io-> log_level= strdup(config_get_string_value(io->config, "LOG_LEVEL"));
    t_log_level nivel = obtener_log_level(io->log_level);

    io-> logger= iniciar_logger("io.log", "[IO]", true, nivel);

    log_destroy(logger_temp);

    io-> ip_kernel_scheduler= config_get_string_value(io->config, "IP_KERNEL_SCHEDULER");
    io-> puerto_kernel_scheduler= config_get_int_value(io->config, "PUERTO_KERNEL_SCHEDULER");
    io->tipo_IO= strdup(tipo_io);
    io->nombre = NULL;
    io-> socket_kernel_scheduler= -1;
    
    return io;
}

void verificar_io(t_io* io){
    log_debug(io->logger, "IO inicializado correctamente");
    log_debug(io->logger, "Tipo de IO: %s", io->tipo_IO);
    log_debug(io->logger, "IP del Kernel Scheduler: %s", io->ip_kernel_scheduler);
    log_debug(io->logger, "Puerto del Kernel Scheduler: %d", io->puerto_kernel_scheduler);
}

void liberar_io(t_io* io){
    if(!io) return;

    if(io->logger) log_destroy(io->logger);
    if(io->config) config_destroy(io->config);
    if(io->tipo_IO) free(io->tipo_IO);
    if(io->nombre) free(io->nombre);
    if(io->log_level) free(io->log_level);
    free(io);
}

int conectar_a_kernel_scheduler(t_io* io){
    char* puerto_str = string_itoa(io->puerto_kernel_scheduler);

    io->socket_kernel_scheduler = crear_conexion(io->logger, io->ip_kernel_scheduler, puerto_str);

    free(puerto_str);

    if(io->socket_kernel_scheduler != -1){
        log_info(io->logger, "## Conectado a Kernel Scheduler");
        return 0;
    } else {
        log_error(io->logger, "Error al conectar a Kernel Scheduler en %s:%d", io->ip_kernel_scheduler, io->puerto_kernel_scheduler);
        return -1;
      
    }
}

void enviar_handshake(t_io* io){
    t_paquete* paquete = crear_paquete(IO_HANDSHAKE, crear_buffer());

    // Guardamos un nombre lógico para esta interfaz de IO.
    io->nombre = strdup(io->tipo_IO);
    agregar_a_paquete(paquete, (void*)io->nombre, strlen(io->nombre) + 1);

    int tipo_interfaz = obtener_tipo_operacion(io->tipo_IO);
    agregar_a_paquete(paquete, &tipo_interfaz, sizeof(int));

    enviar_paquete(paquete, io->socket_kernel_scheduler, io->logger);
    eliminar_paquete(paquete);
    log_info(io->logger, "HANDSHAKE A KERNEL Scheduler ENVIADO: %s tipo=%d", io->nombre, tipo_interfaz);
}


