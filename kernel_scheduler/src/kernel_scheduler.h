#ifndef KERNEL_SCHEDULER_H_
#define KERNEL_SCHEDULER_H_

#include <pthread.h>
#include "../../utils/include/conexion.h"
#include "../../utils/include/protocolo.h"
#include "../../utils/include/utils.h"
#include <commons/collections/queue.h>
#include <pthread.h>
#include <semaphore.h>
#define COLOR_VERDE "\033[32m"

typedef enum{
    NEW,
    READY,
    READY_SUSP,
    EXEC,
    BLOCK,
    BLOCK_SUSP,
    EXIT
} t_estado;

typedef struct{
    int pid;
    int prioridad;
    t_estado estado;
    char* path;
} t_pcb;

// Estructura para solicitud de IO
typedef struct {
    int pid;
    t_io_operation tipo_operacion;
    uint32_t datos_size;
    void* datos;
} t_solicitud_io;

// Estructura para manejo de hilos que esperan finalización de IO
typedef struct {
    int socket_io;
    int pid;
    t_pcb* pcb;
    t_log* logger;
} t_io_waiting_thread;

typedef struct {
    t_log* logger;
    t_config* config;
    char* log_level;   
    char* puerto_escucha;
    char* ip_kernel_memory;
    char* puerto_kernel_memory;
    int socket_kernel_memory;
    int socket_io;  // Socket para comunicarse con IO
}t_kernel_scheduler;

// Estructura para pasar datos a los hilos de atención
typedef struct {
    int socket_cliente;
    t_log* logger;
}t_atencion_cliente;

// funciones de inicializacion
t_kernel_scheduler* iniciar_kernel_scheduler(char* path_config);
void verificar_kernel_scheduler(t_kernel_scheduler* kernel_scheduler);
void destruir_kernel_scheduler(t_kernel_scheduler* kernel_scheduler);

// funciones de servidor
void esperar_conexiones(t_kernel_scheduler* kernel_scheduler);
void* atender_cliente_scheduler(void* arg);
int recibir_operacion(int socket_cliente);
//funciones de cliente
void conectar_con_kernel_memory(t_kernel_scheduler* kernel_scheduler);

void iniciarPlanificadorLargoPlazo();
t_pcb* crear_PCB(char* path, int prioridad);
void crearProceso(t_kernel_scheduler* ks, char* path, int prioridad);
void pasarProcesoAReady();

// funciones de IO
void enviar_peticion_io(int socket_io, t_solicitud_io* solicitud, t_log* logger);
void* esperar_finalizacion_io(void* args);

void enviarPathKM(char* path, t_kernel_scheduler* ks);

void enviarPathYPidKM(int pid, char* path, t_kernel_scheduler* ks);
#endif /* KERNEL_SCHEDULER_H*/
