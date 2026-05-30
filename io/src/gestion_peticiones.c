#include "io.h"

// Convierte un string de tipo IO a enum t_io_operation
t_io_operation obtener_tipo_operacion(const char* tipo_io_str) {
    if (tipo_io_str == NULL) {
        return OP_SLEEP;
    }
    
    if (strcasecmp(tipo_io_str, "STDIN") == 0) {
        return OP_STDIN;
    } else if (strcasecmp(tipo_io_str, "STDOUT") == 0) {
        return OP_STDOUT;
    } else if (strcasecmp(tipo_io_str, "SLEEP") == 0) {
        return OP_SLEEP;
    }
    
    return OP_SLEEP;  // Por defecto
}

void recibir_y_ejecutar_tarea(t_io* io, t_io_operation tipo_modulo) {
    t_list* paquete = recibir_paquete(io->socket_kernel_scheduler);
    
    if (paquete == NULL) {
        log_error(io->logger, "Conexión perdida con Kernel Scheduler.");
        exit(EXIT_FAILURE);
    }

    // Verificar código de operación
    int* cod_op_ptr = (int*) list_get(paquete, 0);
    int cod_op = *cod_op_ptr;

    if (cod_op != SLEEP && cod_op != STDIN && cod_op != STDOUT) {
        log_warning(io->logger, "Código de operación inesperado para IO: esperado SLEEP/STDIN/STDOUT, recibido %d", cod_op);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    // Deserializar datos de la solicitud
    int pid = -1;
    if (list_size(paquete) > 1) {
        int* pid_ptr = (int*) list_get(paquete, 1);
        pid = *pid_ptr;
    }

    log_info(io->logger, "## PID: %d - Inicio de IO", pid);
    switch (cod_op) {
        case STDIN: {
            uint32_t dir_logica = 0;
            uint32_t tamano = 0;
            if (list_size(paquete) > 2) dir_logica = *(uint32_t*) list_get(paquete, 2);
            if (list_size(paquete) > 3) tamano = *(uint32_t*) list_get(paquete, 3);
            log_debug(io->logger, "IO STDIN recibido: PID %d, dir_logica=%u, tamano=%u", pid, dir_logica, tamano);
            ejecutar_stdin(pid, tamano, io->socket_kernel_scheduler, io->logger);
            break;
        }
        case STDOUT: {
            uint32_t dir_logica = 0;
            uint32_t tamano = 0;
            if (list_size(paquete) > 2) dir_logica = *(uint32_t*) list_get(paquete, 2);
            if (list_size(paquete) > 3) tamano = *(uint32_t*) list_get(paquete, 3);
            log_debug(io->logger, "IO STDOUT recibido: PID %d, dir_logica=%u, tamano=%u", pid, dir_logica, tamano);
            ejecutar_stdout(pid, tamano, NULL, io->socket_kernel_scheduler, io->logger);
            break;
        }
        case SLEEP: {
            uint32_t tiempo_ms = 0;
            if (list_size(paquete) > 2) tiempo_ms = *(uint32_t*) list_get(paquete, 2);
            log_debug(io->logger, "IO SLEEP recibido: PID %d, tiempo_ms=%u", pid, tiempo_ms);
            ejecutar_sleep(pid, tiempo_ms, io->socket_kernel_scheduler, io->logger);
            break;
        }
        default:
            log_error(io->logger, "Tipo de operación desconocido: %d", cod_op);
            break;
    }
               
    log_info(io->logger, "## PID: %d - Fin de IO", pid);

    // Liberar lista recibida
    list_destroy_and_destroy_elements(paquete, free);
}

void enviar_confirmacion_ks(int socket_ks, int pid, void* datos_respuesta, uint32_t datos_size, t_log* logger) {
    if (socket_ks < 0 || !logger) {
        return;
    }

    // Crear paquete con código IO_OK
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(IO_OK, buffer);

    // Serializar datos de confirmación
    // Formato: PID + datos_size + datos (si aplica)
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &datos_size, sizeof(uint32_t));
    
    if (datos_size > 0 && datos_respuesta != NULL) {
        agregar_a_paquete(paquete, datos_respuesta, datos_size);
    }

    // Enviar paquete
    int resultado = enviar_paquete(paquete, socket_ks, logger);
    if (resultado == 0) {
        log_info(logger, "Enviando confirmación IO_OK para PID %d al socket %d", pid, socket_ks);
    } else {
        log_error(logger, "Error al enviar IO_OK para PID %d al socket %d", pid, socket_ks);
    }
    // Liberar estructura
    eliminar_paquete(paquete);
}


