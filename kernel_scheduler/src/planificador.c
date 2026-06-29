#include "kernel_scheduler.h"

int pidParaAsignar = 0;
bool noHayCompactacion = true;
pthread_t hiloQuantum;

t_queue* colaNEW;
t_queue** colasREADY;
t_queue* colaREADY_SUSP;
t_queue* colaEXEC;
t_queue* colaBLOCK;
t_queue* colaBLOCK_SUSP;
t_queue* colaEXIT;

t_queue* colaCPUs;
pthread_mutex_t mutex_CPU;

pthread_mutex_t mutex_NEW;
pthread_mutex_t* mutex_READY;
pthread_mutex_t mutex_READY_SUSP;
pthread_mutex_t mutex_EXEC;
pthread_mutex_t mutex_BLOCK;
pthread_mutex_t mutex_BLOCK_SUSP;
pthread_mutex_t mutex_EXIT;

sem_t sem_hayProcesosEnReady;
sem_t sem_readyPrioridad;
sem_t sem_hayCPUdisponible;
sem_t sem_finSyscall;
sem_t sem_hayMemoria;
sem_t* sem_hayIO;
sem_t* sem_haySolicitudIO;
sem_t sem_recibiLecuraDeIO;
sem_t sem_recibiLecuraDeKM;
sem_t sem_suspension_ok;
sem_t sem_desuspension_ok;

t_dictionary* diccionario_mutex;
pthread_mutex_t mutex_diccionario;

// t_list* lista_interfaces_io;
t_interfaz_conectada interfaces[3];
pthread_mutex_t mutex_interfaces[3];

void inicializarColas(){
    colaNEW = queue_create();
    if(strcmp(kernel->planification_algorithm,"CMN") == 0 ){
        colasREADY = malloc(kernel->cantidadColasMultinivel * sizeof(t_queue*));
        for (int i = 0; i < kernel->cantidadColasMultinivel; i++) {
            colasREADY[i] = queue_create();
        }
    }else{
        colasREADY = malloc( 1 * sizeof(t_queue*));
        colasREADY[0] = queue_create();
    }
    colaREADY_SUSP = queue_create();
    colaEXEC = queue_create();
    colaBLOCK = queue_create();
    colaBLOCK_SUSP = queue_create();
    colaEXIT = queue_create();

    colaCPUs = queue_create();

}

void inicializarSemaforos(){
    //MUTEX
    pthread_mutex_init(&mutex_NEW, NULL);
    if(obtenerPlanificacion(kernel->planification_algorithm) == CMN){
       mutex_READY = malloc(kernel->cantidadColasMultinivel * sizeof(pthread_mutex_t));
        for (int i = 0; i < kernel->cantidadColasMultinivel; i++) {
            pthread_mutex_init(&mutex_READY[i], NULL);
        }
    }else{
        mutex_READY = malloc( 1 * sizeof(pthread_mutex_t));
        pthread_mutex_init(&mutex_READY[0], NULL);
    }
    pthread_mutex_init(&mutex_READY_SUSP, NULL);
    pthread_mutex_init(&mutex_EXEC, NULL);
    pthread_mutex_init(&mutex_BLOCK, NULL);
    pthread_mutex_init(&mutex_BLOCK_SUSP, NULL);
    pthread_mutex_init(&mutex_EXIT, NULL);

    //SEMAFOROS
    sem_init(&sem_hayCPUdisponible,0,0); // CPU se conecta o liberamos
    sem_init(&sem_hayMemoria,0,0);  //CHEQUEAR
    sem_init(&sem_hayProcesosEnReady, 0, 0);
    sem_init(&sem_readyPrioridad,0,0);
    sem_init(&sem_finSyscall,0,0);  // 
    sem_init(&sem_recibiLecuraDeIO,0,0);
    sem_init(&sem_recibiLecuraDeKM,0,0);
    sem_init(&sem_suspension_ok, 0, 0);
    sem_init(&sem_desuspension_ok, 0, 0);

    diccionario_mutex = dictionary_create();
    pthread_mutex_init(&mutex_diccionario, NULL);

}

void pasarProcesoNewAReady(){
    // saco de New
    pthread_mutex_lock(&mutex_NEW);
    if (!queue_is_empty(colaNEW)){
        t_pcb* pcb = queue_pop(colaNEW);
        pthread_mutex_unlock(&mutex_NEW);

        //paso a Ready

        pcb->estado = READY;

        switch (obtenerPlanificacion(kernel->planification_algorithm)){
        case FIFO:
        case RR:
            pthread_mutex_lock(&mutex_READY[0]);
            queue_push(colasREADY[0],pcb);
            pthread_mutex_unlock(&mutex_READY[0]);
            break;

        case CMN:
            pthread_mutex_lock(&mutex_READY[pcb->prioridad]);
            queue_push(colasREADY[pcb->prioridad],pcb);
            pthread_mutex_unlock(&mutex_READY[pcb->prioridad]);
            sem_post(&sem_readyPrioridad); 
            break;
        
        default:
            log_error(kernel->logger,"Se desconoce el algortimo elegido para la planificacion");
            break;
        }
       
        log_info(kernel->logger,"## (<%d>) Pasa del estado <NEW> al estado <READY>",pcb->pid);
        sem_post(&sem_hayProcesosEnReady);
    }else{
        pthread_mutex_unlock(&mutex_NEW);
    }

}

t_planificador obtenerPlanificacion(char* planificador){
    if(strcmp(planificador, "FIFO") == 0) return FIFO;
    if(strcmp(planificador, "RR") == 0) return RR;
    if(strcmp(planificador, "CMN") == 0) return CMN;
    return -1; // desconocido
}

void pasarProcesoReadyAExec(){
    t_pcb* pcbAEjecutar;

    //ELEGIR DE READY

    switch (obtenerPlanificacion(kernel->planification_algorithm)){
        case FIFO:
            pcbAEjecutar = elegirPorFIFO();
            break;
        case RR:
            pcbAEjecutar = elegirPorRR();
            break;

        case CMN:
            pcbAEjecutar = elegirPorCMN();
            break;
        default:
            log_error(kernel->logger,"Se desconoce el algortimo elegido para la planificacion");
            break;
    }
    // ELEGIR CPU
    t_cpu_conectada* cpuElegida = elegirCPU();
    if(cpuElegida == NULL || pcbAEjecutar == NULL) {
        log_error(kernel->logger,"No se pudo asignar proceso a CPU:%d PID:%d", cpuElegida->id_cpu, pcbAEjecutar->pid);
        return;
    }
    // LO AGREGO A EXEC
    cpuElegida->pidEjecutando = pcbAEjecutar->pid;
    cpuElegida->pcbEjecutando = pcbAEjecutar;
    
    pcbAEjecutar->estado = EXEC;
    pcbAEjecutar->socketCPUEjecuta = cpuElegida->socket_cliente;

    pthread_mutex_lock(&mutex_EXEC);
    queue_push(colaEXEC,pcbAEjecutar);
    pthread_mutex_unlock(&mutex_EXEC);

    log_info(kernel->logger,"## (<%d>) Pasa del estado <READY> al estado <EXEC>",pcbAEjecutar->pid);

    // ENVIAR A CPU
    enviarPIDAcpu(pcbAEjecutar->pid,cpuElegida);

    if(buscarPCBPorPID(pcbAEjecutar->pid,colaEXEC,mutex_EXEC)->ejecutaPorRR){
        log_debug(kernel->logger,"Iniciando el temporizador por %d ms...",kernel->rr_quantum);
        pthread_create(&hiloQuantum, NULL, iniciarTemporizadorRR, cpuElegida);
        pthread_detach(hiloQuantum);
    }
}

t_pcb* elegirPorFIFO(){
    t_pcb* pcb = NULL; 
    pthread_mutex_lock(&mutex_READY[0]);
    if (!queue_is_empty(colasREADY[0])){
        pcb = queue_pop(colasREADY[0]);
        }
    pthread_mutex_unlock(&mutex_READY[0]);
    return pcb;
}

t_pcb* elegirPorRR(){
    t_pcb* pcb = NULL; 
    pthread_mutex_lock(&mutex_READY[0]);
    if (!queue_is_empty(colasREADY[0])){
        pcb = queue_pop(colasREADY[0]);// primero guardo en una variable
    }    
    pthread_mutex_unlock(&mutex_READY[0]); //soltar el mutex antes de retornar
    return pcb;
}

t_pcb* elegirPorCMN(){
    t_pcb* pcbELegido = NULL;
    for (int i = 0;i<kernel->cantidadColasMultinivel;i++){
        pthread_mutex_lock(&mutex_READY[i]);
        if (!queue_is_empty(colasREADY[i])){
            pcbELegido = queue_pop(colasREADY[i]);
        }
        pthread_mutex_unlock(&mutex_READY[i]);
    }
    if (pcbELegido != NULL){
        return pcbELegido;
    }
    return NULL;
}

int colaDeProcesoEjecutaRR(int prioridad){
    char* algoritmo = kernel->queues_algorithms[prioridad];
    switch (obtenerPlanificacion(algoritmo)){
        case FIFO:
            return 0;
            break;
        case RR:
            return 1;
            break;
        default:
            log_error(kernel->logger,"Se desconoce el algortimo elegido para la prioridad: %d", prioridad);
            break;
    }
    return -1;
}

t_cpu_conectada* elegirCPU(){
    t_cpu_conectada* cpu_elegida = buscarCPULibre();
    //MARCO CPU COMO OCUPADA Y LA AGREGO A LA COLA
    if (cpu_elegida != NULL) {
        cpu_elegida->libre = false;
    }
    return cpu_elegida;
}

t_cpu_conectada* buscarCPULibre(){
    t_cpu_conectada* cpu_elegida = NULL;
    t_queue* colaAux = queue_create();

    pthread_mutex_lock(&mutex_CPU);

    //REVISO COLA CPU MIENTRAS BUSCO UNA LIBRE
    while (!queue_is_empty(colaCPUs)){
        t_cpu_conectada* cpu = queue_pop(colaCPUs);
        if (cpu_elegida == NULL && cpu->libre){
            cpu_elegida = cpu;
            queue_push(colaCPUs, cpu_elegida);
        } else {
            queue_push(colaAux, cpu);
        }
    }
    while(!queue_is_empty(colaAux)){
        queue_push(colaCPUs, queue_pop(colaAux));
    }
    queue_destroy(colaAux);
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

void* iniciarTemporizadorRR(void* arg){
    t_cpu_conectada* cpu = (t_cpu_conectada*) arg;
    
    usleep(kernel->rr_quantum * 1000);
    log_debug(kernel->logger,"Finalizo el temporizador, notificando desalojo a cpu ID: %d", cpu-> id_cpu);
    
    notificarDesalojo(cpu, NULL, PROCESO_DESALOJADO_QUANTUM);
    
    return NULL;
}

void notificarDesalojo(t_cpu_conectada* cpu, t_pcb* pcbOtroProceso, op_code motivo){

    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(motivo, buffer);

    agregar_a_paquete(paquete, &cpu->pidEjecutando, sizeof(int));

    enviar_paquete(paquete, cpu->socket_cliente, kernel->logger);

    eliminar_paquete(paquete);

    switch (motivo){
    case PROCESO_DESALOJADO_QUANTUM:
        log_info(kernel->logger,"## (<%d>) - Desalojado por fin de quantum",cpu->pidEjecutando);
        break;
    case PROCESO_DESALOJADO_PRIORIDAD:
        t_pcb* pcbEnExec = buscarPCBPorPID(cpu->pidEjecutando,colaEXEC,mutex_EXEC);
        log_info(kernel->logger,"## (<%d>) Prioridad: <%d> - Desalojado por cola más prioritaria por el proceso <%d> con prioridad <%d>",pcbEnExec->pid, pcbEnExec->prioridad, pcbOtroProceso->pid,pcbOtroProceso->prioridad);
        break;
    default:
        break;
    }
}

void* monitorPrioridades(void* arg){
    while(1) {
        while(noHayCompactacion){
            sem_wait(&sem_readyPrioridad);
            while(buscarCPULibre() == NULL){
                pthread_mutex_lock(&mutex_CPU);
                int cantidad = queue_size(colaCPUs);    
                for(int i = 0; i < cantidad; i++) {
                    t_cpu_conectada* cpu = queue_pop(colaCPUs);
                    if(!cpu->libre ) {
                        int prioridadActual = cpu->pidEjecutando;
                        int nuevaPrioridad = procesoMasPrioritario(prioridadActual);
                        if(nuevaPrioridad != -1) {
                            t_pcb* pcbMasPrioridad = buscarPCBenReadyConPrioridad(nuevaPrioridad);
                            notificarDesalojo(cpu, pcbMasPrioridad, PROCESO_DESALOJADO_PRIORIDAD);
                        }
                    }
                    queue_push(colaCPUs,cpu);
                }
                pthread_mutex_unlock(&mutex_CPU);
            }
        }
    }
    return NULL;
}

int procesoMasPrioritario(int prioridadActual){
    for(int i = 0; i < prioridadActual; i++) {
        if(!queue_is_empty(colasREADY[i])) {
            return i;
        }
    }
    return -1;
}

t_pcb* buscarPCBPorPID(int pid, t_queue* cola, pthread_mutex_t mutex){
    t_pcb* pcbEncontrada = NULL;

    t_queue* colaAux = queue_create();
    
    pthread_mutex_lock(&mutex);

    while (!queue_is_empty(cola)){
        t_pcb* pcb = queue_pop(cola);
        if (pcbEncontrada == NULL && pcb->pid == pid){
            pcbEncontrada = pcb;
        }
        queue_push(colaAux, pcb);
    }

    while(!queue_is_empty(colaAux)){
        queue_push(cola, queue_pop(colaAux));
    }
    
    queue_destroy(colaAux);
    
    pthread_mutex_unlock(&mutex);
    
    return pcbEncontrada;

}

t_pcb* buscarPCBenReadyConPrioridad(int prioridad){
    pthread_mutex_lock(&mutex_READY[prioridad]);
    if (!queue_is_empty(colasREADY[prioridad])){
        t_pcb* pcb = queue_peek(colasREADY[prioridad]);
        pthread_mutex_unlock(&mutex_READY[prioridad]);
        return pcb;
    }
    pthread_mutex_unlock(&mutex_READY[prioridad]);
    return NULL;
}

void pasarProcesoExecAReady(int pid){
    t_pcb* pcb = NULL;

    //SACO DE EXEC
    pthread_mutex_lock(&mutex_EXEC);
    
    int cantidad = queue_size(colaEXEC);
    for(int i = 0; i < cantidad; i++){
        t_pcb* pcbEjecuta = queue_pop(colaEXEC);
        if (pcb == NULL && pcbEjecuta->pid == pid){
            pcb = pcbEjecuta;
        }else{
            queue_push(colaEXEC,pcbEjecuta);
        }        
    }
    pthread_mutex_unlock(&mutex_EXEC);

    pcb->estado = READY;
    pcb->socketCPUEjecuta = -1;
    
    //AGREGO A READY
    switch (obtenerPlanificacion(kernel->planification_algorithm)){
    case FIFO:
    case RR:
        pthread_mutex_lock(&mutex_READY[0]);
        queue_push(colasREADY[0],pcb);
        pthread_mutex_unlock(&mutex_READY[0]);
        break;

    case CMN:
        pthread_mutex_lock(&mutex_READY[pcb->prioridad]);
        queue_push(colasREADY[pcb->prioridad],pcb);
        pthread_mutex_unlock(&mutex_READY[pcb->prioridad]);
        sem_post(&sem_readyPrioridad); 
        break;   
    default:
        log_error(kernel->logger,"Se desconoce el algortimo elegido para la planificacion");
        break;
    }
    
    log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <READY>",pcb->pid);
    sem_post(&sem_hayProcesosEnReady);
}

void liberarCPU(t_cpu_conectada* cpu){
    pthread_mutex_lock(&mutex_CPU);
    cpu->libre = true;
    cpu->pidEjecutando = -1;
    cpu->pcbEjecutando = NULL;
    pthread_mutex_unlock(&mutex_CPU);
    sem_post(&sem_hayCPUdisponible);
}

void pasarProcesoExecABlock(int pid){
    t_pcb* pcb = NULL;

    //SACO DE EXEC
    pthread_mutex_lock(&mutex_EXEC);
    
    int cantidad = queue_size(colaEXEC);
    for(int i = 0; i < cantidad; i++){
        t_pcb* pcbEjecuta = queue_pop(colaEXEC);
        if (pcb == NULL && pcbEjecuta->pid == pid){
            pcb = pcbEjecuta;
        }else{
            queue_push(colaEXEC,pcbEjecuta);
        }        
    }
    pthread_mutex_unlock(&mutex_EXEC);

    pcb->estado = BLOCK;
    // int socket_cpu = pcb->socketCPUEjecuta;
    pcb->socketCPUEjecuta = -1;
    /*
    LIBERO AL RECIBIR SYSCALL
    t_cpu_conectada* cpu = buscar_cpu_por_socket(socket_cpu);
    if(cpu != NULL){
        //LIBERO CPU
        liberarCPU(cpu);
    }else{
        log_error(kernel->logger, "ERROR al liberar CPU");
    }
    */
    //AGREGO A BLOCK
    pthread_mutex_lock(&mutex_BLOCK);
    queue_push(colaBLOCK,pcb);
    pthread_mutex_unlock(&mutex_BLOCK);
    
    log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <BLOCK>",pcb->pid);

    //CORRO TEMPORIZADOR PARA PASAR A SUSP BLOCK
    pthread_t hiloBlock;
    pthread_create(&hiloBlock, NULL, iniciarTemporizadorSuspendido, pcb);
    pthread_detach(hiloBlock);

}

void* iniciarTemporizadorSuspendido(void* arg){
    t_pcb* pcb = (t_pcb*) arg;
    
    usleep(kernel->suspension_time * 1000);
    
    if (pcb->estado == BLOCK){
        pasarProcesoBlockABlockSusp(pcb->pid);
    }
    
    return NULL;
}

void pasarProcesoBlockaReady(int pid){
    t_pcb* pcb = NULL;

    //SACO DE BLOCK
    pthread_mutex_lock(&mutex_BLOCK);
    
    int cantidad = queue_size(colaBLOCK);
    for(int i = 0; i < cantidad; i++){
        t_pcb* pcbBlock = queue_pop(colaBLOCK);
        if (pcb == NULL && pcbBlock->pid == pid){
            pcb = pcbBlock;
        }else{
            queue_push(colaBLOCK,pcbBlock);
        }        
    }
    pthread_mutex_unlock(&mutex_BLOCK);

    //AGREGO A READY

    pcb->estado = READY;
    
    switch (obtenerPlanificacion(kernel->planification_algorithm)){
        case FIFO:
        case RR:
            pthread_mutex_lock(&mutex_READY[0]);
            queue_push(colasREADY[0],pcb);
            pthread_mutex_unlock(&mutex_READY[0]);
            break;

        case CMN:
            pthread_mutex_lock(&mutex_READY[pcb->prioridad]);
            queue_push(colasREADY[pcb->prioridad],pcb);
            pthread_mutex_unlock(&mutex_READY[pcb->prioridad]);
            sem_post(&sem_readyPrioridad); 
            break;
        
        default:
            log_error(kernel->logger,"Se desconoce el algortimo elegido para la planificacion");
            break;
    }
       
    log_info(kernel->logger,"## (<%d>) Pasa del estado <BLOCK> al estado <READY>",pcb->pid);
    sem_post(&sem_hayProcesosEnReady); 
}

void pasarProcesoBlockABlockSusp(int pid){
    t_pcb* pcb = NULL;
    //SACO A BLOCK
    pthread_mutex_lock(&mutex_BLOCK);
    
    int cantidad = queue_size(colaBLOCK);
    for(int i = 0; i < cantidad; i++){
        t_pcb* pcbBlock = queue_pop(colaBLOCK);
        if (pcb == NULL && pcbBlock->pid == pid){
            pcb = pcbBlock;
        }else{
            queue_push(colaBLOCK,pcbBlock);
        }        
    }
    pthread_mutex_unlock(&mutex_BLOCK);

    if (pcb != NULL) {
        log_info(kernel->logger, "## (<%d>) Iniciando traspaso a SWAP...", pcb->pid);

        
        t_paquete* paquete = crear_paquete(SUSPENSION_DE_PROCESO, crear_buffer());
        agregar_a_paquete(paquete, &pid, sizeof(int));
        enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);
        eliminar_paquete(paquete);

        sem_wait(&sem_suspension_ok);

        pcb->estado = BLOCK_SUSP;

        //AGREGO SUSP BLOCK
        pthread_mutex_lock(&mutex_BLOCK_SUSP);
        queue_push(colaBLOCK_SUSP,pcb);
        pthread_mutex_unlock(&mutex_BLOCK_SUSP);

        log_info(kernel->logger,"## (<%d>) Pasa del estado <BLOCK> al estado <BLOCK_SUSP>",pcb->pid);


    } else {
        log_error(kernel->logger, "Error: No se encontró el PID %d en la cola BLOCK", pid);
    }

}

void pasarProcesoBlockSuspAReadySusp(int pid){
    t_pcb* pcb = NULL;
    //SACO A SUSP BLOCK
    pthread_mutex_lock(&mutex_BLOCK_SUSP);
    
    int cantidad = queue_size(colaBLOCK_SUSP);
    for(int i = 0; i < cantidad; i++){
        t_pcb* pcbBlock = queue_pop(colaBLOCK_SUSP);
        if (pcb == NULL && pcbBlock->pid == pid){
            pcb = pcbBlock;
        }else{
            queue_push(colaBLOCK_SUSP,pcbBlock);
        }        
    }
    pthread_mutex_unlock(&mutex_BLOCK_SUSP);

    pcb->estado = READY_SUSP;

    //AGREGO SUSP BLOCK
    pthread_mutex_lock(&mutex_READY_SUSP);
    queue_push(colaREADY_SUSP,pcb);
    pthread_mutex_unlock(&mutex_READY_SUSP);

    log_info(kernel->logger,"## (<%d>) Pasa del estado <BLOCK_SUSP> al estado <READY_SUSP>",pcb->pid);

    sem_wait(&sem_hayMemoria);
    pasarProcesoReadySuspAReady(pid);
}

void pasarProcesoReadySuspAReady(int pid){
    t_pcb* pcb = NULL;
    //SACO A SUSP BLOCK
    pthread_mutex_lock(&mutex_READY_SUSP);
    
    int cantidad = queue_size(colaREADY_SUSP);
    for(int i = 0; i < cantidad; i++){
        t_pcb* pcbBlock = queue_pop(colaREADY_SUSP);
        if (pcb == NULL && pcbBlock->pid == pid){
            pcb = pcbBlock;
        }else{
            queue_push(colaREADY_SUSP,pcbBlock);
        }        
    }
    pthread_mutex_unlock(&mutex_READY_SUSP);

    if (pcb != NULL) {
        log_info(kernel->logger,"## (<%d>) Intentando desuspender...", pcb->pid);

        t_paquete* paquete = crear_paquete(DESUSPENSION_DE_PROCESO, crear_buffer());
        agregar_a_paquete(paquete, &pid, sizeof(int));
        enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);
        eliminar_paquete(paquete);

        sem_wait(&sem_desuspension_ok);

    //AGREGO A READY

    pcb->estado = READY;
    
    switch (obtenerPlanificacion(kernel->planification_algorithm)){
        case FIFO:
        case RR:
            pthread_mutex_lock(&mutex_READY[0]);
            queue_push(colasREADY[0],pcb);
            pthread_mutex_unlock(&mutex_READY[0]);
            break;

        case CMN:
            pthread_mutex_lock(&mutex_READY[pcb->prioridad]);
            queue_push(colasREADY[pcb->prioridad],pcb);
            pthread_mutex_unlock(&mutex_READY[pcb->prioridad]);
            sem_post(&sem_readyPrioridad); 
            break;
        
        default:
            log_error(kernel->logger,"Se desconoce el algortimo elegido para la planificacion");
            break;
    }
       
    log_info(kernel->logger,"## (<%d>) Pasa del estado <READY_SUSP> al estado <READY>",pcb->pid);
    sem_post(&sem_hayProcesosEnReady); 

    } else {
        log_error(kernel->logger, "Error: No se encontró el PID %d en la cola READY_SUSP", pid);
    }

}

void pasarProcesoExecAExit(){
    //SACO DE EXEC
    // pthread_mutex_lock(&mutex_EXEC);
    
    // int cantidad = queue_size(colaEXEC);
    // for(int i = 0; i < cantidad; i++){
    //     t_pcb* pcbEjecuta = queue_pop(colaEXEC);
    //     if (pcb == NULL && pcbEjecuta->pid == pid){
    //         pcb = pcbEjecuta;
    //     }else{
    //         queue_push(colaEXEC,pcbEjecuta);
    //     }        
    // }
    // pthread_mutex_unlock(&mutex_EXEC);
}
void pasarProcesoReadyAExit(){
    //SACO DE READY
    // pthread_mutex_lock(&mutex_READY);
    
    // int cantidad = queue_size(colaREADY);
    // for(int i = 0; i < cantidad; i++){
    //     t_pcb* pcbReady = queue_pop(colaREADY);
    //     if (pcb == NULL && pcbReady->pid == pid){
    //         pcb = pcbReady;
    //     }else{
    //         queue_push(colaREADY,pcbReady);
    //     }        
    // }
    // pthread_mutex_unlock(&mutex_READY);
}
void pasarProcesoBlockAExit(){
    //SACO DE BLOCK
    // pthread_mutex_lock(&mutex_BLOCK);    
    // int cantidad = queue_size(colaBLOCK);
    // for(int i = 0; i < cantidad; i++){
    //     t_pcb* pcbBlock = queue_pop(colaBLOCK);
    //     if (pcb == NULL && pcbBlock->pid == pid){
    //         pcb = pcbBlock;
    //     }else{
    //         queue_push(colaBLOCK,pcbBlock);
    //     }
    // }
    // pthread_mutex_unlock(&mutex_BLOCK);
}

t_cpu_conectada* buscarCPUSegunPID(int pid){
    t_cpu_conectada* cpu_elegida = NULL;
    t_queue* colaAux = queue_create();

    pthread_mutex_lock(&mutex_CPU);

    while (!queue_is_empty(colaCPUs)){
        t_cpu_conectada* cpu = queue_pop(colaCPUs);
        if (cpu_elegida == NULL && cpu->pidEjecutando == pid){
            cpu_elegida = cpu;
        }
        queue_push(colaAux, cpu);
    }

    while(!queue_is_empty(colaAux)){
        queue_push(colaCPUs, queue_pop(colaAux));
    }
    
    queue_destroy(colaAux);
    pthread_mutex_unlock(&mutex_CPU);

    return cpu_elegida;
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

void* loop_corto_plazo(void* args) {
    log_info(kernel->logger, "Planificador de Corto Plazo iniciado correctamente");
    
    while(1) {
        sem_wait(&sem_hayCPUdisponible);
        sem_wait(&sem_hayProcesosEnReady);
            pasarProcesoReadyAExec();
    }
    return NULL;
}

t_tipo_io buscarTipoIOPorSocket(int socket_io) {
   // pthread_mutex_lock(&mutex_interfaces[0]);
    for (int i = 0; i < 3; i++) {
        pthread_mutex_lock(&mutex_interfaces[i]);
        if (interfaces[i].socket_interfaz == socket_io) {
            pthread_mutex_unlock(&mutex_interfaces[i]);
            return interfaces[i].tipo;
        }
        pthread_mutex_unlock(&mutex_interfaces[i]);
    }
    return -1; // No se encontró el tipo de IO para el socket dado
}

void reencolarAlInicio(int pid){
    t_pcb* pcb = NULL;

    //SACO DE EXEC
    pthread_mutex_lock(&mutex_EXEC);
    
    int cantidad = queue_size(colaEXEC);
    for(int i = 0; i < cantidad; i++){
        t_pcb* pcbEjecuta = queue_pop(colaEXEC);
        if (pcb == NULL && pcbEjecuta->pid == pid){
            pcb = pcbEjecuta;
        }else{
            queue_push(colaEXEC,pcbEjecuta);
        }        
    }
    pthread_mutex_unlock(&mutex_EXEC);

    pcb->estado = READY;
        
    //AGREGO A READY

    t_queue* colaAux = queue_create();
    
    switch (obtenerPlanificacion(kernel->planification_algorithm)){
    case FIFO:
    case RR:
        //SACO TODO DE LA COLA, AGREGO, METO LO SACADO
        pthread_mutex_lock(&mutex_READY[0]);
        while(!queue_is_empty(colasREADY[0])){
            queue_push(colaAux,queue_pop(colasREADY[0]));
        }
        queue_push(colasREADY[0],pcb);
        while(!queue_is_empty(colaAux)){
            queue_push(colasREADY[0],queue_pop(colaAux));
        }
        pthread_mutex_unlock(&mutex_READY[0]);
        break;

    case CMN:
        int prioridad = pcb->prioridad;
        pthread_mutex_lock(&mutex_READY[prioridad]);
        while(!queue_is_empty(colasREADY[prioridad])){
            queue_push(colaAux,queue_pop(colasREADY[prioridad]));
        }
        queue_push(colasREADY[prioridad],pcb);
        while(!queue_is_empty(colaAux)){
            queue_push(colasREADY[prioridad],queue_pop(colaAux));
        }
        pthread_mutex_unlock(&mutex_READY[prioridad]);
        break;   
    default:
        log_error(kernel->logger,"Se desconoce el algortimo elegido para la planificacion");
        break;
    }
    queue_destroy(colaAux);
    log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <READY>",pcb->pid);
    sem_post(&sem_hayProcesosEnReady);
}

void inicializarHilos(){
    //CPU E IO AL CONECTARSE

    //KM
    pthread_t hilo_escucha_km;
    if (pthread_create(&hilo_escucha_km, NULL, atender_kernel_memory, NULL) != 0) {
        log_error(kernel->logger, "No se pudo crear el hilo de escucha de Kernel Memory");
        return ;
    }
    pthread_detach(hilo_escucha_km);

    //PLANIFICADOR CORTO PLAZO
    pthread_t hilo_corto_plazo;
    if (pthread_create(&hilo_corto_plazo, NULL, loop_corto_plazo, NULL) != 0) {
        log_error(kernel->logger, "No se pudo crear el hilo del Planificador de Corto Plazo");
        return ;
    }
    pthread_detach(hilo_corto_plazo);

    //MONITOR PRIORIDADES
    if(kernel->queues_preemption && obtenerPlanificacion(kernel->planification_algorithm) == CMN){
        pthread_t hiloMonitorPrioridades;
        if (pthread_create(&hiloMonitorPrioridades, NULL, monitorPrioridades, NULL) != 0) {
            log_error(kernel->logger, "No se pudo crear el hilo de monitorización de prioridades");
            return ;
        }
        pthread_detach(hiloMonitorPrioridades);
    }

    //ATENCION DE SYSCALL IO
    pthread_t solicitudSleep;
    if(pthread_create(&solicitudSleep, NULL, atencionIOsleep, NULL) != 0){
        log_error(kernel->logger, "No se pudo crear el hilo para atencion a syscall SLEEP");
            return ;
    }
    pthread_detach(solicitudSleep);

}

void* atencionIOsleep(void* args) {
    
    while(1) {
        sem_wait(&sem_hayIO[IO_SLEEP]);
        sem_wait(&sem_haySolicitudIO[IO_SLEEP]);
        revisarProcesosBloqueadosParaTipoIO(IO_SLEEP);
    }
    return NULL;
}