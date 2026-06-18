#include "kernel_scheduler.h"

t_pcb* crear_PCB(char* path, int prioridad){
    t_pcb* pcbCreado = malloc(sizeof(t_pcb));
    pcbCreado->pid = pidParaAsignar;
    pcbCreado->prioridad = prioridad;
    pcbCreado->path = path;
    pcbCreado->estado = NEW; //NO REQUIERO DE MEMORIA, LO CREO DIRECTAMENTE
    pcbCreado->socketCPUEjecuta = -1;
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

    pthread_mutex_lock(&mutex_interfaces[IO_SLEEP]);
    if (!interfaces[IO_SLEEP].ocupada) {
        interfaces->ocupada = true;
        enviarAIO(interfaces[IO_SLEEP].socket_interfaz, solicitud);
        pthread_mutex_unlock(&mutex_interfaces[IO_SLEEP]);
    } else {
        queue_push(interfaces[IO_SLEEP].solicitudes, solicitud);
        pthread_mutex_unlock(&mutex_interfaces[IO_SLEEP]);
    }
    
    liberarCPU(cpu);
}
/*
void manejar_stdin(int pid, uint32_t dir_logica, uint32_t tamano, t_cpu_conectada* cpu) {
    t_pcb* pcb = pasarProcesoExecABlockSinLiberar(pid);
    if (pcb == NULL) {
        log_error(kernel->logger, "Error: No se encontró el proceso %d en colaEXEC", pid);
        liberar_cpu_y_notificar(cpu);
        return;
    }

    // Crear solicitud de IO
    uint32_t datos_size = sizeof(uint32_t) * 2;
    uint32_t* params = malloc(datos_size);
    params[0] = dir_logica;
    params[1] = tamano;
    // t_solicitud_io* solicitud = crear_solicitud_io(pid, pcb, OP_STDIN, datos_size, params);
    // solicitud->dir_logica = dir_logica;

    // Encolar en cola de IO STDIN
    t_queue* cola_tipo = obtener_cola_bloqueados_por_tipo(IO_STDIN);
    pthread_mutex_t* mutex_tipo = obtener_mutex_cola_por_tipo(IO_STDIN);

    pthread_mutex_lock(mutex_tipo);
    // queue_push(cola_tipo, solicitud);
    pthread_mutex_unlock(mutex_tipo);

    // Buscar interfaz libre y enviar
    t_interfaz_conectada* interfaz_libre = buscar_interfaz_libre_por_tipo(IO_STDIN);
    if (interfaz_libre != NULL) {
        interfaz_libre->ocupada = true;
        log_info(kernel->logger, "Administración: Interfaz [%s] libre. Enviando solicitud de PID %d por red.",
                 interfaz_libre->nombre, pid);
        // enviar_operacion_a_io(interfaz_libre, solicitud);
    } else {
        log_info(kernel->logger, "## Interfaz STDIN ocupada. PID %d queda en cola de espera.", pid);
    }

    liberar_cpu_y_notificar(cpu);
}

void manejar_stdout(int pid, uint32_t dir_logica, uint32_t tamano, t_cpu_conectada* cpu) {
    t_pcb* pcb = pasarProcesoExecABlockSinLiberar(pid);
    if (pcb == NULL) {
        log_error(kernel->logger, "Error: No se encontró el proceso %d en colaEXEC", pid);
        liberar_cpu_y_notificar(cpu);
        return;
    }

    // Pedir datos a Kernel Memory
    void* datos = obtenerDatosDeKM(dir_logica, tamano);
    if (datos == NULL) {
        log_error(kernel->logger, "Error: No se pudieron obtener datos de KM para STDOUT. PID: %d", pid);
    }

    // Crear solicitud de IO con los datos obtenidos de KM
    // t_solicitud_io* solicitud = crear_solicitud_io(pid, pcb, OP_STDOUT, tamano, datos);

    // Encolar en cola de IO STDOUT
    t_queue* cola_tipo = obtener_cola_bloqueados_por_tipo(IO_STDOUT);
    pthread_mutex_t* mutex_tipo = obtener_mutex_cola_por_tipo(IO_STDOUT);

    pthread_mutex_lock(mutex_tipo);
    // queue_push(cola_tipo, solicitud);
    pthread_mutex_unlock(mutex_tipo);

    // Buscar interfaz libre y enviar
    t_interfaz_conectada* interfaz_libre = buscar_interfaz_libre_por_tipo(IO_STDOUT);
    if (interfaz_libre != NULL) {
        interfaz_libre->ocupada = true;
        log_info(kernel->logger, "Administración: Interfaz [%s] libre. Enviando solicitud de PID %d por red.",
                 interfaz_libre->nombre, pid);
        // enviar_operacion_a_io(interfaz_libre, solicitud);
    } else {
        log_info(kernel->logger, "## Interfaz STDOUT ocupada. PID %d queda en cola de espera.", pid);
    }

    liberar_cpu_y_notificar(cpu);
}
*/
void finalizarProceso(int pid, op_code motivo){
    t_pcb* pcb = NULL;
    switch (motivo){
        case CREACION_DE_PROCESO_ERROR:
            pcb = buscarPCBPorPID(pid, colaNEW,mutex_NEW);
            break;
        /*case ELIMINACION_DE_SEGMENTO_ERROR:
            pcb = buscarPCBPorPID(pid, colaEXEC,mutex_EXEC);
            break;*/
        case EXIT_PROC:
            pcb = buscarPCBPorPID(pid, colaEXEC,mutex_EXEC);
            break;
        default:
            log_error(kernel->logger, "Se desconoce el motivo de finalizacion de proceso");
            break;
    }
    

    if(pcb == NULL){
        log_error(kernel->logger, "No se encontro el proceso PID: %d, entre los procesos ejecutando", pid);
        exit(EXIT_FAILURE);
    }

    pcb->estado = EXIT;

    pthread_mutex_lock(&mutex_EXIT);
    queue_push(colaEXIT, pcb);
    pthread_mutex_unlock(&mutex_EXIT);

    log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <EXIT>",pcb->pid);

    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(motivo, buffer); 

    agregar_a_paquete(paquete, &pid, sizeof(int));

    int resultado = enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);

    if (resultado != 0) {
        log_error(kernel->logger, "Error al notificar a Kernel Memory para la finalizacion del proceso PID: %d", pid);
        exit(EXIT_FAILURE);
    }

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
    
    default:
        log_error(kernel->logger, "Se desconoce el motivo de finalizacion de proceso");
        break;
    }
    
}

void enviarAIO(int socket_io, t_solicitud_io* solicitud){
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(solicitud->tipo, buffer);

    agregar_a_paquete(paquete, &(solicitud->pidSolicitaSyscall), sizeof(int));
    if(solicitud->tipo == IO_SLEEP){
        agregar_a_paquete(paquete, &(solicitud->tiempoSleep), sizeof(int));
    } else if (solicitud->tipo == IO_STDIN){
        agregar_a_paquete(paquete, &(solicitud->tamanio), sizeof(uint32_t));
    } else if (solicitud->tipo == IO_STDOUT){
        agregar_a_paquete(paquete, &solicitud->tamanio, sizeof(uint32_t));
    }

    enviar_paquete(paquete, socket_io, kernel->logger);

    eliminar_paquete(paquete);
}

void enviarAKM(t_solicitud_io* solicitud){
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(solicitud->tipo, buffer);
    agregar_a_paquete(paquete, &solicitud->pidSolicitaSyscall, sizeof(int));
    agregar_a_paquete(paquete, &solicitud->direccion, sizeof(uint32_t));
    agregar_a_paquete(paquete, &solicitud->tamanio, sizeof(uint32_t));

    enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);

    eliminar_paquete(paquete);
}