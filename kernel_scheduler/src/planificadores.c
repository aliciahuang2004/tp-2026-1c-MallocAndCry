#include "kernel_scheduler.h"

int pidParaAsignar = 0;

t_queue* colaNEW;
t_queue* colaREADY;
t_queue* colaEXEC;
t_queue* colaCPUs;

pthread_mutex_t mutex_NEW;
pthread_mutex_t mutex_READY;
pthread_mutex_t mutex_BLOCK;
pthread_mutex_t mutex_EXEC;
pthread_mutex_t mutex_CPU;

sem_t sem_procesosReady;
sem_t sem_procesosExec;

char* planificador;
t_kernel_scheduler* kernel;

void iniciarPlanificadorLargoPlazo(t_kernel_scheduler* kernel){
    colaNEW = queue_create();
    colaREADY = queue_create();

    pthread_mutex_init(&mutex_EXEC, NULL);

    sem_init(&sem_procesosReady,0,0);
}


void iniciarPlanificadorLCortoPlazo(t_kernel_scheduler* kernel){
    //colaREADY inicia con planificador largo
    colaEXEC = queue_create();

    pthread_mutex_init(&mutex_NEW, NULL);
    pthread_mutex_init(&mutex_READY, NULL);

    planificador = kernel->planification_algorithm;

    sem_init(&sem_procesosExec,0,0);
}

void iniciarCPU(){
    colaCPUs = queue_create();

    pthread_mutex_init(&mutex_CPU,NULL);
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
        sem_post(&sem_procesosReady);
    }else{
        pthread_mutex_unlock(&mutex_NEW);
    }
}

void pasarProcesoReadyAExec(){

    t_pcb* pcb;

    pthread_mutex_lock(&mutex_READY);
    if (!queue_is_empty(colaREADY)){
        
        if (planificador == "FIFO"){
            pcb = queue_pop(colaREADY);
            pthread_mutex_unlock(&mutex_READY);

        }else if (planificador == "RR"){
            //TO DO
            // ELEGIR PCB A SACAR
            // ENVIAR PCB A CPU
            // CORRER TIEMPO PARA DESALOJO
            printf("RR");
        }
    }else{
        pthread_mutex_unlock(&mutex_READY);
    }
    enviarPIDAcpu(pcb->pid, kernel);
    // agregar a cola exec
}

void enviarPIDAcpu(int pid, t_kernel_scheduler* ks){
    
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(PROCESO_A_PROCESAR, buffer);

    agregar_a_paquete(paquete, &pid, sizeof(int));

    t_cpu_conectada* cpu = elegirCPU();

    enviar_paquete(paquete, cpu->socket_cliente, ks->logger);

    eliminar_paquete(paquete);
}

t_cpu_conectada* elegirCPU(){

    int cantidad = queue_size(colaCPUs);
    t_cpu_conectada* cpu_elegida = malloc(sizeof(t_cpu_conectada));

    t_queue* colaAux = queue_create();

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

    cpu_elegida -> libre = false;
    queue_push(colaCPUs,cpu_elegida);

    return cpu_elegida;
}

bool hayCpuLibre(){

    return 1;
}