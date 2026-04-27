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

    if (cod_op != IO_REQUEST) {
        log_warning(io->logger, "Código de operación inesperado: esperado IO_REQUEST (%d), recibido %d", 
                    IO_REQUEST, cod_op);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    // Deserializar datos de la solicitud
    // Formato esperado: PID + tipo_operacion + datos_size + datos
    int pid = -1;
    t_io_operation tipo_operacion = OP_SLEEP;
    uint32_t datos_size = 0;
    void* datos = NULL;

    if (list_size(paquete) > 1) {
        int* pid_ptr = (int*) list_get(paquete, 1);
        pid = *pid_ptr;
    }

    if (list_size(paquete) > 2) {
        t_io_operation* op_ptr = (t_io_operation*) list_get(paquete, 2);
        tipo_operacion = *op_ptr;
    }

    if (list_size(paquete) > 3) {
        uint32_t* size_ptr = (uint32_t*) list_get(paquete, 3);
        datos_size = *size_ptr;
    }

    if (list_size(paquete) > 4 && datos_size > 0) {
        datos = list_get(paquete, 4);
    }

    // Validar que el tipo de operación coincida con el tipo de este módulo
    if (tipo_operacion != tipo_modulo) {
        log_error(io->logger, "Error: Operación de tipo %d recibida, pero este módulo solo procesa tipo %d", 
                  tipo_operacion, tipo_modulo);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    log_info(io->logger, "## PID: %d - Inicio de IO", pid);
    switch (tipo_operacion) {
        case OP_STDIN:  ejecutar_stdin(pid, datos_size, io->socket_kernel_scheduler, io->logger); break;
        case OP_STDOUT: ejecutar_stdout(pid, datos_size, datos, io->socket_kernel_scheduler, io->logger); break;
        case OP_SLEEP:  ejecutar_sleep(pid, *(uint32_t*)datos, io->socket_kernel_scheduler, io->logger); break;
        default: log_error(io->logger, "Tipo de operación desconocido: %d", tipo_operacion); break;
            
    }
               
    log_info(io->logger, "## PID: %d - Fin de IO", pid);

    // Liberar datos si se asignaron en STDIN
    if (tipo_operacion == OP_STDIN && datos != NULL) {
        free(datos);
    }
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

    // Liberar estructura
    eliminar_paquete(paquete);
}


