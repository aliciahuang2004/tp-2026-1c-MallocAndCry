#include "kernel_scheduler.h"

t_pcb* crear_PCB(char* path, int prioridad){
    t_pcb* pcbCreado = malloc(sizeof(t_pcb));
    pcbCreado->pid = pidParaAsignar;
    pcbCreado->prioridad = prioridad;
    pcbCreado->path = strdup(path);
    pcbCreado->estado = NEW; //NO REQUIERO DE MEMORIA, LO CREO DIRECTAMENTE
    pcbCreado->socketCPUEjecuta = -1;
    pidParaAsignar ++;

    return pcbCreado;
}

void crearProceso(char* path, int prioridad){
    t_pcb* pcbNuevo = crear_PCB(path, prioridad);
    
    
    
    //AGREGAR A COLA NEW, es realmente necesario?
    
    pthread_mutex_lock(&mutex_NEW);
    queue_push(colaNEW, pcbNuevo);
    pthread_mutex_unlock(&mutex_NEW);
    log_info(kernel->logger,"## (<%d>) Se crea el proceso - Estado: NEW",pcbNuevo->pid);
    enviarPathYPidKM(pcbNuevo->pid,path);
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

void tomarMutex(int pidSolicitaSyscall, char* nombreMutex, t_cpu_conectada* cpu){
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
        enviarPIDAcpu(pidSolicitaSyscall,cpu);
        
    } else {
        int* pid_ptr = malloc(sizeof(int));
        *pid_ptr = pidSolicitaSyscall;
        queue_push(mutex->cola_bloqueados, pid_ptr);
        pthread_mutex_unlock(&mutex_diccionario);

        // Usar pasarProcesoExecABlock para hacer la transición completa
        pasarProcesoExecABlock(pidSolicitaSyscall);
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

            t_pcb* pcbADesbloquear = buscarPCBPorPID(proximo_pid,colaBLOCK, mutex_BLOCK);
            if(pcbADesbloquear == NULL){
                pcbADesbloquear = buscarPCBPorPID(proximo_pid,colaBLOCK_SUSP, mutex_BLOCK_SUSP);
                if (pcbADesbloquear == NULL){
                    log_error(kernel->logger,"ERROR, no se encontro el proceso de PID: %d",proximo_pid);
                }else{
                    pasarProcesoBlockSuspAReadySusp(pcbADesbloquear->pid);
                }
                
            }else{
                pasarProcesoBlockaReady(pcbADesbloquear->pid);
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

void asignarMemoria(int pidSolicitaSyscall, int idSegmento, int tamanio){

    t_paquete* solicitud = crear_paquete(CREACION_DE_SEGMENTO, crear_buffer());
    
    agregar_a_paquete(solicitud,&pidSolicitaSyscall,sizeof(int));
    agregar_a_paquete(solicitud,&idSegmento,sizeof(int));
    agregar_a_paquete(solicitud,&tamanio,sizeof(int));
    
    enviar_paquete(solicitud,kernel->socket_kernel_memory,kernel->logger);
    
    eliminar_paquete(solicitud);

    //SEMAFORO PARA INICIAR COMPACTACION
    //LO DELEGO EN ATENCION A KM()
} 

void liberarMemoria(int pidSolicitaSyscall, int idSegmento){

    t_paquete* solicitud = crear_paquete(ELIMINACION_DE_SEGMENTO, crear_buffer());
    
    agregar_a_paquete(solicitud,&pidSolicitaSyscall,sizeof(int));
    agregar_a_paquete(solicitud,&idSegmento,sizeof(int));
    
    enviar_paquete(solicitud,kernel->socket_kernel_memory,kernel->logger);
    
    eliminar_paquete(solicitud);

    // int cop_op = recibir_operacion(kernel->socket_kernel_memory);
    /*if (cop_op == LIBERAR_MEMORIA_OK){
        // podria verificar que se libera y pasar de suspReady a Ready y semaforo ready
    }*/
    
}

void manejar_sleep(int pid, int tiempo_ms, t_cpu_conectada* cpu) {
    t_solicitud_io* solicitud = malloc(sizeof(t_solicitud_io));
    solicitud->pidSolicitaSyscall = pid;
    solicitud->tipo = IO_SLEEP;
    solicitud->tiempoSleep = tiempo_ms;
    solicitud->tamanio = 0;
    solicitud->direccion = 0;
    solicitud->leido = NULL;

    pasarProcesoExecABlock(pid);  // mueve a BLOCK
    sem_wait(&sem_hayIO[IO_SLEEP]);
    pthread_mutex_lock(&mutex_interfaces[IO_SLEEP]);
    if (!interfaces[IO_SLEEP].ocupada) {
        interfaces[IO_SLEEP].ocupada = true;
        interfaces[IO_SLEEP].pidAsignado = pid;
        pthread_mutex_unlock(&mutex_interfaces[IO_SLEEP]);
        enviarAIO(interfaces[IO_SLEEP].socket_interfaz, solicitud);
        free(solicitud);
    } else {
        queue_push(interfaces[IO_SLEEP].solicitudes, solicitud);
        pthread_mutex_unlock(&mutex_interfaces[IO_SLEEP]);
    }
    
   // liberarCPU(cpu);
}

void manejar_stdin(int pid, uint32_t dir_fisica, uint32_t tamano, t_cpu_conectada* cpu) {
    t_solicitud_io* solicitud = malloc(sizeof(t_solicitud_io));
    solicitud->pidSolicitaSyscall = pid;
    solicitud->tipo = IO_STDIN;
    solicitud->tiempoSleep = 0;
    solicitud->tamanio = tamano;
    solicitud->direccion = dir_fisica;
    solicitud->leido = NULL;

    pasarProcesoExecABlock(pid);  // mueve a BLOCK

    sem_wait(&sem_hayIO[IO_STDIN]);

    pthread_mutex_lock(&mutex_interfaces[IO_STDIN]);
    if (!interfaces[IO_STDIN].ocupada) {
        interfaces[IO_STDIN].ocupada = true;
        interfaces[IO_STDIN].pidAsignado = pid;
        queue_push(interfaces[IO_STDIN].solicitudes, solicitud);
        int socket_io = interfaces[IO_STDIN].socket_interfaz;
        pthread_mutex_unlock(&mutex_interfaces[IO_STDIN]);
        hacerSTDIN(solicitud,socket_io);
        // KM confirmo OK, ahora sí desbloqueamos
        if (buscarPCBPorPID(pid, colaBLOCK, mutex_BLOCK) == NULL) {
            pasarProcesoBlockSuspAReadySusp(pid);
        } else {
            pasarProcesoBlockaReady(pid);
        }
    } else {
        queue_push(interfaces[IO_STDIN].solicitudes, solicitud);
        pthread_mutex_unlock(&mutex_interfaces[IO_STDIN]);
    }
    if(solicitud->leido != NULL) free(solicitud->leido);
    free(solicitud);
}

void manejar_stdout(int pid, uint32_t dir_fisica, uint32_t tamano, t_cpu_conectada* cpu) {
    t_solicitud_io* solicitud = malloc(sizeof(t_solicitud_io));
    solicitud->pidSolicitaSyscall = pid;
    solicitud->tipo = IO_STDOUT;
    solicitud->tiempoSleep = 0;
    solicitud->tamanio = tamano;
    solicitud->direccion = dir_fisica;
    solicitud->leido = NULL;

    pasarProcesoExecABlock(pid);  // mueve a BLOCK

    sem_wait(&sem_hayIO[IO_STDOUT]);

    pthread_mutex_lock(&mutex_interfaces[IO_STDOUT]);
    if (!interfaces[IO_STDOUT].ocupada) {
        interfaces[IO_STDOUT].ocupada = true;
        interfaces[IO_STDOUT].pidAsignado = pid;
        queue_push(interfaces[IO_STDOUT].solicitudes, solicitud);
        int socket_io = interfaces[IO_STDOUT].socket_interfaz;
        pthread_mutex_unlock(&mutex_interfaces[IO_STDOUT]);
        hacerSTDOUT(solicitud,socket_io);
    } else {
        queue_push(interfaces[IO_STDOUT].solicitudes, solicitud);
        pthread_mutex_unlock(&mutex_interfaces[IO_STDOUT]);
    }
}

void finalizarProceso(int pid, op_code motivo){
    t_pcb* pcb = NULL;

    //BUSCAMOS PCB SEGUN MOTIVO DE FINALIZACION
    switch (motivo){
        case CREACION_DE_PROCESO_ERROR:{
            pthread_mutex_lock(&mutex_NEW);
            t_queue* aux = queue_create();
            while (!queue_is_empty(colaNEW)) {
                t_pcb* p = queue_pop(colaNEW);
                if (pcb == NULL && p->pid == pid) pcb = p;
                else queue_push(aux, p);
            }
            while (!queue_is_empty(aux)){
                queue_push(colaNEW, queue_pop(aux));
            } 
            queue_destroy(aux);
            pthread_mutex_unlock(&mutex_NEW);
            break;
        }
        case DESCONEXION_CPU:
        case SEG_FAULT:
        case EXIT_PROC:
            pthread_mutex_lock(&mutex_EXEC);
            t_queue* aux = queue_create();
            while (!queue_is_empty(colaEXEC)) {
                t_pcb* p = queue_pop(colaEXEC);
                if (pcb == NULL && p->pid == pid) pcb = p;
                else queue_push(aux, p);
            }
            while (!queue_is_empty(aux)) queue_push(colaEXEC, queue_pop(aux));
            queue_destroy(aux);
            pthread_mutex_unlock(&mutex_EXEC);
            break;
        
        /*
        case DESCONEXION_IO:{
            //BUSCAR EN BLOQUEADO O SUSPENDIDO BLOQUEADO
            pthread_mutex_lock(&mutex_BLOCK);
            t_queue* aux = queue_create();
            while (!queue_is_empty(colaBLOCK)) {
                t_pcb* p = queue_pop(colaBLOCK);
                if (pcb == NULL && p->pid == pid) pcb = p;
                else queue_push(aux, p);
            }
            while (!queue_is_empty(aux)){
                queue_push(colaEXEC, queue_pop(aux));
            }
            queue_destroy(aux);
            pthread_mutex_unlock(&mutex_BLOCK);
            break;
        }
        */
        default:{
            log_error(kernel->logger, "Se desconoce el motivo de finalizacion de proceso no se pudo retirar del estado actual");
            break;
        }
    }
    

    if(pcb == NULL){
        log_error(kernel->logger, "No se encontro el proceso PID: %d, entre los procesos ejecutando", pid);
        exit(EXIT_FAILURE);
    }

    //LO PASAMOS A LA COLA EXIT
    pcb->estado = EXIT;

    pthread_mutex_lock(&mutex_EXIT);
    queue_push(colaEXIT, pcb);
    pthread_mutex_unlock(&mutex_EXIT);

    log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <EXIT>",pcb->pid);

    //AVISAMOS A KM PARA QUE LIBERE SEGMENTOS SI LOS TIENE
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(FINALIZAR_PROCESO, buffer); //Km espera este codigo

    agregar_a_paquete(paquete, &pid, sizeof(int));

    int resultado = enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);

    if (resultado != 0) {
        log_error(kernel->logger, "Error al notificar a Kernel Memory para la finalizacion del proceso PID: %d", pid);
        exit(EXIT_FAILURE);
    }
    eliminar_paquete(paquete);

    //ELIMINO EL PROCESO
    eliminarProceso(pid,motivo);
        
}

void eliminarProceso(int pid, op_code motivo){

    t_queue* colaAux = queue_create();
    t_pcb* pcbEncontrado = NULL;

    pthread_mutex_lock(&mutex_EXIT);
    int cantidad = queue_size(colaEXIT);

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


    switch (motivo){
    case EXIT_PROC:
        log_info(kernel->logger,"## (<%d>) Finalizó su ejecución con motivo de <SYSCALL EXIT>",pid);
        break;

    case CREACION_DE_PROCESO_ERROR:
        log_info(kernel->logger,"## (<%d>) Finalizó su ejecución con motivo de <ERROR AL CREAR EL PROCESO>",pid);
        break;

    case DESCONEXION_CPU:
        log_info(kernel->logger,"## (<%d>) Finalizó su ejecución con motivo de <DESCONEXION DE CPU>",pid);
        break;
    case SEG_FAULT:
        log_info(kernel->logger,"## (<%d>) Finalizó su ejecución con motivo de <SEGMENTATION FAULT>",pid);
        break;
    default:
        log_error(kernel->logger, "Se desconoce el motivo de finalizacion de proceso");
        break;
    }
    
}

void enviarAIO(int socket_io, t_solicitud_io* solicitud){
    t_buffer* buffer = crear_buffer();
    op_code cod_io;
    switch(solicitud->tipo) {
        case IO_SLEEP:  cod_io = SLEEP;  break;
        case IO_STDIN:  cod_io = STDIN;  break;
        default:        cod_io = STDOUT; break;
    }
    t_paquete* paquete = crear_paquete(cod_io, buffer);
    agregar_a_paquete(paquete, &(solicitud->pidSolicitaSyscall), sizeof(int));
    if(solicitud->tipo == IO_SLEEP){
        agregar_a_paquete(paquete, &(solicitud->tiempoSleep), sizeof(int));
    } else if (solicitud->tipo == IO_STDIN){
        agregar_a_paquete(paquete, &(solicitud->tamanio), sizeof(uint32_t));
    } else if (solicitud->tipo == IO_STDOUT){
        agregar_a_paquete(paquete, &solicitud->tamanio, sizeof(uint32_t));  // tamanio en [2]
        agregar_a_paquete(paquete, solicitud->leido, solicitud->tamanio);   // datos reales en [3]
    }

    enviar_paquete(paquete, socket_io, kernel->logger);

    eliminar_paquete(paquete);
}

void enviarAKMSolicitudIO(t_solicitud_io* solicitud){
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = NULL;
    
    if (solicitud->tipo == IO_STDIN){
        paquete = crear_paquete(ESCRITURA_DE_DATOS, buffer);
        agregar_a_paquete(paquete, &solicitud->pidSolicitaSyscall, sizeof(int));
        agregar_a_paquete(paquete, &solicitud->direccion, sizeof(uint32_t));
        agregar_a_paquete(paquete, &solicitud->tamanio, sizeof(uint32_t));
        agregar_a_paquete(paquete, solicitud->leido, solicitud->tamanio);
    }
    if(solicitud->tipo == IO_STDOUT){
        paquete = crear_paquete(LECTURA_DE_DATOS, buffer);
        agregar_a_paquete(paquete, &solicitud->pidSolicitaSyscall, sizeof(int));
        agregar_a_paquete(paquete, &solicitud->direccion, sizeof(uint32_t));
        agregar_a_paquete(paquete, &solicitud->tamanio, sizeof(uint32_t));
    }

    enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);

    eliminar_paquete(paquete);
}

void hacerSTDIN(t_solicitud_io* solicitud,int socket_io){
    enviarAIO(socket_io, solicitud);
    sem_wait(&sem_recibiLecuraDeIO);
    enviarAKMSolicitudIO(solicitud);
    sem_wait(&sem_recibiLecuraDeKM);// espera que KM confirme la escritura
}

void hacerSTDOUT(t_solicitud_io* solicitud, int socket_io){
    
    enviarAKMSolicitudIO(solicitud);
    sem_wait(&sem_recibiLecuraDeKM);
    enviarAIO(socket_io, solicitud);
}