#include "kernel_scheduler.h"

t_pcb* crear_PCB(char* path, int prioridad){
    t_pcb* pcbCreado = malloc(sizeof(t_pcb));
    pcbCreado->pid = pidParaAsignar;
    pcbCreado->prioridad = prioridad;
    pcbCreado->prioridadBase = prioridad;
    pcbCreado->mutexTomados = list_create();
    pcbCreado->path = strdup(path);
    pcbCreado->estado = NEW; //NO REQUIERO DE MEMORIA, LO CREO DIRECTAMENTE
    pcbCreado->socketCPUEjecuta = -1;
    pcbCreado->suspensionEnCurso = false; 
    pcbCreado->ioCompletadaEnTransito = false;
    pcbCreado->idBloqueoActual = 0;
    switch (obtenerPlanificacion(kernel->planification_algorithm)){
    case FIFO:
        pcbCreado->ejecutaPorRR = false;
        break;
    case RR:
        pcbCreado->ejecutaPorRR = true;
        break;
    case CMN:
        if(colaDeProcesoEjecutaRR(prioridad)){
            pcbCreado->ejecutaPorRR = true;
            log_debug(kernel->logger,"PLANIFICA CMN- CON RR");
        }
        else{
            pcbCreado->ejecutaPorRR = false;
            log_debug(kernel->logger,"PLANIFICA CMN- CON FIFO");
        }
        break;
    }
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

    int resultado = enviarPaqueteAKM(paquete);

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
        int pidDuenio = mutex->pidAsignado;
        pthread_mutex_unlock(&mutex_diccionario);

        // HERENCIA: si el que pide tiene mayor prioridad (numero MENOR) que el dueño, se la presta
        t_pcb* pcbSolicitante = buscarPCBPorPID(pidSolicitaSyscall, colaEXEC, &mutex_EXEC);
        t_pcb* pcbDuenio = buscarPCBEnCualquierEstado(pidDuenio);
        if(pcbSolicitante != NULL && pcbDuenio != NULL && pcbSolicitante->prioridad < pcbDuenio->prioridad){
            log_debug(kernel->logger, "## (<%d>) Hereda prioridad <%d> del proceso <%d>", pidDuenio, pcbSolicitante->prioridad, pidSolicitaSyscall);
            log_info(kernel->logger, "## <%d> Cambio de prioridad: <%d> - <%d>", pidDuenio, pcbDuenio->prioridad, pcbSolicitante->prioridad);
            pcbDuenio->prioridad = pcbSolicitante->prioridad;
        }        
        pasarProcesoExecABlock(pidSolicitaSyscall);
        liberarCPU(cpu);
    }
}

void liberarMutex(int pidLiberaMutex, char* nombreMutex, t_cpu_conectada* cpu){
    pthread_mutex_lock(&mutex_diccionario);
    t_mutex* mutex = dictionary_get(diccionario_mutex, nombreMutex);

    if(mutex == NULL) {
        pthread_mutex_unlock(&mutex_diccionario);
        enviarPIDAcpu(pidLiberaMutex, cpu);
        return;
    }
    t_pcb* pcbDuenioAnterior = NULL;
    t_pcb* pcbADesbloquear = NULL;
    bool huboCambioDeDuenio = false;

    if(mutex->bloqueado && mutex->pidAsignado == pidLiberaMutex){
        log_info(kernel->logger, "## (<%d>) Libera el Mutex <%s>", pidLiberaMutex, nombreMutex); 
        
        pcbDuenioAnterior = buscarPCBEnCualquierEstado(pidLiberaMutex);
        if(pcbDuenioAnterior != NULL){
            list_remove_element(pcbDuenioAnterior->mutexTomados, mutex);
        }
        if(!queue_is_empty(mutex->cola_bloqueados)){
            int* proximo_pid_ptr = queue_pop(mutex->cola_bloqueados);
            int proximo_pid = *proximo_pid_ptr;
            free(proximo_pid_ptr);

            mutex->pidAsignado = proximo_pid;
            //pthread_mutex_unlock(&mutex_diccionario);

            pcbADesbloquear = buscarPCBPorPID(proximo_pid,colaBLOCK, &mutex_BLOCK);
            if(pcbADesbloquear == NULL){
                pcbADesbloquear = buscarPCBPorPID(proximo_pid,colaBLOCK_SUSP, &mutex_BLOCK_SUSP);
            }
            if (pcbADesbloquear != NULL){
                list_add(pcbADesbloquear->mutexTomados, mutex);
            } else {
                    log_error(kernel->logger,"ERROR, no se encontro el proceso de PID: %d",proximo_pid);
            }
            huboCambioDeDuenio = true;
        } else {
            // No hay nadie en la cola de espera, el mutex queda libre 
            mutex->bloqueado = false;
            mutex->pidAsignado = -1;
        }
    }
    
    pthread_mutex_unlock(&mutex_diccionario);
    // Transiciones de estado
    if(huboCambioDeDuenio && pcbADesbloquear != NULL){
        if(pcbADesbloquear->estado == BLOCK_SUSP){
            pasarProcesoBlockSuspAReadySusp(pcbADesbloquear->pid);
        } else {
            pasarProcesoBlockaReady(pcbADesbloquear->pid);
        }
    }

    if(pcbDuenioAnterior != NULL){
        recalcularPrioridad(pcbDuenioAnterior);
    }

    enviarPIDAcpu(pidLiberaMutex, cpu);
}

t_pcb* buscarPCBEnCualquierEstado(int pid){
    t_pcb* pcb = buscarPCBPorPID(pid, colaEXEC, &mutex_EXEC);
    if(pcb != NULL) return pcb;

    pcb = buscarPCBPorPID(pid, colaBLOCK, &mutex_BLOCK);
    if(pcb != NULL) return pcb;

    pcb = buscarPCBPorPID(pid, colaBLOCK_SUSP, &mutex_BLOCK_SUSP);
    if(pcb != NULL) return pcb;

    pcb = buscarPCBPorPID(pid, colaREADY_SUSP, &mutex_READY_SUSP);
    if(pcb != NULL) return pcb;

    if(obtenerPlanificacion(kernel->planification_algorithm) == CMN){
        for(int i = 0; i < kernel->cantidadColasMultinivel; i++){
            pcb = buscarPCBPorPID(pid, colasREADY[i], &mutex_READY[i]);
            if(pcb != NULL) return pcb;
        }
    } else {
        pcb = buscarPCBPorPID(pid, colasREADY[0], &mutex_READY[0]);
        if(pcb != NULL) return pcb;
    }

    return NULL;
}

void recalcularPrioridad(t_pcb* pcb){
    pthread_mutex_lock(&mutex_diccionario);

    int prioridadCalculada = pcb->prioridadBase;

    int cantidadMutex = list_size(pcb->mutexTomados);
    for(int i = 0; i < cantidadMutex; i++){
        t_mutex* m = list_get(pcb->mutexTomados, i);

        t_queue* aux = queue_create();
        while(!queue_is_empty(m->cola_bloqueados)){
            int* pid_ptr = queue_pop(m->cola_bloqueados);
            t_pcb* pcbEsperando = buscarPCBEnCualquierEstado(*pid_ptr);
            if(pcbEsperando != NULL && pcbEsperando->prioridad < prioridadCalculada){
                prioridadCalculada = pcbEsperando->prioridad;
            }
            queue_push(aux, pid_ptr);
        }
        while(!queue_is_empty(aux)){
            queue_push(m->cola_bloqueados, queue_pop(aux));
        }
        queue_destroy(aux);
    }

    pthread_mutex_unlock(&mutex_diccionario);

    if(pcb->prioridad != prioridadCalculada){
        log_debug(kernel->logger, "## (<%d>) Prioridad efectiva pasa de <%d> a <%d> tras recalcular herencia", pcb->pid, pcb->prioridad, prioridadCalculada);
        log_info(kernel->logger, "## <%d> Cambio de prioridad: <%d> - <%d>", pcb->pid, pcb->prioridad, prioridadCalculada);
        pcb->prioridad = prioridadCalculada;
    }
}
void asignarMemoria(int pidSolicitaSyscall, int idSegmento, int tamanio){

    t_paquete* solicitud = crear_paquete(CREACION_DE_SEGMENTO, crear_buffer());
    
    agregar_a_paquete(solicitud,&pidSolicitaSyscall,sizeof(int));
    agregar_a_paquete(solicitud,&idSegmento,sizeof(int));
    agregar_a_paquete(solicitud,&tamanio,sizeof(int));
    
    enviarPaqueteAKM(solicitud);
    
    eliminar_paquete(solicitud);

    //SEMAFORO PARA INICIAR COMPACTACION
    //LO DELEGO EN ATENCION A KM()
} 

void liberarMemoria(int pidSolicitaSyscall, int idSegmento){

    t_paquete* solicitud = crear_paquete(ELIMINACION_DE_SEGMENTO, crear_buffer());
    
    agregar_a_paquete(solicitud,&pidSolicitaSyscall,sizeof(int));
    agregar_a_paquete(solicitud,&idSegmento,sizeof(int));
    
    enviarPaqueteAKM(solicitud);
    
    eliminar_paquete(solicitud);
    
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

    pthread_mutex_lock(&mutex_interfaces[IO_SLEEP]);
    queue_push(interfaces[IO_SLEEP].solicitudes, solicitud);
    pthread_mutex_unlock(&mutex_interfaces[IO_SLEEP]);

    sem_post(&sem_haySolicitudIO[IO_SLEEP]);
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

    pthread_mutex_lock(&mutex_interfaces[IO_STDIN]);
    queue_push(interfaces[IO_STDIN].solicitudes, solicitud);
    pthread_mutex_unlock(&mutex_interfaces[IO_STDIN]);
    
    sem_post(&sem_haySolicitudIO[IO_STDIN]);
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

    pthread_mutex_lock(&mutex_interfaces[IO_STDOUT]);
    queue_push(interfaces[IO_STDOUT].solicitudes, solicitud);
    pthread_mutex_unlock(&mutex_interfaces[IO_STDOUT]);
    
    sem_post(&sem_haySolicitudIO[IO_STDOUT]);
}

void finalizarProceso(int pid, op_code motivo){
    kernel->pcbFinalizados++;
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
                queue_push(colaBLOCK, queue_pop(aux));
            }
            queue_destroy(aux);
            pthread_mutex_unlock(&mutex_BLOCK);
            if( pcb == NULL){
                pthread_mutex_lock(&mutex_BLOCK_SUSP);
                t_queue* aux = queue_create();
                while (!queue_is_empty(colaBLOCK_SUSP)) {
                    t_pcb* p = queue_pop(colaBLOCK_SUSP);
                    if (pcb == NULL && p->pid == pid) pcb = p;
                    else queue_push(aux, p);
                }
                while (!queue_is_empty(aux)){
                    queue_push(colaBLOCK_SUSP, queue_pop(aux));
                }
                queue_destroy(aux);
                pthread_mutex_unlock(&mutex_BLOCK_SUSP);
                }
            break;
        }
        case CORRUPCION_MEMORIA:{
            pcb = retiraSegunPID(pid,colaNEW,mutex_NEW);
            if (pcb == NULL){
                pcb = retiraSegunPID(pid,colaEXEC,mutex_EXEC);
            }
            if (pcb == NULL){
                pcb = retiraSegunPID(pid,colaBLOCK,mutex_BLOCK);
            }
            if (pcb == NULL){
                pcb = retiraSegunPID(pid,colaBLOCK_SUSP,mutex_BLOCK_SUSP);
            }
            if (pcb == NULL){
                pcb = retiraSegunPID(pid,colaREADY_SUSP,mutex_READY_SUSP);
            }
            if (pcb == NULL){
                pcb = retiraSegunPIDdeREADY(pid);
            }
        }
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

    int resultado = enviarPaqueteAKM(paquete);

    if (resultado != 0) {
        log_error(kernel->logger, "Error al notificar a Kernel Memory para la finalizacion del proceso PID: %d", pid);
        exit(EXIT_FAILURE);
    }
    eliminar_paquete(paquete);

    //ELIMINO EL PROCESO
    eliminarProceso(pid,motivo);
    sem_post(&sem_procesoFinalizado);
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
    case DESCONEXION_IO:
        log_info(kernel->logger,"## (<%d>) Finalizó su ejecución con motivo de <DESCONEXION DE IO>",pid);
        break;
        case CORRUPCION_MEMORIA:
        log_info(kernel->logger,"## (<%d>) Finalizó su ejecución con motivo de <CORRUPCION_MEMORIA>",pid);
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
    
    log_debug(kernel->logger,"Se envio solicitud a io: %d", solicitud->tipo);
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

    enviarPaqueteAKM(paquete);

    eliminar_paquete(paquete);

    log_debug(kernel->logger,"Se envio solicitud a km: %d", solicitud->tipo);
}

void hacerSTDIN(t_solicitud_io* solicitud,int socket_io){

    enviarAIO(socket_io, solicitud);
    log_warning(kernel->logger, "EsperandoIO_OK, enviando a KM para PID %d", solicitud->pidSolicitaSyscall);
    sem_wait(&sem_recibiLecuraDeIO);
    log_warning(kernel->logger, "IO_OK, enviando a KM para PID %d", solicitud->pidSolicitaSyscall);
    enviarAKMSolicitudIO(solicitud);
    sem_wait(&sem_recibiEscrituraDeKM);// espera que KM confirme la escritura

    t_solicitud_io* ioSolicitud = retirarSolicitud(IO_STDIN, solicitud->pidSolicitaSyscall);

    log_debug(kernel->logger, "IO_OK recibido para PID %d en socket %d", solicitud->pidSolicitaSyscall, socket_io);
    if (buscarPCBPorPID(solicitud->pidSolicitaSyscall, colaBLOCK, &mutex_BLOCK) == NULL) {
        log_debug(kernel->logger, "No se encontró el PCB para PID %d en BLOCK. Verificando otras colas...", solicitud->pidSolicitaSyscall);
        t_pcb* pcbBlockSusp = buscarPCBPorPID(solicitud->pidSolicitaSyscall, colaBLOCK_SUSP, &mutex_BLOCK_SUSP);
        if (pcbBlockSusp == NULL) {
            log_error(kernel->logger, "Error: No se encontró el PCB para PID %d en ninguna cola de bloqueados.", solicitud->pidSolicitaSyscall);
        } else {
            pthread_mutex_lock(&mutex_BLOCK_SUSP);
            if (pcbBlockSusp->suspensionEnCurso) {
                pcbBlockSusp->ioCompletadaEnTransito = true;
                pthread_mutex_unlock(&mutex_BLOCK_SUSP);
                log_debug(kernel->logger, "## (<%d>) IO (STDIN) finalizó pero la suspensión en KM sigue en curso, se difiere", solicitud->pidSolicitaSyscall);
            } else {
                pthread_mutex_unlock(&mutex_BLOCK_SUSP);
                log_info(kernel->logger, "## (<%d>) finalizó IO y pasa a SUSP. READY", solicitud->pidSolicitaSyscall);
                pasarProcesoBlockSuspAReadySusp(solicitud->pidSolicitaSyscall);
                solicitarDesuspenderProceso(-1);
            }
        }    // VER PORQUE REPITE ESTA LINEA: log_info(kernel->logger, "## (<%d>) finalizó IO y pasa a SUSP. READY", solicitud->pidSolicitaSyscall);
            
    } else {
        log_debug(kernel->logger, "PCB para PID %d encontrado en BLOCK.", solicitud->pidSolicitaSyscall);
        log_info(kernel->logger, "## (<%d>) finalizó IO y pasa a READY", solicitud->pidSolicitaSyscall);
        pasarProcesoBlockaReady(solicitud->pidSolicitaSyscall);
    }
    free(ioSolicitud);
}

void hacerSTDOUT(t_solicitud_io* solicitud, int socket_io){
    enviarAKMSolicitudIO(solicitud);
    sem_wait(&sem_recibiLecuraDeKM);
    enviarAIO(socket_io, solicitud);
}