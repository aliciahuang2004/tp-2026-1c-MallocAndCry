#include "kernel_scheduler.h"

void* atender_io(void* arg) {
    int socket_io = *(int*)arg;
    free(arg);

    log_debug(kernel->logger, "IO Listener: hilo iniciado en socket %d", socket_io);
    sem_post(&sem_hayIO[buscarTipoIOPorSocket(socket_io)]);

    while(1) {
        t_tipo_io tipoIO = buscarTipoIOPorSocket(socket_io);
        t_list* paquete = recibir_paquete(socket_io);
        if (!paquete) {
            switch (tipoIO){
                case IO_SLEEP:
                    log_error(kernel->logger, "Se desconecto IO SLEEP, en socket %d",socket_io);
                    break;
                case IO_STDIN:
                    log_error(kernel->logger, "Se desconecto IO STDIN, en socket %d",socket_io);
                    break;
                case IO_STDOUT:
                    log_error(kernel->logger, "Se desconecto IO STDOUT, en socket %d",socket_io);
                    break;
                default:
                    break;
            }
            pthread_mutex_lock(&mutex_interfaces[tipoIO]);
            if(interfaces[tipoIO].socket_interfaz == socket_io){
                pthread_mutex_unlock(&mutex_interfaces[tipoIO]);
                int pid = finalizoConexionIO(tipoIO);
                if(pid!=-1) finalizarProceso(pid,DESCONEXION_IO);
                close(socket_io);
                return NULL;
            }
            pthread_mutex_unlock(&mutex_interfaces[tipoIO]);
        }
        int cod_op = *(int*) list_get(paquete, 0);
        int pid = *(int*) list_get(paquete, 1);

        switch (cod_op) {
            case IO_OK://termino syscall
                
                switch(tipoIO){
                    case IO_STDIN:{
                        log_debug(kernel->logger, "IO_OK recibido para PID: %d TIPO: STDIN", pid);
                        char* lecturaIO = (char*) list_get(paquete, 3);
                        pthread_mutex_lock(&mutex_interfaces[tipoIO]);
                        t_solicitud_io* solicitudSTDIN = queue_peek(interfaces[tipoIO].solicitudes);
                        if(solicitudSTDIN->pidSolicitaSyscall == pid){
                            solicitudSTDIN->leido = strdup(lecturaIO);
                            log_debug(kernel->logger, "STDIN envia '%s' para PID: %d", lecturaIO, pid);
                            pthread_mutex_unlock(&mutex_interfaces[tipoIO]);
                        }
                        pthread_mutex_unlock(&mutex_interfaces[tipoIO]);
                        sem_post(&sem_recibiLecuraDeIO);
                        liberarIO(tipoIO);
                        break;
                    }
                    case IO_SLEEP:{
                        log_debug(kernel->logger, "IO_OK recibido para PID: %d TIPO: SLEEP", pid);
                        t_solicitud_io* solicitud = retirarSolicitud(tipoIO,pid);
                        if (solicitud == NULL){
                            log_error(kernel->logger, "ERROR al retirar solicitud IO de la cola de solicitudes");
                        }
                            // Extraer de forma segura el PID que envió el módulo de I/O
                        if (buscarPCBPorPID(pid, colaBLOCK, &mutex_BLOCK) == NULL) {
                            log_debug(kernel->logger, "No se encontró el PCB para PID %d en BLOCK. Verificando otras colas...", pid);
                             t_pcb* pcbSuspendiendo = buscarPCBPorPID(pid, colaBLOCK_SUSP, &mutex_BLOCK_SUSP);
                            if (pcbSuspendiendo == NULL) {
                                log_error(kernel->logger, "Error: No se encontró el PCB para PID %d en ninguna cola de bloqueados.", pid);
                                break; // Salimos del case para evitar errores posteriores
                            } else if (pcbSuspendiendo->suspensionEnCurso) {
                                pcbSuspendiendo->ioCompletadaEnTransito = true;
                                log_debug(kernel->logger, "## (<%d>) IO finalizo mientras se completaba el traspaso a SWAP, se desuspendera al terminar", pid);
                            } else {
                                pasarProcesoBlockSuspAReadySusp(pid);
                                solicitarDesuspenderProceso(pid);
                                log_debug(kernel->logger, "PCB para PID %d encontrado en BLOCK_SUSP.", pid);
                                log_info(kernel->logger, "## (<%d>) finalizó IO y pasa a SUSP. READY", pid);
                            }
                        } else {
                            pasarProcesoBlockaReady(pid);
                            log_debug(kernel->logger, "PCB para PID %d encontrado en BLOCK.", pid);
                            log_info(kernel->logger, "## (<%d>) finalizó IO y pasa a READY", pid);
                        }
                        free(solicitud);
                        liberarIO(tipoIO);
                        break;
                    }
                    case IO_STDOUT:{
                        log_debug(kernel->logger, "IO_OK recibido para PID: %d TIPO: STDOUT", pid);
                        t_solicitud_io* solicitud = retirarSolicitud(tipoIO,pid);
                        if (solicitud == NULL){
                            log_error(kernel->logger, "ERROR al retirar solicitud IO de la cola de solicitudes");
                        }
                            // Extraer de forma segura el PID que envió el módulo de I/O
                        if (buscarPCBPorPID(pid, colaBLOCK, &mutex_BLOCK) == NULL) {
                            log_debug(kernel->logger, "No se encontró el PCB para PID %d en BLOCK. Verificando otras colas...", pid);
                            t_pcb* pcbSuspendiendo = buscarPCBPorPID(pid, colaBLOCK_SUSP, &mutex_BLOCK_SUSP);
                            if (pcbSuspendiendo == NULL) {
                                log_error(kernel->logger, "Error: No se encontró el PCB para PID %d en ninguna cola de bloqueados.", pid);
                                break; // Salimos del case para evitar errores posteriores
                            } else if (pcbSuspendiendo->suspensionEnCurso) {
                                pcbSuspendiendo->ioCompletadaEnTransito = true;
                                log_debug(kernel->logger, "## (<%d>) IO finalizo mientras se completaba el traspaso a SWAP, se desuspendera al terminar", pid);
                            } else {
                                pasarProcesoBlockSuspAReadySusp(pid);
                                solicitarDesuspenderProceso(pid);
                                log_debug(kernel->logger, "PCB para PID %d encontrado en BLOCK_SUSP.", pid);
                                log_info(kernel->logger, "## (<%d>) finalizó IO y pasa a SUSP. READY", pid);
                            }
                        } else {
                            pasarProcesoBlockaReady(pid);
                            log_debug(kernel->logger, "PCB para PID %d encontrado en BLOCK.", pid);
                            log_info(kernel->logger, "## (<%d>) finalizó IO y pasa a READY", pid);
                        }
                        liberarIO(tipoIO);
                        break;
                    }
                default:
                log_warning(kernel->logger, "Operación desconocida de cliente en socket %d", socket_io);
                break;
            }
            list_destroy_and_destroy_elements(paquete,free);
    // Aca el hilo puede continuar en un bucle según la necesidad del protocolo
        }
    }
}

int finalizoConexionIO(t_tipo_io tipo){
    pthread_mutex_lock(&mutex_interfaces[tipo]);
    interfaces[tipo].ocupada = false;
    int pid = interfaces[tipo].pidAsignado;
    interfaces[tipo].pidAsignado = -1;
    interfaces[tipo].socket_interfaz = -1;
    pthread_mutex_unlock(&mutex_interfaces[tipo]);
    return pid;
}

void liberarIO(t_tipo_io tipo){
    pthread_mutex_lock(&mutex_interfaces[tipo]);
    interfaces[tipo].ocupada = false;
    interfaces[tipo].pidAsignado = -1;
    log_debug(kernel->logger, "## Interfaz [%s] liberada.", interfaces[tipo].nombre);
    pthread_mutex_unlock(&mutex_interfaces[tipo]);
    sem_post(&sem_hayIO[tipo]);
}

void revisarProcesosBloqueadosParaTipoIO(t_tipo_io tipo) {
    pthread_mutex_lock(&mutex_interfaces[tipo]);
    if (!queue_is_empty(interfaces[tipo].solicitudes) && interfaces[tipo].ocupada == false) {
        t_solicitud_io* solicitud = queue_peek(interfaces[tipo].solicitudes);
        interfaces[tipo].ocupada = true;
        interfaces[tipo].pidAsignado = solicitud->pidSolicitaSyscall;
        log_debug(kernel->logger, "## PID %d - Enviado a Interfaz [%s] desde cola de espera.", solicitud->pidSolicitaSyscall, interfaces[tipo].nombre);
        switch (tipo){
        case IO_STDIN:
            pthread_mutex_unlock(&mutex_interfaces[tipo]);
            hacerSTDIN(solicitud,interfaces[tipo].socket_interfaz);
            break;
        case IO_SLEEP:
            pthread_mutex_unlock(&mutex_interfaces[tipo]);
            enviarAIO(interfaces[tipo].socket_interfaz, solicitud);
            break;
        case IO_STDOUT:
            pthread_mutex_unlock(&mutex_interfaces[tipo]);
            hacerSTDOUT(solicitud,interfaces[tipo].socket_interfaz);          
            break;
        }
        pthread_mutex_unlock(&mutex_interfaces[tipo]);
    }
}

t_solicitud_io* retirarSolicitud(t_tipo_io tipo, int pid){
    pthread_mutex_lock(&mutex_interfaces[tipo]);
    t_solicitud_io* solicitud = queue_pop(interfaces[tipo].solicitudes);
    pthread_mutex_unlock(&mutex_interfaces[tipo]);
    return solicitud;
}