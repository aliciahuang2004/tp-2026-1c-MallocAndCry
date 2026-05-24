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
    t_mutex* nuevoMutex = malloc(sizeof(t_mutex));
                nuevoMutex->nombreMutex = nombreMutex;
                nuevoMutex->bloqueado = false;
                nuevoMutex->pidAsignado = -1;
                pthread_mutex_init(&nuevoMutex->mutex,NULL);

                //AGREGARLO PARA MANEJAR: COLAS? LISTAS?

                log_info(kernel->logger, "## Mutex %s creado correctamente", nuevoMutex->nombreMutex);
}

void tomarMutex(int pidSolicitaSyscall,char* nombreMutex){
    //BUSCAR MUTEX SEGUN NOMBRE
    t_mutex* mutex;
    if(!mutex->bloqueado){
        mutex->bloqueado = true;
        mutex->pidAsignado = pidSolicitaSyscall;
        pthread_mutex_lock(&mutex->mutex);
        log_info(kernel->logger,"## (<%d>) Toma el Mutex <%s>",pidSolicitaSyscall,mutex->nombreMutex);

    }else{
                    
        //BLOQUEAR ESTE PROCESO
        //pasarProcesoReadyABlock();

    }
}

void liberarMutex(int pidLiberaMutex,char* nombreMutex){
    //BUSCAR MUTEX SEGUN NOMBRE
    t_mutex* mutex;

    if(mutex->pidAsignado == pidLiberaMutex && mutex->bloqueado ){
        mutex->bloqueado = false;
        mutex->pidAsignado = -1;
        pthread_mutex_unlock(&mutex->mutex);
        log_info(kernel->logger,"## (<%d>) Libera el Mutex <%s>",pidLiberaMutex,mutex->nombreMutex);
        //DESBLOQUEAR PROCESO QUE QUIERA ESTE MUTEX
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