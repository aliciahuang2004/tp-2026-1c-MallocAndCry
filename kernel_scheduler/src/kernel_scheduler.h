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

extern char* pathInicial;
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
    char* planification_algorithm;
    int rr_quantum;
    bool procesoInicialCreado;
}t_kernel_scheduler;

// Estructura para pasar datos a los hilos de atención
typedef struct {
    int socket_cliente;
    t_log* logger;
}t_atencion_cliente;

typedef struct {
    int socket_cliente;
    int id_cpu;
    bool libre;
    int pidEjecutando;
}t_cpu_conectada;

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

// funciones de IO
void enviar_peticion_io(int socket_io, t_solicitud_io* solicitud, t_log* logger);
void* esperar_finalizacion_io(void* args);

//SYSCALL
//Crear proceso
t_pcb* crear_PCB(char* path, int prioridad);
void crearProceso(char* path, int prioridad);
void enviarPathYPidKM(int pid, char* path);

//Planificador

extern int pidParaAsignar;

extern t_queue* colaNEW;
extern t_queue* colaREADY;
extern t_queue* colaEXEC;
extern t_queue* colaCPUs;

extern pthread_mutex_t mutex_NEW;
extern pthread_mutex_t mutex_READY;
extern pthread_mutex_t mutex_BLOCK;
extern pthread_mutex_t mutex_EXEC;
extern pthread_mutex_t  mutex_CPU;

extern sem_t sem_procesosReady;
extern sem_t sem_procesosExec;
extern sem_t sem_hayCPUs;

extern int cpu_socket;

extern t_kernel_scheduler* kernel;

void iniciarPlanificadorLargoPlazo();
void iniciarPlanificadorLCortoPlazo();
void iniciarCPU();
void pasarProcesoNewAReady();
void pasarProcesoReadyAExec();
void enviarPIDAcpu(int pid, t_cpu_conectada* cpu);
t_cpu_conectada* elegirCPULibre();
void* loop_corto_plazo(void* args);
void ejecutarPorFIFO();
void ejecutarPorRR();
void pedirDesalojoPorFinDeQuantum(int pid, t_cpu_conectada* cpu);
void pasarProcesoExecAReady(int pid, t_cpu_conectada* cpu);
t_pcb* buscarPcbporPID(int pid);
void atender_cpu(int socket_cpu);
#endif /* KERNEL_SCHEDULER_H*/