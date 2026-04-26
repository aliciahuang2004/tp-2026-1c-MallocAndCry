#include "kernel_scheduler.h"

int pidParaAsignar = 0;
t_queue* colaNEW;
t_queue* colaREADY;

pthread_mutex_t mutex_NEW;
pthread_mutex_t mutex_READY;

sem_t sem_procesosReady;


void iniciarPlanificadorLargoPlazo() {
    colaNEW = queue_create();
    colaREADY = queue_create();

    pthread_mutex_init(&mutex_NEW, NULL);
    pthread_mutex_init(&mutex_READY, NULL);

    sem_init(&sem_procesosReady,0,0);
}

t_pcb* crear_PCB(char* path, int prioridad){
    t_pcb* pcbCreado = malloc(sizeof(t_pcb));
    pcbCreado->pid = pidParaAsignar;
    pcbCreado->prioridad = prioridad;
    pcbCreado->estado = NEW; //NO REQUIERO DE MEMORIA, LO CREO DIRECTAMENTE
    pcbCreado->path = path;
    pidParaAsignar ++;
    return pcbCreado;
}

void crearProceso(t_kernel_scheduler* ks ,char* path, int prioridad){
    t_pcb* pcbNuevo = crear_PCB(path, prioridad);
    
    enviarPathYPidKM(pcbNuevo->pid,path,ks);
    //AGREGAR A COLA NEW, es realmente necesario?
    pthread_mutex_lock(&mutex_NEW);
    queue_push(colaNEW, pcbNuevo);
    pthread_mutex_unlock(&mutex_NEW);

    pasarProcesoAReady();
}


void pasarProcesoAReady(){
    pthread_mutex_lock(&mutex_NEW);
    if (!queue_is_empty(colaNEW)){
        t_pcb* pcb = queue_pop(colaNEW);
        pthread_mutex_unlock(&mutex_NEW);

        pcb->estado = READY;
        
        pthread_mutex_lock(&mutex_READY);
        queue_push(colaREADY, pcb);
        pthread_mutex_unlock(&mutex_READY);
        //PLANIFICADOR CORTO PLAZO SEÑAL
    }else{
        pthread_mutex_unlock(&mutex_NEW);
    }

}

void enviarPathYPidKM(int pid, char* path, t_kernel_scheduler* ks){
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(CREACION_DE_PROCESO, buffer); 

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, path, strlen(path) + 1);

    int resultado = enviar_paquete(paquete, ks->socket_kernel_memory, ks->logger);

    if (resultado != 0) {
        log_error(ks->logger, "Error al enviar el Path a Kernel Memory");
        exit(EXIT_FAILURE);
    }
    eliminar_paquete(paquete);
}
