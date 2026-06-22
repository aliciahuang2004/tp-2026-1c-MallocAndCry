#include "kernel_scheduler.h"

void* atender_kernel_memory(void* arg) {
    log_info(kernel->logger, "KM Listener: hilo iniciado en socket %d", kernel->socket_kernel_memory);

    while (1) {
        t_list* paquete = recibir_paquete(kernel->socket_kernel_memory);

        if (paquete == NULL) {
            log_error(kernel->logger, "KM Listener: KM se desconectó o error en socket");
            break;
        }

        int cod_op = *(int*) list_get(paquete, 0);

        switch (cod_op) {
            case CREACION_DE_PROCESO_OK:{
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM confirmó creación de proceso - PID: %d", pid);
                pasarProcesoNewAReady();
                break;
            }
            case CREACION_DE_PROCESO_ERROR:{
                int pid = *(int*) list_get(paquete, 1);
                log_error(kernel->logger, "## KM reportó error en creación de proceso - PID: %d", pid);
                finalizarProceso(pid,CREACION_DE_PROCESO_ERROR);
                break;
            }
            /*case ELIMINACION_DE_SEGMENTO_OK:{
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM confirmó eliminación de proceso - PID: %d", pid);
                enviarPIDAcpu(pid,buscarCPUSegunPID(pid));
                break;
            }
            case ELIMINACION_DE_SEGMENTO_ERROR:{
                int pid = *(int*) list_get(paquete, 1);
                log_error(kernel->logger, "## KM reportó error en eliminación de proceso - PID: %d", pid);
                //SE VUELVE A MANDAR O FINALIZA?
                break;
            }*/
            case CREACION_DE_SEGMENTO_OK:{
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM confirmó creación de segmento - PID: %d", pid);
                enviarPIDAcpu(pid,buscarCPUSegunPID(pid));
                break;
            }
            case CREACION_DE_SEGMENTO_ERROR:{
                int pid = *(int*) list_get(paquete, 1);
                log_error(kernel->logger, "## KM reportó error en creación de segmento - PID: %d", pid);
                //SE VUELVE A MANDAR O FINALIZA?
                break;
            }
            case RTA_LECTURA:{
                int pid = *(int*) list_get(paquete, 1);
                char* leido = (char*) list_get(paquete, 2);
                int tamanioLectura = strlen(leido) + 1;
                log_debug(kernel->logger, "## KM confirmó lectura : %s", leido);
                pthread_mutex_lock(&mutex_interfaces[STDIN]);
                t_solicitud_io* solicitud = queue_peek(interfaces[STDIN].solicitudes);
                // *** chequeo por las dudas pero no deberia de haber error
                if(solicitud->pidSolicitaSyscall == pid && solicitud->tamanio == tamanioLectura){
                    solicitud->leido = leido;
                }
                pthread_mutex_unlock(&mutex_interfaces[STDIN]);
                sem_post(&sem_recibiLecuraDeKM);
                break;
            }
            case CORRUPCION_MEMORIA:{
                log_debug(kernel->logger, "## KM informa corrupcion");
                // TODO
                /* se deberán finalizar todos los Procesos y finalizar 
                el Kernel Scheduler con motivo de Blue Screen of Death*/
                break;
            }
            case INICIAR_COMPACTACION:{
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM solicita inicio de compactación para creación de segmento - PID: %d", pid);
                //prepararParaCompactar();
                /*
                // pedirDesalojoPorCompactacion();
                // reencolar procesos
                t_paquete* paquete = crear_paquete(CPUS_DESALOJADAS, crear_buffer());
            
                agregar_a_paquete(paquete,&pidSolicitaSyscall,sizeof(int));

                enviar_paquete(paquete,kernel->socket_kernel_memory,kernel->logger);
            
                eliminar_paquete(paquete);

                int cod_op2 = recibir_operacion(kernel->socket_kernel_memory);
                if(cod_op2 == CREACION_DE_SEGMENTO_OK) {
                    log_info(kernel->logger, "Compactación finalizada, reactivando planificador");
                    // reactivar planificador
                }
                */

                break;
            }
            case COMPACTACION_TERMINADA:{
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM informó fin de compactación para creación de segmento - PID: %d", pid);
                // replanificar();
                break;
            }
            case AUMENTO_DE_MEMORIA:{
                log_debug(kernel->logger, "## KM informó tener un aumento de memoria");
                sem_post(&sem_hayMemoria); 
                // OJO PORQUE AL PEDIR CREAR SEGMENTO VUELVO A PERDER MEMORIA 
                // ENTONCES DEBERIA HACER sem_wait(&sem_hayMemoria); AL RECIBIR CREACION DE MEMORIA

            /*
            t_pcb* pcb = sacardeColaBlockPorPID(pid);
            if (pcb != NULL) {
                pcb->estado = READY;
                pthread_mutex_lock(&mutex_READY);
                queue_push(colaREADY, pcb);
                pthread_mutex_unlock(&mutex_READY);
                log_info(kernel->logger, "## (<%d>) Pasa del estado <BLOCK> al estado <READY>", pid);
                sem_post(&sem_procesosReady);
            } else {
                log_error(kernel->logger,
                        "KM Listener: CREACION_DE_SEGMENTO_OK - no se encontró PID %d en BLOCK", pid);
            }
            break;
            */
/*
            case AUMENTO_DE_MEMORIA: {
                uint32_t memoria_total = *(uint32_t*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM reporta aumento de memoria disponible: %u bytes", memoria_total);
                break;
            }
            }*/
                break;
            }
            default:
                log_warning(kernel->logger, "KM Listener: código de operación inesperado: %d", cod_op);
                break;
        }

        list_destroy_and_destroy_elements(paquete,free);
    }

    return NULL;
}