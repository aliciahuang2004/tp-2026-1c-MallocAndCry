#ifndef KERNEL_SCHEDULER_H_
#define KERNEL_SCHEDULER_H_

#include "../../utils/include/conexion.h"
#include "../../utils/include/protocolo.h"
#include "../../utils/include/utils.h"
#include <commons/collections/queue.h>
#include <pthread.h>
#include <semaphore.h>
#include <commons/collections/dictionary.h>
#include <unistd.h>
#define COLOR_VERDE "\033[32m"

typedef enum{
    FIFO,
    RR,
    CMN
} t_planificador;
typedef enum{
    NULO,
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
    int prioridad; // prioridad que usa el planificador
    int prioridadBase; //prioridad original del proceso, nunca se modificara
    char* path;
    t_estado estado;
    int socketCPUEjecuta;
    bool ejecutaPorRR;
    t_list* mutexTomados; // Lista de mutex que el proceso tiene tomado ahora mismo
    bool suspensionEnCurso;        
    bool ioCompletadaEnTransito;
} t_pcb;

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
    char** queues_algorithms;
    int rr_quantum;
    bool queues_preemption;
    int suspension_time;
    // bool procesoInicialCreado;
    int cantidadColasMultinivel;
    bool noHayCompactacion;
    bool noHayCorrupcion;
    int pcbFinalizados;
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
    t_pcb* pcbEjecutando;
}t_cpu_conectada;

typedef struct {
    char* nombreMutex;            
    bool bloqueado;
    int pidAsignado;
    t_queue* cola_bloqueados;
    pthread_mutex_t mutex;
} t_mutex;

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
    bool ocupada;   
    int pidAsignado;
    t_queue* solicitudes;
} t_interfaz_conectada;

typedef struct {
    int pidSolicitaSyscall;
    t_tipo_io tipo;
    uint32_t tamanio;
    uint32_t direccion;
    int tiempoSleep;
    char* leido;
} t_solicitud_io;

// extern char* pathInicial;
extern int pidParaAsignar;
extern int idCPUParaAsignar;
extern t_kernel_scheduler* kernel;

extern t_interfaz_conectada interfaces[3];
extern pthread_mutex_t mutex_interfaces[3];

extern t_queue* colaNEW;
extern t_queue** colasREADY;
extern t_queue* colaREADY_SUSP;
extern t_queue* colaEXEC;
extern t_queue* colaBLOCK;
extern t_queue* colaBLOCK_SUSP;
extern t_queue* colaEXIT;

extern t_queue* colaCPUs;

extern pthread_mutex_t mutex_NEW;
extern pthread_mutex_t* mutex_READY;
extern pthread_mutex_t mutex_READY_SUSP;
extern pthread_mutex_t mutex_EXEC;
extern pthread_mutex_t mutex_BLOCK;
extern pthread_mutex_t mutex_BLOCK_SUSP;
extern pthread_mutex_t mutex_EXIT;

extern pthread_mutex_t mutex_CPU;

extern sem_t sem_hayProcesosEnReady;
extern sem_t sem_readyPrioridad;
extern sem_t sem_hayCPUdisponible;
// extern sem_t sem_finSyscall;
// extern sem_t sem_hayMemoria;
extern sem_t* sem_hayIO;
extern sem_t* sem_haySolicitudIO;
extern sem_t sem_recibiLecuraDeIO;
extern sem_t sem_recibiLecuraDeKM;
extern sem_t sem_recibiEscrituraDeKM;
// extern sem_procesoCreado;
extern sem_t sem_suspension_ok;
extern sem_t sem_procesoFinalizado;
// extern sem_t sem_hayProcesosEnExec;
extern sem_t sem_procesoDesalojadoPrioridad;

extern t_dictionary* diccionario_mutex;
extern pthread_mutex_t mutex_diccionario;

// extern t_list* lista_interfaces_io;
// extern pthread_mutex_t mutex_lista_interfaces;
extern sem_t sem_compactacionTerminada;

// kernel_scheduler
t_kernel_scheduler* iniciar_kernel_scheduler(char* path_config);
void conectar_con_kernel_memory();
void esperar_conexiones();
void* atender_cliente_scheduler(void* arg);
void inicializar_interfaces();
extern pthread_mutex_t mutex_socket_KM; //Necesitamos un mutex dedicado que serialice todos los envios al socket de KM
int enviarPaqueteAKM(t_paquete* paquete);
// void imprimir_lista_interfaces_io(t_log* logger);
void destruir_kernel_scheduler(t_kernel_scheduler* kernel_scheduler);
void liberarConexiones();
void finalizarColas();
void finalizarSemaforos();
void finalizarInterfaces();

// atencionKM
void* atender_kernel_memory(void* arg);
void pedirDesalojoPorCompactacion();
void chequearCPUsDesalojadas();
void solicitarDesuspenderProceso(int pid);
void finalizarTodosLosProcesos();
void pedirDesalojoPorCorrupcion(); // SE FINALIZA LOS EXEC
void finalizarProcesosBlock();
void finalizarProcesosBlockSusp();
void finalizarProcesosNew();
void finalizarProcesosReady();
void finalizarProcesosReadySusp();

// atencionCPU
void* atender_cpu(void* arg);
void quitarCPU(t_cpu_conectada* cpuDesconectada);

// atencionIO
void* atender_io(void* arg);
int finalizoConexionIO(t_tipo_io tipo);
void liberarIO(t_tipo_io tipo);
void revisarProcesosBloqueadosParaTipoIO(t_tipo_io tipo);
t_solicitud_io* retirarSolicitud(t_tipo_io tipo, int pid);

// planificador
void inicializarColas();
void inicializarSemaforos();
void pasarProcesoNewAReady();
t_planificador obtenerPlanificacion(char* planificador);
void pasarProcesoReadyAExec();
t_pcb* elegirPorFIFO();
t_pcb* elegirPorRR();
t_pcb* elegirPorCMN();
int colaDeProcesoEjecutaRR(int prioridad);
t_cpu_conectada* elegirCPU();
t_cpu_conectada* buscarCPULibre();
void enviarPIDAcpu(int pid, t_cpu_conectada* cpu);
void* iniciarTemporizadorRR(void* arg);
void notificarDesalojo(t_cpu_conectada* cpu, t_pcb* pcbOtroProceso, op_code motivo);
void* monitorPrioridades(void* arg);
t_pcb* buscarMenorPrioridad();
int procesoMasPrioritario(int prioridadActual);
t_pcb* buscarPCBPorPID(int pid, t_queue* cola, pthread_mutex_t* mutex);
t_pcb* buscarPCBenReadyConPrioridad(int prioridad);
void pasarProcesoExecAReady(int pid);
void liberarCPU(t_cpu_conectada* cpu);
void pasarProcesoExecABlock(int pid);
void* iniciarTemporizadorSuspendido(void* arg);
void pasarProcesoBlockaReady(int pid);
void pasarProcesoBlockABlockSusp(int pid);
void pasarProcesoBlockSuspAReadySusp(int pid);
void pasarProcesoReadySuspAReady(int pid);

void pasarProcesoExecAExit(); // finaliza por DESCONEXION_CPU
void pasarProcesoReadyAExit();  //finaliza por CORRUPCION
void pasarProcesoBlockAExit(); // finaliza por DESCONEXION_IO

t_cpu_conectada* buscarCPUSegunPID(int pid);
t_cpu_conectada* buscar_cpu_por_socket(int socket_cpu);
void* loop_corto_plazo(void* args);
t_tipo_io buscarTipoIOPorSocket(int socket_io);
void reencolarAlInicio(int pid);
void inicializarHilos();
void* atencionIOsleep(void* args);
void* atencionIOstdIN(void* args);
void* atencionIOstdOUT(void* args);
void ordenarSuspReadySegunPrioridad();
void* finalizarKernelScheduler(void* args);
t_pcb* retiraSegunPID(int pid, t_queue* cola, pthread_mutex_t mutex);
t_pcb* retiraSegunPIDdeREADY(int pid);

// syscall
t_pcb* crear_PCB(char* path, int prioridad);
void crearProceso(char* path, int prioridad);
void enviarPathYPidKM(int pid, char* path);
void crearMutex(char* nombreMutex);
void tomarMutex(int pidSolicitaSyscall, char* nombreMutex, t_cpu_conectada* cpu);
void liberarMutex(int pidLiberaMutex, char* nombreMutex, t_cpu_conectada* cpu);
void asignarMemoria(int pidSolicitaSyscall, int idSegmento, int tamanio);
void liberarMemoria(int pidSolicitaSyscall, int idSegmento);
t_pcb* buscarPCBEnCualquierEstado(int pid);
void recalcularPrioridad(t_pcb* pcb);
void manejar_sleep(int pid, int tiempo_ms, t_cpu_conectada* cpu);
void manejar_stdin(int pid, uint32_t dir_logica, uint32_t tamano, t_cpu_conectada* cpu);
void manejar_stdout(int pid, uint32_t dir_logica, uint32_t tamano, t_cpu_conectada* cpu);
void finalizarProceso(int pid, op_code motivo);
void eliminarProceso(int pid, op_code motivo);
void enviarAIO(int socket_io, t_solicitud_io* solicitud);
void enviarAKMSolicitudIO(t_solicitud_io* solicitud);
void hacerSTDIN(t_solicitud_io* solicitud, int socket_io);
void hacerSTDOUT(t_solicitud_io* solicitud, int socket_io);


#endif /* KERNEL_SCHEDULER_H*/