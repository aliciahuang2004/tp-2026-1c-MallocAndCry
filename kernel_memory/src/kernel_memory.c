#include "kernel_memory.h"
#include "procesos.h"

t_list* lista_ms;
t_list* lista_dir_global_ms;
t_list* cpus_conectadas;
t_list* lista_ms_conexion;
t_list* lista_huecos_libres;
pthread_mutex_t mutex_huecos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cpus_conectadas = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_lista_dir_global_ms = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_lista_ms = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_memoria_total = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_lista_ms_conexion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_socket_swap = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_paths = PTHREAD_MUTEX_INITIALIZER;

t_kernel_memory* iniciar_kernelMemory(char* argv){

    t_kernel_memory* kernelMemory = malloc(sizeof(t_kernel_memory));
    memset(kernelMemory, 0, sizeof(t_kernel_memory));
    t_log* logger_temp= iniciar_logger("kernelMemory.log", "[KERNEL_MEMORY_INIT]", true, LOG_LEVEL_INFO);

    kernelMemory -> config = iniciar_config(logger_temp, argv);

    kernelMemory -> log_level = strdup(config_get_string_value(kernelMemory -> config, "LOG_LEVEL"));
    t_log_level nivel = obtener_log_level(kernelMemory->log_level);

    kernelMemory -> logger = iniciar_logger("kernelMemory.log", "[KERNEL_MEMORY]", true, nivel);

    log_destroy(logger_temp);

    kernelMemory -> puerto_escucha = config_get_string_value(kernelMemory->config, "PUERTO_ESCUCHA");
    kernelMemory -> allocation_strategy = config_get_string_value(kernelMemory->config, "ALLOCATION_STRATEGY");
    kernelMemory -> scripts_basePath = config_get_string_value(kernelMemory->config, "SCRIPTS_BASEPATH");
    kernelMemory -> segment_max_size = config_get_int_value(kernelMemory->config, "SEGMENT_MAX_SIZE");
    kernelMemory -> instruction_delay = config_get_int_value(kernelMemory->config, "INSTRUCTION_DELAY");
    kernelMemory -> compaction_delay = config_get_int_value(kernelMemory->config, "COMPACTION_DELAY");
    kernelMemory->paths_por_pid = dictionary_create();
    
    log_debug(kernelMemory->logger, "Kernel Memory inicializado correctamente");

    //iniciar_tabla_procesos();
    
    return kernelMemory;
    
}

void verificarKernelMemory(t_kernel_memory* kernelMemory){
    log_debug(kernelMemory->logger, "Kernel Memory cargado con los siguientes datos");
    log_debug(kernelMemory->logger, "SEGMENT_MAX_SIZE; %d", kernelMemory -> segment_max_size);
    log_debug(kernelMemory->logger, "ALLOCATION_STRATEGY; %s", kernelMemory -> allocation_strategy);
    log_debug(kernelMemory->logger, "INSTRUCTION_DELAY; %d", kernelMemory -> instruction_delay);
    log_debug(kernelMemory->logger, "COMPACTION_DELAY; %d", kernelMemory -> compaction_delay);
    log_debug(kernelMemory->logger, "SCRIPTS_BASEPATH; %s", kernelMemory -> scripts_basePath);
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void enviar_operacion(int socket_cliente, op_code codigo) {
    send(socket_cliente, &codigo, sizeof(op_code), 0);
}

uint32_t aumentar_memoria_total(uint32_t tamano) {
    pthread_mutex_lock(&mutex_memoria_total);

    uint32_t base = memoria_total;
    memoria_total += tamano;

    pthread_mutex_unlock(&mutex_memoria_total);

    return base;
}
void eliminar_ms_conexion_elemento(void* elemento) {
    if (elemento == NULL) return;
    
    t_ms_conexion* ms_con = (t_ms_conexion*) elemento;
    
    if (ms_con->ip != NULL) {
        free(ms_con->ip);
    }
    if (ms_con->puerto != NULL) {
        free(ms_con->puerto);
    }
    
    free(ms_con);
}
void limpiar_lista_ms_conexiones(void) {
    pthread_mutex_lock(&mutex_lista_ms_conexion);
    
    if (lista_ms_conexion != NULL) {
        list_destroy_and_destroy_elements(lista_ms_conexion, eliminar_ms_conexion_elemento);
        lista_ms_conexion = NULL; 
    }
    
    pthread_mutex_unlock(&mutex_lista_ms_conexion);
}
void destruir_elemento_diccionario(void* elemento) {
    if (elemento != NULL) {
        free(elemento);
    }
}
void destruir_kernel_memory(t_kernel_memory* km) {
    if (!km) return;

    log_debug(km->logger, "Iniciando limpieza absoluta de Kernel Memory por SIGINT...");

    limpiar_lista_ms_conexiones(); 

    pthread_mutex_lock(&mutex_lista_dir_global_ms);
    if (lista_dir_global_ms != NULL) {
        list_destroy_and_destroy_elements(lista_dir_global_ms, free);
        lista_dir_global_ms = NULL;
    }
    pthread_mutex_unlock(&mutex_lista_dir_global_ms);

    pthread_mutex_lock(&mutex_lista_ms);
    if (lista_ms != NULL) {
        list_destroy_and_destroy_elements(lista_ms, free);
        lista_ms = NULL;
    }
    pthread_mutex_unlock(&mutex_lista_ms);

    pthread_mutex_lock(&mutex_cpus_conectadas);
    if (cpus_conectadas != NULL) {
        list_destroy_and_destroy_elements(cpus_conectadas, free);
        cpus_conectadas = NULL;
    }
    pthread_mutex_unlock(&mutex_cpus_conectadas);

    pthread_mutex_lock(&mutex_huecos);
    if (lista_huecos_libres != NULL) {
        list_destroy_and_destroy_elements(lista_huecos_libres, free);
        lista_huecos_libres = NULL;
    }
    pthread_mutex_unlock(&mutex_huecos);

    if (tabla_contextos != NULL) {
        dictionary_destroy_and_destroy_elements(tabla_contextos, free);
        tabla_contextos = NULL;
    }
    if (procesos != NULL) {
        dictionary_destroy_and_destroy_elements(procesos, free);
        procesos = NULL;
    }

    if (km->paths_por_pid != NULL) {
        dictionary_destroy_and_destroy_elements(km->paths_por_pid, free);
    }

    if (km->log_level) {
        free(km->log_level);
    }

    pthread_mutex_destroy(&mutex_huecos);
    pthread_mutex_destroy(&mutex_cpus_conectadas);
    pthread_mutex_destroy(&mutex_lista_dir_global_ms);
    pthread_mutex_destroy(&mutex_lista_ms);
    pthread_mutex_destroy(&mutex_memoria_total);
    pthread_mutex_destroy(&mutex_lista_ms_conexion);

    if (km->config) {
        config_destroy(km->config);
    }
    if (km->logger) {
        log_destroy(km->logger);
    }

    free(km);
    
    printf("[KERNEL MEMORY] Estructuras liberadas exitosamente. Todo limpio.\n");
}