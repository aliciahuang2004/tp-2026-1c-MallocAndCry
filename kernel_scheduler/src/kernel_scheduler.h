#ifndef KERNEL_SCHEDULER_H_
#define KERNEL_SCHEDULER_H_

#include <pthread.h>
#include "../../utils/include/conexion.h"
#include "../../utils/include/protocolo.h"
#include "../../utils/include/utils.h"
#include <commons/collections/queue.h>
#include <pthread.h>
#include <semaphore.h>
#include <commons/collections/dictionary.h>
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

typedef enum {
    IO_SLEEP,
    IO_STDIN,
    IO_STDOUT
} t_tipo_io;

// Estructura administrativa que mantendrá el Kernel por cada interfaz física conectada
typedef struct {
    char* nombre;               // Nombre único de la interfaz (ej: "GENERICA", "TECLADO")
    t_tipo_io tipo;             // Tipo (SLEEP, STDIN, STDOUT)
    int socket_interfaz;        // Socket de red para enviarle las operaciones
    bool ocupada;               // Si la interfaz está ejecutando una tarea actualmente
} t_interfaz_conectada;

// Estructura para solicitud de IO
typedef struct {
    int pid;
    t_pcb* pcb;
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

typedef struct {
    char* nombreMutex;            
    bool bloqueado;
    int pidAsignado;
    t_queue* cola_bloqueados;
    pthread_mutex_t mutex;
} t_mutex;


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
void crearMutex(char* nombreMutex);
void tomarMutex(int pidSolicitaSyscall, char* nombreMutex, int socket_cpu);
void liberarMutex(int pidLiberaMutex,char* nombreMutex);
void hacerSleep(int pid,int tiempo_ms);
void hacerStdOut(int pidSolicitaSyscall,int direccionALeer, int tamanioLectura);
void hacerStdIn(int pidSolicitaSyscall,int direccionAEscribir, int tamanioLectura);
void finalizarProceso(int pid);
void eliminarProceso(int pid, op_code motivo);

//Planificador

extern int pidParaAsignar;

extern t_queue* colaNEW;
extern t_queue* colaREADY;
extern t_queue* colaREADY_SUSP;
extern t_queue* colaEXEC;
extern t_queue* colaBLOCK;
extern t_queue* colaBLOCK_SUSP;
extern t_queue* colaEXIT;
extern t_queue* colaCPUs;

extern pthread_mutex_t mutex_NEW;
extern pthread_mutex_t mutex_READY;
extern pthread_mutex_t mutex_READY_SUSP;
extern pthread_mutex_t mutex_BLOCK;
extern pthread_mutex_t mutex_BLOCK_SUSP;
extern pthread_mutex_t mutex_EXEC;
extern pthread_mutex_t mutex_EXIT;
extern pthread_mutex_t mutex_CPU;
extern t_dictionary* diccionario_mutex;
extern pthread_mutex_t mutex_diccionario;

extern sem_t sem_procesosReady;
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
t_pcb* buscarPcbporPIDEnColaExec(int pid);
void pasarProcesoExecABlock(int pid, t_cpu_conectada* cpu);

//void atender_cpu(int socket_cpu);
t_cpu_conectada* buscarCpuPorSocket(int socket_cpu);
t_pcb* sacardeColaBlockPorPID(int pid);


void* atender_cpu(void* socket_cpu_ptr);
// IO globals (colas por tipo y lista de interfaces)
extern t_list* lista_interfaces_io;
extern t_queue* cola_bloqueados_sleep;
extern t_queue* cola_bloqueados_stdin;
extern t_queue* cola_bloqueados_stdout;
extern pthread_mutex_t mutex_lista_interfaces;
extern pthread_mutex_t mutex_cola_sleep;
extern pthread_mutex_t mutex_cola_stdin;
extern pthread_mutex_t mutex_cola_stdout;

// IO helpers (visibles desde otros módulos)
t_queue* obtener_cola_bloqueados_por_tipo(t_tipo_io tipo);
pthread_mutex_t* obtener_mutex_cola_por_tipo(t_tipo_io tipo);
t_interfaz_conectada* buscar_interfaz_libre_por_tipo(t_tipo_io tipo);
t_interfaz_conectada* buscar_interfaz_por_socket(int socket_interfaz);
t_solicitud_io* crear_solicitud_io(int pid, t_pcb* pcb, t_io_operation tipo_operacion, uint32_t datos_size, void* datos);
void liberar_solicitud_io(t_solicitud_io* solicitud);
void enviar_operacion_a_io(t_interfaz_conectada* interfaz, t_solicitud_io* solicitud);
void imprimir_lista_interfaces_io(t_log* logger);

t_cpu_conectada* buscar_cpu_por_socket(int socket_cpu);
void liberar_cpu_y_notificar(t_cpu_conectada* cpu);
#endif /* KERNEL_SCHEDULER_H*/