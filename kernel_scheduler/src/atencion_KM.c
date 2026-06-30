#include "kernel_scheduler.h"

void* atender_kernel_memory(void* arg) {
    log_debug(kernel->logger, "KM Listener: hilo iniciado en socket %d", kernel->socket_kernel_memory);

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
            case ELIMINACION_DE_SEG_OK: {
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM confirmó eliminación de segmento - PID: %d", pid);
                break;
            }
            case ELIMINACION_DE_SEG_ERROR: {
                int pid = *(int*) list_get(paquete, 1);
                log_error(kernel->logger, "## KM reportó error en eliminación de segmento - PID: %d", pid);
                break;
            }

            case CREACION_DE_SEGMENTO_OK:{
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM confirmó creación de segmento - PID: %d", pid);
                enviarPIDAcpu(pid,buscarCPUSegunPID(pid));
                break;
            }
            case CREACION_DE_SEGMENTO_ERROR:{ // en nuestro flujo e compactacion cuando llega CREACION_DE_SEGMENTO_ERROR, el proceso ya fue movido a READY y la CPU ya fue liberada
                int pid = *(int*) list_get(paquete, 1);
                log_error(kernel->logger, "## KM reportó error en creación de segmento - PID: %d", pid);
                break;
            }
            case RTA_LECTURA:{
                int pid = *(int*) list_get(paquete, 1);
                char* leido = (char*) list_get(paquete, 2);
                log_debug(kernel->logger, "## KM confirmó lectura para PID: %d", pid);
                pthread_mutex_lock(&mutex_interfaces[IO_STDOUT]);
                t_solicitud_io* solicitud = queue_peek(interfaces[IO_STDOUT].solicitudes);
                if(solicitud != NULL && solicitud->pidSolicitaSyscall == pid){
                    solicitud->leido = malloc(solicitud->tamanio);
                    memcpy(solicitud->leido, leido, solicitud->tamanio);
                }
                pthread_mutex_unlock(&mutex_interfaces[IO_STDOUT]);
                sem_post(&sem_recibiLecuraDeKM);
                break;
            }
            case ESCRITURA_DE_DATOS_OK: {
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM confirmó escritura STDIN para PID: %d", pid);
                sem_post(&sem_recibiEscrituraDeKM);
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
                log_debug(kernel->logger, "## KM solicita inicio de compactación para creación de segmento");
                
                noHayCompactacion = false; // para el planificador
                pedirDesalojoPorCompactacion();
                chequearCPUsDesalojadas();
                
                t_paquete* paquete = crear_paquete(CPUS_DESALOJADAS, crear_buffer());

                enviar_paquete(paquete,kernel->socket_kernel_memory,kernel->logger);
            
                eliminar_paquete(paquete);
                log_info(kernel->logger, "## Inicio de compactación");

                break;
            }
            case COMPACTACION_OK:{
                log_debug(kernel->logger, "## KM informó fin de compactación");
                noHayCompactacion = true; // reinicia planificador
                log_info(kernel->logger, "## Fin de compactación");
                break;
            }
            case AUMENTO_DE_MEMORIA:{
                log_debug(kernel->logger, "## KM informó tener un aumento de memoria, eligiendo proceso suspendido");
                solicitarDesuspenderProceso();
                // pedir hacer swap
                break;
            }
            case SUSPENSION_OK: {
            log_debug(kernel->logger, "## KM confirmo la suspension");
            sem_post(&sem_suspension_ok);
            break;
            }
            case DESUSPENSION_OK: {
                int pid = *(int*) list_get(paquete, 1);
                log_debug(kernel->logger, "## KM confirmo la desuspension");
                pasarProcesoReadySuspAReady(pid);
                solicitarDesuspenderProceso();
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

void pedirDesalojoPorCompactacion(){
    t_cpu_conectada* cpuOcupada = NULL;

    t_queue* cpuOcupadas = queue_create();

    pthread_mutex_lock(&mutex_CPU);
    int cantidadCpu = queue_size(colaCPUs); 
    for(int i = 0; i < cantidadCpu ; i++){
        t_cpu_conectada* cpu = queue_pop(colaCPUs);
        if(!cpu->libre){
            cpuOcupada = cpu;
            queue_push(cpuOcupadas,cpuOcupada);
        }
        queue_push(colaCPUs,cpu);
    }
    pthread_mutex_unlock(&mutex_CPU);

    log_debug(kernel->logger,"TOTAL CPU'S %d", cantidadCpu);
    log_debug(kernel->logger,"TOTAL CPU'S OCUPADAS %d",  queue_size(cpuOcupadas));
    log_debug(kernel->logger, "[DEBUG-1] queue_is_empty(cpuOcupadas)=%d (0=tiene CPUs, 1=vacia)", queue_is_empty(cpuOcupadas));
    while (!queue_is_empty(cpuOcupadas)){
        log_debug(kernel->logger, "[DEBUG-2] Enviando desalojo a una CPU");
        t_cpu_conectada* cpu = queue_pop(cpuOcupadas);
        notificarDesalojo(cpu,NULL, PROCESO_DESALOJADO_COMPACTACION);
    }
    queue_destroy(cpuOcupadas);
}

void chequearCPUsDesalojadas(){
    bool faltaLiberar =true ;
    while(faltaLiberar){
        pthread_mutex_lock(&mutex_EXEC);
        int cantidad = queue_size(colaEXEC);
        pthread_mutex_unlock(&mutex_EXEC);
        if(cantidad == 0){
            log_debug(kernel->logger, "Procesos ejecutando %d", cantidad);
            faltaLiberar = false;
        }
    }
}

void solicitarDesuspenderProceso(){
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(DESUSPENSION_DE_PROCESO, buffer); 

    pthread_mutex_lock(&mutex_READY_SUSP);
    t_pcb* pcb = queue_peek(colaREADY_SUSP);
    pthread_mutex_unlock(&mutex_READY_SUSP);
    agregar_a_paquete(paquete, &pcb->pid, sizeof(int));

    int resultado = enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);

    if (resultado != 0) {
        log_error(kernel->logger, "Error al enviar pedido de desuspencion a Kernel Memory");
        exit(EXIT_FAILURE);
    }
    eliminar_paquete(paquete);
}