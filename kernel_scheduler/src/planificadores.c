#include "kernel_scheduler.h"
#include <unistd.h>

int pidParaAsignar = 0;

t_dictionary* diccionario_mutex;
pthread_mutex_t mutex_diccionario;

t_queue* colaNEW;
t_queue* colaREADY;
t_queue* colaREADY_SUSP;
t_queue* colaEXEC;
t_queue* colaBLOCK;
t_queue* colaBLOCK_SUSP;
t_queue* colaEXIT;
t_queue* colaCPUs;

pthread_mutex_t mutex_NEW;
pthread_mutex_t mutex_READY;
pthread_mutex_t mutex_READY_SUSP;
pthread_mutex_t mutex_BLOCK;
pthread_mutex_t mutex_BLOCK_SUSP;
pthread_mutex_t mutex_EXEC;
pthread_mutex_t mutex_EXIT;
pthread_mutex_t mutex_CPU;

sem_t sem_procesosReady;
sem_t sem_hayCPUs;

t_kernel_scheduler* kernel;
void iniciarPlanificadorLargoPlazo(){
    colaNEW = queue_create();
    colaREADY = queue_create();

    pthread_mutex_init(&mutex_NEW, NULL);
    pthread_mutex_init(&mutex_READY, NULL);

    sem_init(&sem_procesosReady,0,0);
}

void iniciarPlanificadorLCortoPlazo(){
    //colaREADY inicia con planificador largo
    colaEXEC = queue_create();
    colaBLOCK = queue_create();

    pthread_mutex_init(&mutex_EXEC, NULL);
    pthread_mutex_init(&mutex_BLOCK, NULL);

    // Inicialización del almacenamiento global de Mutexes
    diccionario_mutex = dictionary_create();
    pthread_mutex_init(&mutex_diccionario, NULL);
}

t_cpu_conectada* buscarCpuPorSocket(int socket_cpu) {
    t_cpu_conectada* cpuEncontrada = NULL;
    t_queue* colaAux = queue_create();

    pthread_mutex_lock(&mutex_CPU);
    int cantidad = queue_size(colaCPUs);

    for(int i = 0; i < cantidad; i++){
        t_cpu_conectada* cpu = queue_pop(colaCPUs);
        if(cpu->socket_cliente == socket_cpu){
            cpuEncontrada = cpu;
        }
        queue_push(colaAux, cpu);
    }

    while(!queue_is_empty(colaAux)){
        queue_push(colaCPUs, queue_pop(colaAux));
    }
    queue_destroy(colaAux);
    pthread_mutex_unlock(&mutex_CPU);

    return cpuEncontrada;
}

t_pcb* sacardeColaBlockPorPID(int pid) {
    t_pcb* pcbEncontrado = NULL;
    t_queue* colaAux = queue_create();

    pthread_mutex_lock(&mutex_BLOCK);
    int cantidad = queue_size(colaBLOCK);

    for(int i = 0; i < cantidad; i++) {
        t_pcb* pcb = queue_pop(colaBLOCK);
        if(pcb->pid == pid && pcbEncontrado == NULL) {
            pcbEncontrado = pcb;
        } else {
            queue_push(colaAux, pcb);
        }
    }

    while(!queue_is_empty(colaAux)) {
        queue_push(colaBLOCK, queue_pop(colaAux));
    }
    queue_destroy(colaAux);
    pthread_mutex_unlock(&mutex_BLOCK);

    return pcbEncontrado;
}

void iniciarCPU(){
    colaCPUs = queue_create();

    pthread_mutex_init(&mutex_CPU,NULL);

    sem_init(&sem_hayCPUs,0,0);
}

void pasarProcesoNewAReady(){
    pthread_mutex_lock(&mutex_NEW);
    if (!queue_is_empty(colaNEW)){
        t_pcb* pcb = queue_pop(colaNEW);
        pthread_mutex_unlock(&mutex_NEW);

        pcb->estado = READY;
        
        pthread_mutex_lock(&mutex_READY);
        queue_push(colaREADY, pcb);
        pthread_mutex_unlock(&mutex_READY);

        log_info(kernel->logger,"## (<%d>) Pasa del estado <NEW> al estado <READY>",pcb->pid);
        sem_post(&sem_procesosReady);
    }else{
        pthread_mutex_unlock(&mutex_NEW);
    }
}

void* loop_corto_plazo(void* args) {
    log_info(kernel->logger, "Planificador de Corto Plazo iniciado correctamente");
    
    while(1) {
        sem_wait(&sem_hayCPUs);
        sem_wait(&sem_procesosReady);
        pasarProcesoReadyAExec();
    }
    return NULL;
}

void pasarProcesoReadyAExec(){
    if (strcmp(kernel->planification_algorithm, "FIFO") == 0){
        log_info(kernel->logger,"EJECUCION POR FIFO");
        ejecutarPorFIFO();
    }
    if (strcmp(kernel->planification_algorithm, "RR") == 0){
        log_info(kernel->logger,"EJECUCION POR RR");
        ejecutarPorRR();
    }

    if(strcmp(kernel->planification_algorithm, "CMN") == 0){
        //COLAS MULTINIVEL
        log_info(kernel->logger,"EJECUCION COLAS MULTINIVEL");
    }
}

void ejecutarPorFIFO(){
    t_cpu_conectada* cpuElegida= elegirCPULibre();
    if(cpuElegida != NULL){
        pthread_mutex_lock(&mutex_READY);
        if (!queue_is_empty(colaREADY)){
            t_pcb* pcb = queue_pop(colaREADY);
            pthread_mutex_unlock(&mutex_READY);

            pcb->estado = EXEC;
            
            pthread_mutex_lock(&mutex_EXEC);
            queue_push(colaEXEC, pcb);
            pthread_mutex_unlock(&mutex_EXEC);
            enviarPIDAcpu(pcb->pid,cpuElegida);
            log_info(kernel->logger,"## (<%d>) Pasa del estado <READY> al estado <EXEC>",pcb->pid);
        }else{
            pthread_mutex_unlock(&mutex_READY);
        } 
    }
}

void ejecutarPorRR(){
    t_cpu_conectada* cpuElegida= elegirCPULibre();
    if(cpuElegida != NULL){
        pthread_mutex_lock(&mutex_READY);
        if (!queue_is_empty(colaREADY)){
            t_pcb* pcb = queue_pop(colaREADY);
            pthread_mutex_unlock(&mutex_READY);

            pcb->estado = EXEC;
            
            pthread_mutex_lock(&mutex_EXEC);
            queue_push(colaEXEC, pcb);
            pthread_mutex_unlock(&mutex_EXEC);
            enviarPIDAcpu(pcb->pid,cpuElegida);
            log_info(kernel->logger,"## (<%d>) Pasa del estado <READY> al estado <EXEC>",pcb->pid);
            
            log_info(kernel->logger,"INICIA QUANTUM");
            usleep(kernel->rr_quantum * 1000);
            log_info(kernel->logger,"FINALIZA QUANTUM");
            
            pedirDesalojoPorFinDeQuantum(pcb->pid, cpuElegida);

        }else{
            pthread_mutex_unlock(&mutex_READY);
        } 
    }
}

t_cpu_conectada* elegirCPULibre(){

    t_cpu_conectada* cpu_elegida = NULL;
    t_queue* colaAux = queue_create();

    pthread_mutex_lock(&mutex_CPU);
    int cantidad = queue_size(colaCPUs);

    for(int i = 0; i < cantidad; i++){
        t_cpu_conectada* cpu = queue_pop(colaCPUs);
        if (cpu_elegida == NULL && cpu->libre){
            cpu_elegida = cpu;
        } else {
            queue_push(colaAux, cpu);
        }
    }

    while(!queue_is_empty(colaAux)){
        queue_push(colaCPUs, queue_pop(colaAux));
    }
    queue_destroy(colaAux);

    if (cpu_elegida != NULL) {
        cpu_elegida->libre = false;
        queue_push(colaCPUs, cpu_elegida); 
    }
    pthread_mutex_unlock(&mutex_CPU);

    return cpu_elegida;
}

void enviarPIDAcpu(int pid, t_cpu_conectada* cpu){
    if (cpu == NULL) {
        log_error(kernel->logger, "No hay CPUs disponibles para procesar el PID %d", pid);
        return;
    }
    
    cpu->pidEjecutando = pid;

    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(PROCESO_A_PROCESAR, buffer);

    agregar_a_paquete(paquete, &pid, sizeof(int));

    enviar_paquete(paquete, cpu->socket_cliente, kernel->logger);

    eliminar_paquete(paquete);

}

void pedirDesalojoPorFinDeQuantum(int pid, t_cpu_conectada* cpu){
    if (cpu == NULL) {
        log_error(kernel->logger, "No hay CPUs disponibles para procesar el PID %d", pid);
        return;
    }

    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(PROCESO_DESALOJADO_QUANTUM, buffer);

    agregar_a_paquete(paquete, &pid, sizeof(int));

    enviar_paquete(paquete, cpu->socket_cliente, kernel->logger);

    eliminar_paquete(paquete);

    log_info(kernel->logger,"ENVIE MENSAJE A CPU PARA DESALOJAR");
            

    t_list* paquete_motivo = recibir_paquete(cpu->socket_cliente);

    if (paquete_motivo == NULL ) {
        log_error(kernel->logger, "El cliente en socket %d se desconectó o envió un paquete inválido", cpu->socket_cliente);
        EXIT_SUCCESS;
    }

    int motivo_desalojo = *(int*)list_get(paquete_motivo, 0);

    log_info(kernel->logger,"recibi paquete CON MOTIVO: %d", motivo_desalojo);

    if(motivo_desalojo == PROCESO_DESALOJADO_QUANTUM){
        log_info(kernel->logger,"## (<%d>) - Desalojado por fin de quantum",pid);
        pasarProcesoExecAReady(pid, cpu);
    } 
}

void pasarProcesoExecAReady(int pid, t_cpu_conectada* cpu){
    // buscar pid del pcb en cola exec
    // sacarlo de cola exec
    // agregarlo a ready
    t_pcb* pcb = buscarPcbporPIDEnColaExec(pid);

    if(pcb != NULL){
        pcb->estado = READY;
        pthread_mutex_lock(&mutex_READY);
        queue_push(colaREADY, pcb);
        pthread_mutex_unlock(&mutex_READY);
        log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <READY>",pcb->pid);
    }
    //LIBERAR CPU
    cpu->libre = true;
    cpu->pidEjecutando = -1;

    sem_post(&sem_procesosReady);
    sem_post(&sem_hayCPUs);
    
}

t_pcb* buscarPcbporPIDEnColaExec(int pid){
    t_pcb* pcbEncontrado = NULL;
    t_queue* colaAux = queue_create();

    pthread_mutex_lock(&mutex_EXEC);
    int cantidad = queue_size(colaEXEC);

    for(int i = 0; i < cantidad; i++){
        t_pcb* pcb = queue_pop(colaEXEC);
        if(pcb->pid == pid && pcbEncontrado == NULL){
            pcbEncontrado = pcb;
        }else{
            queue_push(colaAux,pcb);
        }
    }

    while(!queue_is_empty(colaAux)){
        queue_push(colaEXEC, queue_pop(colaAux));
    }
    queue_destroy(colaAux);

    // queue_push(colaEXEC, pcbEncontrado);
    pthread_mutex_unlock(&mutex_EXEC);

    return pcbEncontrado;
}

void pasarProcesoExecABlock(int pid, t_cpu_conectada* cpu){
    t_pcb* pcb = buscarPcbporPIDEnColaExec(pid);

    if(pcb != NULL){
        pcb->estado = BLOCK;
        pthread_mutex_lock(&mutex_BLOCK);
        queue_push(colaBLOCK, pcb);
        pthread_mutex_unlock(&mutex_BLOCK);
        log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <BLOCK>",pcb->pid);
    }
    //LIBERAR CPU
    cpu->libre = true;
    cpu->pidEjecutando = -1;
    sem_post(&sem_hayCPUs);
}

t_pcb* pasarProcesoExecABlockSinLiberar(int pid){
    t_pcb* pcb = buscarPcbporPIDEnColaExec(pid);

    if(pcb != NULL){
        pcb->estado = BLOCK;
        pthread_mutex_lock(&mutex_BLOCK);
        queue_push(colaBLOCK, pcb);
        pthread_mutex_unlock(&mutex_BLOCK);
        log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <BLOCK>",pcb->pid);
    }

    return pcb;
}

void* atender_cpu(void* socket_cpu_ptr){
    int socket_cpu = *(int*)socket_cpu_ptr;
    free(socket_cpu_ptr); // Liberamos el puntero que reservamos en el handshake
    while(1) {
        t_list* paquete = recibir_paquete(socket_cpu);
        if (!paquete) {
            log_error(kernel->logger, "Error al recibir pedido de syscall");
            return NULL;
        }

        int cod_op = *(int*) list_get(paquete, 0);
        int pidSolicitaSyscall = *(int*) list_get(paquete, 1);
        t_cpu_conectada* cpu_emisora = buscar_cpu_por_socket(socket_cpu);

        switch (cod_op) {
            case MUTEX_CREATE:{
                char* nombreMutex = (char*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MUTEX_CREATE>", pidSolicitaSyscall);
                crearMutex(nombreMutex);
                enviarPIDAcpu(pidSolicitaSyscall,cpu_emisora);
                break;
            }
            case MUTEX_LOCK:{
                char* nombreMutex = (char*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MUTEX_LOCK>", pidSolicitaSyscall);
                tomarMutex(pidSolicitaSyscall, nombreMutex, socket_cpu);
                break;
            }
            case MUTEX_UNLOCK:{
                char* nombreMutex = (char*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MUTEX_UNLOCK>", pidSolicitaSyscall);
                liberarMutex(pidSolicitaSyscall, nombreMutex);
                enviarPIDAcpu(pidSolicitaSyscall,cpu_emisora);
                break;
            }
            case MEM_ALLOC: {//POSIBLE BLOQUEADO => PEDIMOS DESALOJO
                int idSegmento = *(int*) list_get(paquete, 2);
                int tamanio = *(int*) list_get(paquete, 3);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MEM_ALLOC> idSegmento=%d tamanio=%d", pidSolicitaSyscall, idSegmento, tamanio);
                break;
            }
            case MEM_FREE: {// NO BLOQUEA
                int idSegmento = *(int*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <MEM_FREE> idSegmento=%d", pidSolicitaSyscall, idSegmento);
                break;
            }
            case SLEEP:  {
                int tiempo_ms = *(int*) list_get(paquete, 2);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <SLEEP>", pidSolicitaSyscall);
                manejar_sleep(pidSolicitaSyscall, tiempo_ms, cpu_emisora);
                break;
            }
            case STDIN:{
                uint32_t dir_logica = *(uint32_t*) list_get(paquete, 2);
                uint32_t tamano = *(uint32_t*) list_get(paquete, 3);
                log_info(kernel->logger, "## PID: %d - Solicitó Syscall STDIN (dir=%u, tam=%u)", pidSolicitaSyscall, dir_logica, tamano);
                manejar_stdin(pidSolicitaSyscall, dir_logica, tamano, cpu_emisora);
                break;
            }
            case STDOUT:{
                uint32_t dir_logica = *(uint32_t*) list_get(paquete, 2);
                uint32_t tamano = *(uint32_t*) list_get(paquete, 3);
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <STDOUT>", pidSolicitaSyscall);
                manejar_stdout(pidSolicitaSyscall, dir_logica, tamano, cpu_emisora);
                break;
            }
            case INIT_PROC: {// NO BLOQUEA
                char* path_script = (char*) list_get(paquete, 2);
                int prioridad = *(int*) list_get(paquete, 3);
                log_info(kernel->logger, "## CPU solicita creación de proceso: %s (prioridad %d)", path_script, prioridad);
                crearProceso(path_script, prioridad);
                enviarPIDAcpu(pidSolicitaSyscall,cpu_emisora);
                break;
            }
            case EXIT_PROC: {// NO BLOQUEA PERO DESALOJA PORQUE FINALIZA EL PROCESO
                log_info(kernel->logger, "## (<%d>) - Solicitó syscall: <EXIT_PROC>", pidSolicitaSyscall);
                finalizarProceso(pidSolicitaSyscall);
                liberar_cpu_y_notificar(cpu_emisora);
                break;
            }
            default:
                log_warning(kernel->logger, "Operación desconocida de CPU: %d", cod_op);
                break;
        }

        list_destroy_and_destroy_elements(paquete, free);
    }
}


t_cpu_conectada* buscar_cpu_por_socket(int socket_cpu) {
    t_cpu_conectada* encontrada = NULL;
    t_queue* colaAux = queue_create();

    pthread_mutex_lock(&mutex_CPU);
    int cantidad = queue_size(colaCPUs);

    for(int i = 0; i < cantidad; i++){
        t_cpu_conectada* cpu = queue_pop(colaCPUs);
        if (cpu->socket_cliente == socket_cpu && encontrada == NULL){
            encontrada = cpu;
        }
        queue_push(colaAux, cpu);
    }

    while(!queue_is_empty(colaAux)){
        queue_push(colaCPUs, queue_pop(colaAux));
    }
    queue_destroy(colaAux);
    pthread_mutex_unlock(&mutex_CPU);

    return encontrada;
}
void liberar_cpu_y_notificar(t_cpu_conectada* cpu) {
    if (cpu != NULL) {
        pthread_mutex_lock(&mutex_CPU);
        cpu->libre = true;
        cpu->pidEjecutando = -1;
        pthread_mutex_unlock(&mutex_CPU);
        
        // Avisamos al planificador de corto plazo que hay una cpu libre disponible
        sem_post(&sem_hayCPUs); 
        
        log_debug(kernel->logger, "Se liberó la CPU en el socket %d y se notificó al corto plazo.", cpu->socket_cliente);
    }
}