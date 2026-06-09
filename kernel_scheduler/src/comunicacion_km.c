#include "kernel_scheduler.h"
/* Si dos CPUs hacen STDOUT al mismo tiempo, ambas envían LECTURA_DE_DATOS al socket de KM y ambas hacen sem_wait. 
El atender_Kernel_memory recibe dos DATOS_LEIDOS y hace sem_post dos veces, pero ambas escriben sobre el mismo buffer compartido, la primera que se despierte puede leer los datos de la segunda.*/

sem_t           sem_datos_listos;
void*           km_datos_buffer = NULL;
uint32_t        km_datos_size   = 0;
pthread_mutex_t mutex_lectura_km = PTHREAD_MUTEX_INITIALIZER;

void iniciar_semaforos_datos_recibidos(void) {
    sem_init(&sem_datos_listos, 0, 0);
}
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

            case CREACION_DE_PROCESO_OK: {
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM confirmó creación de proceso - PID: %d", pid);
                break;
            }

            case CREACION_DE_PROCESO_ERROR: {
                int pid = *(int*) list_get(paquete, 1);
                log_error(kernel->logger, "## KM reportó error en creación de proceso - PID: %d", pid);
                break;
            }

            case CREACION_DE_SEGMENTO_OK: {
                int pid = *(int*) list_get(paquete, 1);
            log_debug(kernel->logger, "## KM confirmó creación de segmento - PID: %d", pid);

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
        }

            case AUMENTO_DE_MEMORIA: {
                uint32_t memoria_total = *(uint32_t*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM reporta aumento de memoria disponible: %u bytes", memoria_total);
                break;
            }

            case DATOS_LEIDOS: {
                // KM envia: [DATOS_LEIDOS, tamaño (uint32_t), bytes...]
                uint32_t tamanio = *(uint32_t*) list_get(paquete, 1);
                void* datos_paquete = list_get(paquete, 2);

                km_datos_buffer = malloc(tamanio);
                memcpy(km_datos_buffer, datos_paquete, tamanio);
                km_datos_size = tamanio;
                sem_post(&sem_datos_listos);
                break;
            }
            
            default:
                log_warning(kernel->logger, "KM Listener: código de operación inesperado: %d", cod_op);
                break;
        }

        list_destroy_and_destroy_elements(paquete, free);
    }

    return NULL;
}