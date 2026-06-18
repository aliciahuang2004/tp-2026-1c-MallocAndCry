#include "kernel_scheduler.h"

void* atender_io(void* arg) {
    int socket_io = *(int*)arg;
    free(arg);

    while(1) {

        t_list* paquete = recibir_paquete(socket_io);
        if (!paquete) {
            log_error(kernel->logger, "Error al recibir paquete de datos de IO");
            return NULL;
        }

        int cod_op = *(int*) list_get(paquete, 0);
        int pid = *(int*) list_get(paquete, 1);
        //t_cpu_conectada* cpu_emisora = buscar_cpu_por_socket(socket_io);

        switch (cod_op) {
            case IO_OK://termino syscall

                // Extraer de forma segura el PID que envió el módulo de I/O
                log_debug(kernel->logger, "IO_OK recibido para PID %d en socket %d", pid, socket_io);
/*
                // Localizar la interfaz asociada al socket que envió el mensaje
                t_interfaz_conectada* interfaz = buscar_interfaz_por_socket(socket_io);
                if (interfaz == NULL) {
                    log_warning(kernel->logger, "No se encontró la interfaz IO asociada al socket %d", socket_io);
                    break;
                }

                interfaz->ocupada = false;
                log_info(kernel->logger, "## Interfaz [%s] liberada por IO_OK.", interfaz->nombre);

                // Buscar y extraer el PCB para realizar la transición de estados

                t_queue* cola_tipo = obtener_cola_bloqueados_por_tipo(interfaz->tipo);
                pthread_mutex_t* mutex_tipo = obtener_mutex_cola_por_tipo(interfaz->tipo);

                t_pcb* pcb_a_desbloquear = NULL;

                // RECOLECTAMOS LA SOLICITUD QUE YA TERMINÓ
                pthread_mutex_lock(mutex_tipo);
                if (!queue_is_empty(cola_tipo)) {
                    t_solicitud_io* solicitud_terminada = queue_pop(cola_tipo); // <-- Ahora sí sacamos la que terminó
                    if (solicitud_terminada != NULL) {
                        
                        pcb_a_desbloquear = solicitud_terminada->pcb;
                        if (interfaz->tipo == IO_STDIN && list_size(paquete) > 2) {
                            uint32_t datos_size    = *(uint32_t*) list_get(paquete, 2);
                            void* datos_usuario = list_get(paquete, 3);
                            if (datos_size > 0 && datos_usuario != NULL) {
                                enviarEscrituraAKM(pid_io, solicitud_terminada->dir_logica, datos_size, datos_usuario);
                                log_info(logger, "## PID: %d - Datos STDIN enviados a KM (dir=%u, tam=%u)", pid_io, solicitud_terminada->dir_logica, datos_size);
                            }
                        }
                        liberar_solicitud_io(solicitud_terminada); // <-- Recién acá la limpiamos de la memoria
                    }
                }
                pthread_mutex_unlock(mutex_tipo);

                // Devolvemos el proceso recuperado a READY
                if (pcb_a_desbloquear != NULL) {
                    log_info(logger, "## PID: %d - Estado Anterior: BLOCK - Estado Actual: READY", pcb_a_desbloquear->pid);
                    encolarProcesoEnReady(pcb_a_desbloquear); // Esto se encargará de ponerlo en la cola correcta según el algoritmo

                } else {
                    log_error(logger, "Error: El PID %d terminó pero no había nada en la cola.", pid_io);
                }

                
                // Si la cola no quedó vacía, significa que hay otro proceso esperando el dispositivo
                pthread_mutex_lock(mutex_tipo);
                if (!queue_is_empty(cola_tipo)) {
                    // Usamos queue_peek para mirar cuál es la siguiente solicitud sin sacarla de la cola
                    t_solicitud_io* solicitud_siguiente = queue_peek(cola_tipo);
                    if (solicitud_siguiente != NULL) {
                        interfaz->ocupada = true;
                        pthread_mutex_unlock(mutex_tipo);

                        log_info(logger, "## Interfaz [%s] ocupada de inmediato. Despachando siguiente PID en cola: %d.", 
                                 interfaz->nombre, solicitud_siguiente->pid);
                        
                        enviar_operacion_a_io(interfaz, solicitud_siguiente);
                    } else {
                        pthread_mutex_unlock(mutex_tipo);
                        log_error(logger, "Error interno: cola IO no vacía pero queue_peek devolvió NULL.");
                    }
                } else {
                    pthread_mutex_unlock(mutex_tipo);
                }
                break;
            }
    */
            default:
                log_warning(kernel->logger, "Operación desconocida de cliente en socket %d", socket_io);
                break;
        }
        list_destroy_and_destroy_elements(paquete,free);
    // Aca el hilo puede continuar en un bucle según la necesidad del protocolo
    }
}