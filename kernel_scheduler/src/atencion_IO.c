#include "kernel_scheduler.h"

void* atender_io(void* arg) {
    int socket_io = *(int*)arg;
    free(arg);

    log_info(kernel->logger, "IO Listener: hilo iniciado en socket %d", socket_io);

    while(1) {

        t_list* paquete = recibir_paquete(socket_io);
        if (!paquete) {
            log_error(kernel->logger, "Error al recibir paquete de datos de IO");
            t_tipo_io tipoIO = buscarTipoIOPorSocket(socket_io);
            finalizoConexionIO(tipoIO);
            return NULL;
        }

        int cod_op = *(int*) list_get(paquete, 0);
        int pid = *(int*) list_get(paquete, 1);
        t_tipo_io tipo = buscarTipoIOPorSocket(socket_io);
        // t_pcb* pcb = NULL;

        switch (cod_op) {
            case IO_OK://termino syscall

                // Extraer de forma segura el PID que envió el módulo de I/O
                log_debug(kernel->logger, "IO_OK recibido para PID %d en socket %d", pid, socket_io);
                if (buscarPCBPorPID(pid, colaBLOCK, mutex_BLOCK) == NULL) {
                    log_warning(kernel->logger, "No se encontró el PCB para PID %d en BLOCK. Verificando otras colas...", pid);
                    if (buscarPCBPorPID(pid, colaBLOCK_SUSP, mutex_BLOCK_SUSP) == NULL) {
                        log_error(kernel->logger, "Error: No se encontró el PCB para PID %d en ninguna cola de bloqueados.", pid);
                        break; // Salimos del case para evitar errores posteriores
                    } else {
                        pasarProcesoBlockSuspAReadySusp(pid);
                        log_info(kernel->logger, "PCB para PID %d encontrado en BLOCK_SUSP.", pid);
                    }
                } else {
                    pasarProcesoBlockaReady(pid);
                    log_info(kernel->logger, "PCB para PID %d encontrado en BLOCK.", pid);
                }
                liberarIO(tipo);
                revisarProcesosBloqueadosParaTipoIO(tipo);
                break;
            /*case IO_REQUEST:
                break;*/
            default:
                log_warning(kernel->logger, "Operación desconocida de cliente en socket %d", socket_io);
                break;
        }
        list_destroy_and_destroy_elements(paquete,free);
    // Aca el hilo puede continuar en un bucle según la necesidad del protocolo

    }
}
void finalizoConexionIO(t_tipo_io tipo){
    pthread_mutex_lock(&mutex_interfaces[tipo]);
    interfaces[tipo].ocupada = false;
    interfaces[tipo].pidAsignado = -1;
    interfaces[tipo].socket_interfaz = -1;
    pthread_mutex_unlock(&mutex_interfaces[tipo]);
}

void liberarIO(t_tipo_io tipo){
    pthread_mutex_lock(&mutex_interfaces[tipo]);
    interfaces[tipo].ocupada = false;
    interfaces[tipo].pidAsignado = -1;
    log_info(kernel->logger, "## Interfaz [%s] liberada.", interfaces[tipo].nombre);
    pthread_mutex_unlock(&mutex_interfaces[tipo]);
}

void revisarProcesosBloqueadosParaTipoIO(t_tipo_io tipo) {
    pthread_mutex_lock(&mutex_interfaces[tipo]);
    if (!queue_is_empty(interfaces[tipo].solicitudes)) {
        t_solicitud_io* solicitud = queue_pop(interfaces[tipo].solicitudes);
        interfaces[tipo].ocupada = true;
        interfaces[tipo].pidAsignado = solicitud->pidSolicitaSyscall;
        log_info(kernel->logger, "## PID %d - Enviado a Interfaz [%s] desde cola de espera.", solicitud->pidSolicitaSyscall, interfaces[tipo].nombre);
        pthread_mutex_unlock(&mutex_interfaces[tipo]);
        enviarAIO(interfaces[tipo].socket_interfaz, solicitud);
        free(solicitud);
    } else {
        pthread_mutex_unlock(&mutex_interfaces[tipo]);
    }
    
}