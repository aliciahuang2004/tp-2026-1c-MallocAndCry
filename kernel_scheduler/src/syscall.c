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
        log_error(kernel->logger, "Error al enviar el Path a Kernel Memory");
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
        
    } else {
        int* pid_ptr = malloc(sizeof(int));
        *pid_ptr = pidSolicitaSyscall;
        queue_push(mutex->cola_bloqueados, pid_ptr);
        pthread_mutex_unlock(&mutex_diccionario);

        t_pcb* pcb = buscarPcbporPID(pidSolicitaSyscall);
        if(pcb != NULL){
            pcb->estado = BLOCK;
            pthread_mutex_lock(&mutex_BLOCK);
            queue_push(colaBLOCK, pcb);
            pthread_mutex_unlock(&mutex_BLOCK);
            log_info(kernel->logger, "## (<%d>) Pasa del estado <EXEC> al estado <BLOCK>", pcb->pid); 
        }

        t_cpu_conectada* cpu = buscarCpuPorSocket(socket_cpu);
        if(cpu != NULL) {
            cpu->libre = true;
            cpu->pidEjecutando = -1;
        }
        sem_post(&sem_hayCPUs);
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
                pcb_desbloqueado->estado = READY;
                pthread_mutex_lock(&mutex_READY);
                queue_push(colaREADY, pcb_desbloqueado);
                pthread_mutex_unlock(&mutex_READY);
                
                log_info(kernel->logger, "## (<%d>) Pasa del estado <BLOCK> al estado <READY>", pcb_desbloqueado->pid); 
                sem_post(&sem_procesosReady);
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

void hacerSleep(int pid,int tiempo_ms){
    
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(IO_REQUEST, buffer);

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, OP_SLEEP, sizeof(t_io_operation));
    agregar_a_paquete(paquete, &tiempo_ms, sizeof(int));
    //enviar_paquete(paquete, socketIO, kernel->logger);

    eliminar_paquete(paquete);
    
}

void hacerStdOut(int pidSolicitaSyscall,int direccionALeer, int tamanioLectura){
    /*
    //PEDIR A KM DATOS DE LA DIRECCION RECIBIDA

    // ENVIAR A IO LOS DATOS RECIBIDOS DE KM
                
    */
}

void hacerStdIn(int pidSolicitaSyscall,int direccionAEscribir, int tamanioLectura){
    /*
    //PEDIR A IO INGRESO DE DATOS
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(IO_REQUEST, buffer);

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, OP_SLEEP, sizeof(t_io_operation));
    agregar_a_paquete(paquete, &tamanioLectura, sizeof(int));
    enviar_paquete(paquete, socketIO, kernel->logger);

    eliminar_paquete(paquete);

    //recibirpaquete con los datos

    // ENVIAR A KM LOS DATOS DE IO
    */
}

void finalizarProceso(){
    // ELEGIR PCB A DESTRUIR
    // DESTRUIR PCB
    // LOG FINALIZACION
}
