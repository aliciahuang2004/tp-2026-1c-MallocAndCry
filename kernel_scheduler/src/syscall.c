#include "kernel_scheduler.h"

t_pcb* crear_PCB(char* path, int prioridad){
    t_pcb* pcbCreado = malloc(sizeof(t_pcb));
    pcbCreado->pid = pidParaAsignar;
    pcbCreado->prioridad = prioridad;
    pcbCreado->estado = NEW; //NO REQUIERO DE MEMORIA, LO CREO DIRECTAMENTE
    pcbCreado->path = path;
    pidParaAsignar ++;
    return pcbCreado;
}

void crearProceso(char* path, int prioridad){
    t_pcb* pcbNuevo = crear_PCB(path, prioridad);
    
    enviarPathYPidKM(pcbNuevo->pid,path);
    //AGREGAR A COLA NEW, es realmente necesario?
    /*
    int cop_op = recibir_operacion(kernel->socket_kernel_memory);
    switch (cop_op){
        case CREACION_DE_PROCESO_OK:
            log_debug(kernel->logger,"Kernel Memory recibio correctamente el path");

            pthread_mutex_lock(&mutex_NEW);
            queue_push(colaNEW, pcbNuevo);
            pthread_mutex_unlock(&mutex_NEW);

            log_info(kernel->logger,"## (<%d>) Se crea el proceso - Estado: NEW",pcbNuevo->pid);
            
            pasarProcesoNewAReady();
            break;
        case CREACION_DE_PROCESO_ERROR:
            log_error(kernel->logger,"Kernel Memory no logro inicializar el proceso");
            //ACA DEBERIA DESCARTAR EL PROCESO? O REINTENTO ENVIAR PATH?
            break;
        
        default:
            log_warning(kernel->logger, "Operación desconocida por parte de Kernel Memory al crear el proceso");
            break;
    }*/

    pthread_mutex_lock(&mutex_NEW);
    queue_push(colaNEW, pcbNuevo);
    pthread_mutex_unlock(&mutex_NEW);

    log_info(kernel->logger,"## (<%d>) Se crea el proceso - Estado: NEW",pcbNuevo->pid);
    
    pasarProcesoNewAReady();
    
}

void enviarPathYPidKM(int pid, char* path){
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(CREACION_DE_PROCESO, buffer); 

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, path, strlen(path) + 1);

    int resultado = enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);

    if (resultado != 0) {
        log_error(kernel->logger, "Error al enviar el Path a Kernel Memory para el proceso con PID: %s", pid);
        exit(EXIT_FAILURE);
    }
    eliminar_paquete(paquete);
}

void crearMutex(char* nombreMutex){
    pthread_mutex_lock(&mutex_diccionario);
    
    // Verificar si ya existe para evitar duplicados
    if(dictionary_has_key(diccionario_mutex, nombreMutex)) {
        pthread_mutex_unlock(&mutex_diccionario);
        return;
    }

    t_mutex* nuevoMutex = malloc(sizeof(t_mutex));
                nuevoMutex->nombreMutex = strdup(nombreMutex);
                nuevoMutex->bloqueado = false;
                nuevoMutex->pidAsignado = -1;
                nuevoMutex->cola_bloqueados = queue_create();
                pthread_mutex_init(&nuevoMutex->mutex,NULL);

                dictionary_put(diccionario_mutex, nuevoMutex->nombreMutex, nuevoMutex);
                pthread_mutex_unlock(&mutex_diccionario);

                log_info(kernel->logger, "## Mutex %s creado correctamente", nuevoMutex->nombreMutex);
}

void tomarMutex(int pidSolicitaSyscall, char* nombreMutex, int socket_cpu){
    pthread_mutex_lock(&mutex_diccionario);
    t_mutex* mutex = dictionary_get(diccionario_mutex, nombreMutex);
    t_cpu_conectada* cpu = buscarCpuPorSocket(socket_cpu);
    
    if(mutex == NULL) {
        log_error(kernel->logger, "Error: El proceso %d solicitó un Mutex inexistente: %s", pidSolicitaSyscall, nombreMutex);
        pthread_mutex_unlock(&mutex_diccionario);
        return;
    }

    if(!mutex->bloqueado){
        mutex->bloqueado = true;
        mutex->pidAsignado = pidSolicitaSyscall;
        pthread_mutex_unlock(&mutex_diccionario);
        
        log_info(kernel->logger, "## (<%d>) Toma el Mutex <%s>", pidSolicitaSyscall, nombreMutex); 
        enviarPIDAcpu(pidSolicitaSyscall,cpu);
        
    } else {
        int* pid_ptr = malloc(sizeof(int));
        *pid_ptr = pidSolicitaSyscall;
        queue_push(mutex->cola_bloqueados, pid_ptr);
        pthread_mutex_unlock(&mutex_diccionario);

        // Usar pasarProcesoExecABlock para hacer la transición completa
        pasarProcesoExecABlock(pidSolicitaSyscall, cpu);
    }
}

void liberarMutex(int pidLiberaMutex, char* nombreMutex){
    pthread_mutex_lock(&mutex_diccionario);
    t_mutex* mutex = dictionary_get(diccionario_mutex, nombreMutex);

    if(mutex == NULL) {
        pthread_mutex_unlock(&mutex_diccionario);
        return;
    }

    if(mutex->bloqueado && mutex->pidAsignado == pidLiberaMutex){
        log_info(kernel->logger, "## (<%d>) Libera el Mutex <%s>", pidLiberaMutex, nombreMutex); 

        if(!queue_is_empty(mutex->cola_bloqueados)){
            int* proximo_pid_ptr = queue_pop(mutex->cola_bloqueados);
            int proximo_pid = *proximo_pid_ptr;
            free(proximo_pid_ptr);

            mutex->pidAsignado = proximo_pid;
            pthread_mutex_unlock(&mutex_diccionario);
            t_pcb* pcb_desbloqueado = sacardeColaBlockPorPID(proximo_pid);
            if(pcb_desbloqueado != NULL){
                log_info(kernel->logger, "## (<%d>) Pasa del estado <BLOCK> al estado <READY>", pcb_desbloqueado->pid); 
                encolarProcesoEnReady(pcb_desbloqueado); // Esto se encarga de ponerlo en la cola correcta segun el algoritmo
            }
        } else {
            // No hay nadie en la cola de espera, el mutex queda libre 
            mutex->bloqueado = false;
            mutex->pidAsignado = -1;
            pthread_mutex_unlock(&mutex_diccionario);
        }
    } else {
        pthread_mutex_unlock(&mutex_diccionario);
    }
}

void liberarMemoria(int pidSolicitaSyscall, int idSegmento){

    t_paquete* solicitud = crear_paquete(ELIMINACION_DE_SEGMENTO, crear_buffer());
    
    agregar_a_paquete(solicitud,&pidSolicitaSyscall,sizeof(int));
    agregar_a_paquete(solicitud,&idSegmento,sizeof(int));
    
    enviar_paquete(solicitud,kernel->socket_kernel_memory,kernel->logger);
    
    eliminar_paquete(solicitud);

    int cop_op = recibir_operacion(kernel->socket_kernel_memory);
    switch (cod op){
        case ELIMINACION_DE_SEGMENTO_OK:
            log_info(kernel->logger, "## (<%d>) libero segmento : %d ", pidSolicitaSyscall, idSegmento); 
            break;
        case ELIMINACION_DE_SEGMENTO_ERROR:
            log_error(kernel->logger, "Error: El proceso %d no logro liberar segmento: %d", pidSolicitaSyscall, idSegmento);
            break;
        default:
            log_error(kernel->logger, "Se desconoce el motivo de finalizacion de proceso");
            break;
    }
}

void finalizarProceso(int pid){

    t_pcb* pcb = buscarPcbporPIDEnColaExec(pid);
    if(pcb != NULL){
        pcb->estado = EXIT;
        pthread_mutex_lock(&mutex_EXIT);
        queue_push(colaEXIT, pcb);
        pthread_mutex_unlock(&mutex_EXIT);
        log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <EXIT>",pcb->pid);

        t_paquete* paquete = crear_paquete(ELIMINACION_DE_SEGMENTO, buffer); 

        agregar_a_paquete(paquete, &pid, sizeof(int));

        int resultado = enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);

        if (resultado != 0) {
            log_error(kernel->logger, "Error al enviar pedido de eliminacion de segmentos de proceso con PID: %d a finalizar",pid);
            exit(EXIT_FAILURE);
        }
        eliminar_paquete(paquete);
        
        eliminarProceso(pid, EXIT_PROC);
        
    }
}

void eliminarProceso(int pid, op_code motivo ){

    t_queue* colaAux = queue_create();
    t_pcb* pcbEncontrado = NULL;

    pthread_mutex_lock(&mutex_EXIT);
    int cantidad = queue_size(colaEXEC);

    for(int i = 0; i < cantidad; i++){
        t_pcb* pcb = queue_pop(colaEXIT);
        if(pcb->pid == pid && pcbEncontrado == NULL){
            pcbEncontrado = pcb;
        }else{
            queue_push(colaAux,pcb);
        }
    }

    while(!queue_is_empty(colaAux)){
        queue_push(colaEXIT, queue_pop(colaAux));
    }
    queue_destroy(colaAux);

    pthread_mutex_unlock(&mutex_EXIT);

    if (pcbEncontrado->path != NULL) {
        free(pcbEncontrado->path);
    }

    free(pcbEncontrado);


    switch (motivo)
    {
    case EXIT_PROC:
        log_info(kernel->logger,"## (<%d>) Finalizó su ejecución con motivo de <SYSCALL EXIT>",pid);
        break;
    
    default:
        log_error(kernel->logger, "Se desconoce el motivo de finalizacion de proceso");
        break;
    }
    
}

void asignarMemoria(int pidSolicitaSyscall, int idSegmento, int tamanio){
    // Guardo la solicitud para poder reintentarla si KM pide compactación
    t_solicitud_segmento* solicitud = malloc(sizeof(t_solicitud_segmento));
    solicitud->idSegmento = idSegmento;
    solicitud->tamanio = tamanio;

    char* key = string_itoa(pidSolicitaSyscall);
    pthread_mutex_lock(&mutex_segmentos_pendientes);
    dictionary_put(diccionario_segmentos_pendientes, key, solicitud);
    pthread_mutex_unlock(&mutex_segmentos_pendientes);
    free(key);

    t_paquete* solicitud_paquete = crear_paquete(CREACION_DE_SEGMENTO, crear_buffer());
    agregar_a_paquete(solicitud_paquete,&pidSolicitaSyscall,sizeof(int));
    agregar_a_paquete(solicitud_paquete,&idSegmento,sizeof(int));
    agregar_a_paquete(solicitud_paquete,&tamanio,sizeof(int));
    
    enviar_paquete(solicitud_paquete,kernel->socket_kernel_memory,kernel->logger);
    eliminar_paquete(solicitud_paquete);
    
    log_info(kernel->logger, "## (<%d>) Solicita CREACION_DE_SEGMENTO a KM - idSegmento=%d tamanio=%d", pidSolicitaSyscall, idSegmento, tamanio);
}

void liberarMemoria(int pidSolicitaSyscall, int idSegmento){

    t_paquete* solicitud = crear_paquete(ELIMINACION_DE_SEGMENTO, crear_buffer());
    
    agregar_a_paquete(solicitud,&pidSolicitaSyscall,sizeof(int));
    agregar_a_paquete(solicitud,&idSegmento,sizeof(int));
    
    enviar_paquete(solicitud,kernel->socket_kernel_memory,kernel->logger);
    
    eliminar_paquete(solicitud);

  /*  int cop_op = recibir_operacion(kernel->socket_kernel_memory);
    if (cop_op == LIBERAR_MEMORIA_OK){
        // podria verificar que se libera y pasar de suspReady a Ready y semaforo ready
    }*/

    
}