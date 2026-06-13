#include "kernel_scheduler.h"
#include <unistd.h>

int pidParaAsignar = 0;

t_dictionary* diccionario_mutex;
pthread_mutex_t mutex_diccionario;

t_queue* colaNEW;
t_queue* colaREADY;
t_queue** colasREADY_multinivel= NULL;
t_queue* colaREADY_SUSP;
t_queue* colaEXEC;
t_queue* colaBLOCK;
t_queue* colaBLOCK_SUSP;
t_queue* colaEXIT;
t_queue* colaCPUs;

pthread_mutex_t mutex_NEW;
pthread_mutex_t mutex_READY;
pthread_mutex_t* mutexColas = NULL;
pthread_mutex_t mutex_READY_SUSP;
pthread_mutex_t mutex_BLOCK;
pthread_mutex_t mutex_BLOCK_SUSP;
pthread_mutex_t mutex_EXEC;
pthread_mutex_t mutex_EXIT;
pthread_mutex_t mutex_CPU;

t_algoritmo_cola* algoritmos_por_cola = NULL;
sem_t sem_procesosReady;
//sem_t sem_hayProcesos[]; no se usa
sem_t sem_hayCPUs;

t_kernel_scheduler* kernel;
void iniciarPlanificadorLargoPlazo(){
    colaNEW = queue_create();
    
    colaREADY = queue_create();
    pthread_mutex_init(&mutex_READY, NULL);

    /*if (strcmp(kernel->planification_algorithm, "CMN") == 0)
    {
        for (int i = 0; kernel->cantidadColasMultinivel; i++){
            colasREADY[i]= queue_create();
            pthread_mutex_init (&mutexColas[kernel->cantidadColasMultinivel], NULL);
            sem_init(&sem_hayProcesos[i],0,0);
        }
    }*/
    pthread_mutex_init(&mutex_NEW, NULL);
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

    if (strcmp(kernel->planification_algorithm, "CMN") == 0) {
        int n = kernel->cantidadColasMultinivel;
        colasREADY_multinivel = malloc(sizeof(t_queue*) * n);
        mutexColas = malloc(sizeof(pthread_mutex_t) * n);
        algoritmos_por_cola = malloc(sizeof(t_algoritmo_cola) * n);

        for (int i = 0; i < n; i++) {
            colasREADY_multinivel[i] = queue_create();
            pthread_mutex_init(&mutexColas[i], NULL);
            algoritmos_por_cola[i] = parsear_algoritmo(kernel->queues_algorithms[i]);

        }


    }
}
t_algoritmo_cola parsear_algoritmo(char* algoritmo) {
    if (strcasecmp(algoritmo, "RR") == 0) return ALGORITMO_RR;
    if (strcasecmp(algoritmo, "FIFO") == 0) return ALGORITMO_FIFO;
    
    // Si viene un valor desconocido, loggear y defaultear a FIFO
    log_warning(kernel->logger, "Algoritmo desconocido: %s, usando FIFO por defecto", algoritmo);
    return ALGORITMO_FIFO;
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

        encolarProcesoEnReady(pcb);

       /* if (strcmp(kernel->planification_algorithm, "FIFO") == 0 || strcmp(kernel->planification_algorithm, "RR") == 0){
            pthread_mutex_lock(&mutex_READY);
            queue_push(colaREADY, pcb);
            pthread_mutex_unlock(&mutex_READY);
            //sem_post(&sem_procesosReady);
        }
        if (strcmp(kernel->planification_algorithm, "CMN") == 0){
            pthread_mutex_lock(&mutexColas[pcb->prioridad]);
            queue_push(colasREADY_multinivel[pcb->prioridad], pcb);
            pthread_mutex_unlock(&mutexColas[pcb->prioridad]);
            //sem_post(&sem_procesosReady);
        }*/
        
        log_info(kernel->logger,"## (<%d>) Pasa del estado <NEW> al estado <READY>",pcb->pid);
        //sem_post(&sem_procesosReady);
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
        //log_info(kernel->logger,"EJECUCION POR FIFO");
        ejecutarPorFIFO();
    }
    if (strcmp(kernel->planification_algorithm, "RR") == 0){
        //log_info(kernel->logger,"EJECUCION POR RR");
        ejecutarPorRR();
    }

    if(strcmp(kernel->planification_algorithm, "CMN") == 0){
        //log_info(kernel->logger,"EJECUCION COLAS MULTINIVEL");
        ejecutarPorCMN();
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

void ejecutarPorCMN(){
    for (int nivel = 0; nivel < kernel->cantidadColasMultinivel; nivel++) {
        pthread_mutex_lock(&mutexColas[nivel]);
        if (queue_is_empty(colasREADY_multinivel[nivel])) {
            pthread_mutex_unlock(&mutexColas[nivel]);
            continue;
        }

        t_pcb* pcb = queue_pop(colasREADY_multinivel[nivel]);
        pthread_mutex_unlock(&mutexColas[nivel]);

        pthread_mutex_lock(&mutex_READY);
        queue_push(colaREADY, pcb);
        pthread_mutex_unlock(&mutex_READY);

        log_debug(kernel->logger,"## (<%d>) Seleccionado de Cola %d (%s)", pcb->pid, nivel, kernel->queues_algorithms[nivel]);

        if (algoritmos_por_cola[nivel] == ALGORITMO_RR) {
            ejecutarPorRR();
        } else {
            ejecutarPorFIFO();
        }
        return;
    }
}
void encolarProcesoEnReady(t_pcb* pcb){
    pcb->estado = READY;

    if (strcmp(kernel->planification_algorithm, "CMN") == 0) {
        int indice = pcb->prioridad;
        if (indice < 0) indice = 0;
        if (indice >= kernel->cantidadColasMultinivel) indice = kernel->cantidadColasMultinivel - 1;

        pthread_mutex_lock(&mutexColas[indice]);
        queue_push(colasREADY_multinivel[indice], pcb);
        pthread_mutex_unlock(&mutexColas[indice]);

        sem_post(&sem_procesosReady);
        verificarDesalojoPorPrioridad(pcb, indice);
    } else {
        pthread_mutex_lock(&mutex_READY);
        queue_push(colaREADY, pcb);
        pthread_mutex_unlock(&mutex_READY);

        sem_post(&sem_procesosReady);
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
void pedirDesalojoPorPrioridad(int pid, t_cpu_conectada* cpu){
    if (cpu == NULL) return;

    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(PROCESO_DESALOJADO_PRIORIDAD, buffer);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    enviar_paquete(paquete, cpu->socket_cliente, kernel->logger);
    eliminar_paquete(paquete);
}
void verificarDesalojoPorPrioridad(t_pcb* pcbNuevo, int indiceColaNueva){ //indiceColaNueva es el número de cola del proceso nuevo, se usa para comparar contra los que están ejecutando
    if (!kernel->queue_preemption) return;
    t_pcb* pcbVictima = NULL;
    t_queue* colaAux = queue_create();

    pthread_mutex_lock(&mutex_EXEC);
    int cantidad = queue_size(colaEXEC);
    for (int i = 0; i < cantidad; i++){
        t_pcb* pcb = queue_pop(colaEXEC);
        if (pcb->prioridad > indiceColaNueva && (pcbVictima == NULL || pcb->prioridad > pcbVictima->prioridad)) {
            if (pcbVictima != NULL) queue_push(colaAux, pcbVictima);
            pcbVictima = pcb;
        } else {
            queue_push(colaAux, pcb);
        }
    }
    while (!queue_is_empty(colaAux)) {
        queue_push(colaEXEC, queue_pop(colaAux));
    }
    queue_destroy(colaAux);
    if (pcbVictima != NULL) {
        queue_push(colaEXEC, pcbVictima); // sigue en EXEC hasta que la CPU confirme el desalojo
    }
    pthread_mutex_unlock(&mutex_EXEC);

    if (pcbVictima == NULL) return;

    t_cpu_conectada* cpuVictima = buscar_cpu_por_pid(pcbVictima->pid);
    if (cpuVictima == NULL) return;

    log_info(kernel->logger, "## (<%d>) - Desalojado por prioridad por llegada de (<%d>) a la cola %d", pcbVictima->pid, pcbNuevo->pid, indiceColaNueva);
    pedirDesalojoPorPrioridad(pcbVictima->pid, cpuVictima);
}
void pasarProcesoExecAReady(int pid, t_cpu_conectada* cpu){
    // buscar pid del pcb en cola exec
    // sacarlo de cola exec
    // agregarlo a ready
    t_pcb* pcb = buscarPcbporPIDEnColaExec(pid);

    if(pcb != NULL){
        encolarProcesoEnReady(pcb);
        log_info(kernel->logger,"## (<%d>) Pasa del estado <EXEC> al estado <READY>",pcb->pid);
    }
    //LIBERAR CPU
    cpu->libre = true;
    cpu->pidEjecutando = -1;

    //sem_post(&sem_procesosReady);
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

t_cpu_conectada* buscar_cpu_por_pid(int pid) {
    t_cpu_conectada* encontrada = NULL;
    t_queue* colaAux = queue_create();

    pthread_mutex_lock(&mutex_CPU);
    int cantidad = queue_size(colaCPUs);

    for (int i = 0; i < cantidad; i++) {
        t_cpu_conectada* cpu = queue_pop(colaCPUs);
        if (cpu->pidEjecutando == pid && encontrada == NULL) {
            encontrada = cpu;
        }
        queue_push(colaAux, cpu);
    }

    while (!queue_is_empty(colaAux)) {
        queue_push(colaCPUs, queue_pop(colaAux));
    }
    queue_destroy(colaAux);
    pthread_mutex_unlock(&mutex_CPU);

    return encontrada;
}

void pedirDesalojoPorCompactacion(){

    //para cada cpu ocupada
    //obtener cpu o los datos por separado: pid y cpuSocket
        t_buffer* buffer = crear_buffer();
        t_paquete* paquete = crear_paquete(PROCESO_DESALOJADO_COMPACTACION, buffer);
        // agregar_a_paquete(paquete, &pid, sizeof(int));

        // enviar_paquete(paquete, cpu->socket_cliente, kernel->logger);

        eliminar_paquete(paquete);
    
}