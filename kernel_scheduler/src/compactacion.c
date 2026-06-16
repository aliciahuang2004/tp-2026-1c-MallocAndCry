#include "kernel_scheduler.h"

void encolarProcesoEnReadyAlPrincipio(t_pcb* pcb){
    pcb->estado = READY;

    if (strcmp(kernel->planification_algorithm, "CMN") == 0) {
        int indice = pcb->prioridad;
        if (indice < 0) indice = 0;
        if (indice >= kernel->cantidadColasMultinivel) indice = kernel->cantidadColasMultinivel - 1;

        pthread_mutex_lock(&mutexColas[indice]);
        t_queue* colaAux = queue_create();
        queue_push(colaAux, pcb);
        while(!queue_is_empty(colasREADY_multinivel[indice])){
            queue_push(colaAux, queue_pop(colasREADY_multinivel[indice]));
        }
        while(!queue_is_empty(colaAux)){
            queue_push(colasREADY_multinivel[indice], queue_pop(colaAux));
        }
        queue_destroy(colaAux);
        pthread_mutex_unlock(&mutexColas[indice]);
    } else {
        pthread_mutex_lock(&mutex_READY);
        t_queue* colaAux = queue_create();
        queue_push(colaAux, pcb);
        while(!queue_is_empty(colaREADY)){
            queue_push(colaAux, queue_pop(colaREADY));
        }
        while(!queue_is_empty(colaAux)){
            queue_push(colaREADY, queue_pop(colaAux));
        }
        queue_destroy(colaAux);
        pthread_mutex_unlock(&mutex_READY);
    }

    sem_post(&sem_procesosReady);
    log_info(kernel->logger, "## (<%d>) Pasa del estado <EXEC> al estado <READY> (al principio, por compactación)", pcb->pid);
}
void pasarProcesoExecAReadyAlFrente(int pid, t_cpu_conectada* cpu){
    t_pcb* pcb = buscarPcbporPIDEnColaExec(pid);

    if(pcb != NULL){
        encolarProcesoEnReadyAlPrincipio(pcb);
    }

    cpu->libre = true;
    cpu->pidEjecutando = -1;
    sem_post(&sem_hayCPUs);
}

void desalojarTodasLasCPUsPorCompactacion(){
    // Obtener lista de CPUs con procesos en EXEC
    t_list* cpus_a_desalojar = list_create();
    
    pthread_mutex_lock(&mutex_EXEC);
    int cantidad = queue_size(colaEXEC);
    t_queue* colaAux = queue_create();
    for(int i = 0; i < cantidad; i++){
        t_pcb* pcb = queue_pop(colaEXEC);
        t_cpu_conectada* cpu = buscar_cpu_por_pid(pcb->pid);
        if(cpu != NULL){
            list_add(cpus_a_desalojar, cpu);
        }
        queue_push(colaAux, pcb);
    }
    while(!queue_is_empty(colaAux)){
        queue_push(colaEXEC, queue_pop(colaAux));
    }
    queue_destroy(colaAux);
    pthread_mutex_unlock(&mutex_EXEC);

    // Mandar pedido de desalojo a todas las CPUs en paralelo
    for(int i = 0; i < list_size(cpus_a_desalojar); i++){
        t_cpu_conectada* cpu = list_get(cpus_a_desalojar, i);
        t_buffer* buffer = crear_buffer();
        t_paquete* paquete = crear_paquete(PROCESO_DESALOJADO_COMPACTACION, buffer);
        agregar_a_paquete(paquete, &cpu->pidEjecutando, sizeof(int));
        enviar_paquete(paquete, cpu->socket_cliente, kernel->logger);
        eliminar_paquete(paquete);
    }

    // Esperar confirmación de cada CPU y mover proceso al frente de READY
    for(int i = 0; i < list_size(cpus_a_desalojar); i++){
        t_cpu_conectada* cpu = list_get(cpus_a_desalojar, i);
        
        t_list* paquete_motivo = recibir_paquete(cpu->socket_cliente);
        if(paquete_motivo == NULL){
            log_error(kernel->logger, "CPU socket %d se desconectó durante compactación", cpu->socket_cliente);
            continue;
        }
        int motivo = *(int*)list_get(paquete_motivo, 0);
        list_destroy_and_destroy_elements(paquete_motivo, free);
        
        if(motivo == PROCESO_DESALOJADO_COMPACTACION){
            pasarProcesoExecAReadyAlFrente(cpu->pidEjecutando, cpu);
            log_info(kernel->logger, "## (%d) Pasa del estado EXEC al estado READY (compactación)", cpu->pidEjecutando);
        }
    }

    list_destroy(cpus_a_desalojar);
}