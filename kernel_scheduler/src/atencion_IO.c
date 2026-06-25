#include "kernel_scheduler.h"

void* atender_io(void* arg) {
    int socket_io = *(int*)arg;
    free(arg);

    log_info(kernel->logger, "IO Listener: hilo iniciado en socket %d", socket_io);
    sem_post(&sem_hayIO[buscarTipoIOPorSocket(socket_io)]);

    while(1) {
        t_tipo_io tipoIO = buscarTipoIOPorSocket(socket_io);
        t_list* paquete = recibir_paquete(socket_io);
        if (!paquete) {
            log_error(kernel->logger, "Error al recibir paquete de datos de IO");
            pthread_mutex_lock(&mutex_interfaces[tipoIO]);
            if(interfaces[tipoIO].socket_interfaz == socket_io){
                pthread_mutex_unlock(&mutex_interfaces[tipoIO]);
                int pid = finalizoConexionIO(tipoIO);
                finalizarProceso(pid,DESCONEXION_IO);
            return NULL;
            }
            pthread_mutex_unlock(&mutex_interfaces[tipoIO]);
        }
        int cod_op = *(int*) list_get(paquete, 0);
        int pid = *(int*) list_get(paquete, 1);

        switch (cod_op) {
            case IO_OK://termino syscall
                t_solicitud_io* solicitud = NULL;

                if(tipoIO != IO_SLEEP){
                    solicitud = retirarSolicitud(tipoIO,pid);
                    if (solicitud == NULL){
                        log_error(kernel->logger, "ERROR al retirar solicitud IO de la cola de solicitudes");
                    }
                }

                if(tipoIO == IO_STDIN){
                    int tamanio = *(int*) list_get(paquete, 2);
                    char* lecturaIO = (char*) list_get(paquete, 3);
                    int tamanioLectura = strlen(lecturaIO) + 1;
                    if(solicitud->pidSolicitaSyscall == pid && solicitud->tamanio == tamanio){
                        solicitud->leido = strdup(lecturaIO);
                        log_debug(kernel->logger, "STDIN envia '%s' para PID: %d", lecturaIO, pid);
                    }
                    sem_post(&sem_recibiLecuraDeIO);
                }

                    // Extraer de forma segura el PID que envió el módulo de I/O
                log_debug(kernel->logger, "IO_OK recibido para PID %d en socket %d", pid, socket_io);
                if(tipoIO != IO_STDIN) {
                    if (buscarPCBPorPID(pid, colaBLOCK, mutex_BLOCK) == NULL) {
                        log_debug(kernel->logger, "No se encontró el PCB para PID %d en BLOCK. Verificando otras colas...", pid);
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
                }
                liberarIO(tipoIO);
                revisarProcesosBloqueadosParaTipoIO(tipoIO);
                break;
            default:
                log_warning(kernel->logger, "Operación desconocida de cliente en socket %d", socket_io);
                break;
        }
        list_destroy_and_destroy_elements(paquete,free);
    // Aca el hilo puede continuar en un bucle según la necesidad del protocolo

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
    log_info(kernel->logger, "## Interfaz [%s] liberada.", interfaces[tipo].nombre);
    pthread_mutex_unlock(&mutex_interfaces[tipo]);
    sem_post(&sem_hayIO[tipo]);
}

void revisarProcesosBloqueadosParaTipoIO(t_tipo_io tipo) {
    pthread_mutex_lock(&mutex_interfaces[tipo]);
    if (!queue_is_empty(interfaces[tipo].solicitudes)) {
        t_solicitud_io* solicitud = queue_pop(interfaces[tipo].solicitudes);
        interfaces[tipo].ocupada = true;
        interfaces[tipo].pidAsignado = solicitud->pidSolicitaSyscall;
        log_info(kernel->logger, "## PID %d - Enviado a Interfaz [%s] desde cola de espera.", solicitud->pidSolicitaSyscall, interfaces[tipo].nombre);
        if(tipo != IO_STDOUT){
            enviarAIO(interfaces[tipo].socket_interfaz, solicitud);
            pthread_mutex_unlock(&mutex_interfaces[tipo]);
            free(solicitud);
        }else{
            queue_push(interfaces[tipo].solicitudes, solicitud);
            enviarAKMSolicitudIO(solicitud);            
            pthread_mutex_unlock(&mutex_interfaces[tipo]);
        }
    } else {
        pthread_mutex_unlock(&mutex_interfaces[tipo]);
    }
    
}

t_solicitud_io* retirarSolicitud(t_tipo_io tipo, int pid){
    pthread_mutex_lock(&mutex_interfaces[tipo]);
    t_solicitud_io* solicitud = queue_pop(interfaces[tipo].solicitudes);
    pthread_mutex_unlock(&mutex_interfaces[tipo]);
    return solicitud;
}