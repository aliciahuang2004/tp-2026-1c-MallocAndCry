#include "kernel_scheduler.h"

int idCPUParaAsignar = 0;

t_kernel_scheduler* iniciar_kernel_scheduler(char* path_config) {
    t_kernel_scheduler* kernel_scheduler = malloc(sizeof(t_kernel_scheduler));

    t_config* tmp_config = config_create(path_config); // ks.config
    if (!tmp_config) return NULL; 

    kernel_scheduler->log_level = config_get_string_value(tmp_config, "LOG_LEVEL");
    kernel_scheduler->logger = iniciar_logger("kernel_scheduler.log", "KERNEL_SCHEDULER", true, obtener_log_level(kernel_scheduler->log_level));
    config_destroy(tmp_config);
    
    kernel_scheduler->config = iniciar_config(kernel_scheduler->logger, path_config);
    
    kernel_scheduler->puerto_escucha = config_get_string_value(kernel_scheduler->config, "PUERTO_ESCUCHA");
    kernel_scheduler->ip_kernel_memory = config_get_string_value(kernel_scheduler->config, "IP_KERNEL_MEMORY");
    kernel_scheduler->puerto_kernel_memory = config_get_string_value(kernel_scheduler->config, "PUERTO_KERNEL_MEMORY");
    kernel_scheduler->planification_algorithm = config_get_string_value(kernel_scheduler->config, "PLANIFICATION_ALGORITHM");
    kernel_scheduler->queues_algorithms = config_get_array_value(kernel_scheduler->config, "QUEUES_ALGORITHMS");
    kernel_scheduler->rr_quantum = config_get_int_value(kernel_scheduler->config,"RR_QUANTUM");
    kernel_scheduler->queues_preemption = config_get_string_value(kernel_scheduler->config, "QUEUE_PREEMPTION");
    kernel_scheduler->suspension_time = config_get_int_value(kernel_scheduler->config,"SUSPENSION_TIMEOUT");
    
    //kernel_scheduler->procesoInicialCreado = false; 
    kernel_scheduler->cantidadColasMultinivel = 0;   
    while(kernel_scheduler->queues_algorithms[kernel_scheduler->cantidadColasMultinivel] != NULL) {
        kernel_scheduler->cantidadColasMultinivel++;
    }

    log_debug(kernel_scheduler->logger, "El kernel scheduler se inicializo correctamente");
    
    return kernel_scheduler;
}

void conectar_con_kernel_memory(){

    kernel->socket_kernel_memory = crear_conexion(kernel->logger,kernel->ip_kernel_memory,kernel->puerto_kernel_memory);
  
    if(kernel->socket_kernel_memory == -1){
        log_error(kernel->logger, "Error al conectar con kernel memory");  
        exit(EXIT_FAILURE); 
    }

    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(KERNEL_SCHEDULER_HANDSHAKE, buffer);

    enviar_paquete(paquete, kernel->socket_kernel_memory, kernel->logger);

    eliminar_paquete(paquete);

}

void esperar_conexiones() {
    int server_fd = iniciar_servidor(kernel->puerto_escucha);

    if (server_fd == -1) {
        log_error(kernel->logger, "No se pudo iniciar el servidor Scheduler en puerto %s", kernel->puerto_escucha);
        return;
    }
    log_info(kernel->logger, "Servidor Scheduler escuchando en puerto %s", kernel->puerto_escucha);

    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        if (cliente_fd != -1) {
            log_info(kernel->logger, "Cliente conectado en socket %d", cliente_fd);
            pthread_t hilo_atencion;
            t_atencion_cliente* datos = malloc(sizeof(t_atencion_cliente));
            datos->socket_cliente = cliente_fd;
            datos->logger = kernel->logger;

            // Creamos un hilo por cada nueva conexión
            pthread_create(&hilo_atencion, NULL, atender_cliente_scheduler, datos);
            pthread_detach(hilo_atencion);
        }
    }
}

void* atender_cliente_scheduler(void* arg) {
    t_atencion_cliente* datos = (t_atencion_cliente*) arg;
    int socket_cliente = datos->socket_cliente;
    t_log* logger = datos->logger;

    while (1) {
        log_info(logger, "Nuevo cliente detectado en socket %d. Leyendo operación...", socket_cliente);
        t_list* paquete = recibir_paquete(socket_cliente);
        if (paquete == NULL) {
            log_error(logger, "El cliente en socket %d se desconectó o envió un paquete inválido", socket_cliente);
            break;
        }
        int* cod_op_ptr = (int*) list_get(paquete, 0);
        if (cod_op_ptr == NULL) {
            log_error(logger, "Error al obtener código de operación del paquete");
            list_destroy_and_destroy_elements(paquete,free);
            break;
        }
        int cod_op = *cod_op_ptr;
        log_info(logger, "Código de operación recibido: %d", cod_op);

        switch (cod_op) {
            case CPU_HANDSHAKE:
               //Lee el ID de la CPU (si lo envió, si no se asigna uno por defecto)
                int* id_cpu_ptr = (int*) list_get(paquete, 1);
                int id_cpu;
                if (id_cpu_ptr != NULL) {
                    id_cpu = *id_cpu_ptr;
                }else{
                    id_cpu = idCPUParaAsignar;
                    log_warning(kernel->logger, "Se asigna id a CPU");
                    idCPUParaAsignar++;
                }
                //Crea la estructura que representa a esa CPU
                t_cpu_conectada* nuevaCPU = malloc(sizeof(t_cpu_conectada));
                nuevaCPU->socket_cliente = datos->socket_cliente;
                nuevaCPU->id_cpu = id_cpu;
                nuevaCPU->libre = true;
                nuevaCPU->pidEjecutando = -1; // no ejecuta ninguno
                nuevaCPU->pcbEjecutando = NULL;

                // La agrega a la cola de CPUs 
                pthread_mutex_lock(&mutex_CPU);
                queue_push(colaCPUs, nuevaCPU);
                pthread_mutex_unlock(&mutex_CPU);

                log_info(logger, "## CPU %d Conectada", id_cpu);

                // Crea un hilo dedicado para atender a esa CPU
                pthread_t hilo_cpu;
                int* socket_cpu_ptr = malloc(sizeof(int));
                *socket_cpu_ptr = nuevaCPU->socket_cliente;
                pthread_create(&hilo_cpu, NULL, atender_cpu, socket_cpu_ptr);
                pthread_detach(hilo_cpu);

               /* if(!kernel->procesoInicialCreado){
                    log_info(logger, "Creando proceso inicial...");
                    crearProceso(pathInicial, 0);
                    kernel->procesoInicialCreado = true; 
                }*/ // se movio al main para que se cree antes de esperar CPUs, asi no hay riesgo de que llegue una CPU nueva y no haya proceso inicial creado
                
                list_destroy_and_destroy_elements(paquete, free);
                return NULL; // Salimos del hilo de atención porque ahora cada CPU tiene su propio hilo dedicado
                break;
            case IO_HANDSHAKE:
                log_info(logger, "Nuevo módulo de I/O detectado en socket %d. Leyendo datos...", socket_cliente);
                char* nombre_interfaz = (char*) list_get(paquete, 1);
                int* tipo_interfaz_ptr = (int*) list_get(paquete, 2);

                if (nombre_interfaz == NULL || tipo_interfaz_ptr == NULL) {
                    log_error(logger, "Handshake de IO inválido en socket %d", socket_cliente);
                    break;
                }

                // Crear y rellenar la estructura de control de la interfaz
                t_interfaz_conectada* nueva_io = malloc(sizeof(t_interfaz_conectada));
                nueva_io->nombre = strdup(nombre_interfaz);
                nueva_io->tipo = (t_tipo_io)(*tipo_interfaz_ptr);
                nueva_io->socket_interfaz = socket_cliente;
                nueva_io->ocupada = false;

                // Agregar la interfaz a la lista global de forma segura
                pthread_mutex_lock(&mutex_lista_interfaces);
                list_add(lista_interfaces_io, nueva_io);
                int total_interfaces = list_size(lista_interfaces_io); // para prueba
                pthread_mutex_unlock(&mutex_lista_interfaces);

                const char* tipo_str = "DESCONOCIDO";
                if (nueva_io->tipo == IO_SLEEP) tipo_str = "SLEEP";
                else if (nueva_io->tipo == IO_STDIN) tipo_str = "STDIN";
                else if (nueva_io->tipo == IO_STDOUT) tipo_str = "STDOUT";

                log_info(logger, "## Interfaz registrada exitosamente. Nombre: %s. Tipo: %s (%d). Socket: %d. Total interfaces: %d", nueva_io->nombre, tipo_str, nueva_io->tipo, nueva_io->socket_interfaz, total_interfaces);
                // imprimir_lista_interfaces_io(logger);
                break;
        }
    list_destroy_and_destroy_elements(paquete,free);
    }
}
/*
void imprimir_lista_interfaces_io(t_log* logger) {
    pthread_mutex_lock(&mutex_lista_interfaces);
    
    int total = list_size(lista_interfaces_io);
    int count_sleep = 0, count_stdin = 0, count_stdout = 0;
    
    // Contar por tipo
    for (int i = 0; i < total; i++) {
        t_interfaz_conectada* io = list_get(lista_interfaces_io, i);
        if (io != NULL) {
            if (io->tipo == IO_SLEEP) count_sleep++;
            else if (io->tipo == IO_STDIN) count_stdin++;
            else if (io->tipo == IO_STDOUT) count_stdout++;
        }
    }

    log_info(logger, "\n========== RESUMEN DE INTERFACES IO ==========");
    log_info(logger, "Total de interfaces: %d", total);
    log_info(logger, "  - SLEEP:  %d", count_sleep);
    log_info(logger, "  - STDIN:  %d", count_stdin);
    log_info(logger, "  - STDOUT: %d", count_stdout);
    log_info(logger, "===============================================");
    
    // Detalles de cada interfaz
    for (int i = 0; i < total; i++) {
        t_interfaz_conectada* io = list_get(lista_interfaces_io, i);
        if (io != NULL) {
            const char* tipo_str = "DESCONOCIDO";
            if (io->tipo == IO_SLEEP) tipo_str = "SLEEP";
            else if (io->tipo == IO_STDIN) tipo_str = "STDIN";
            else if (io->tipo == IO_STDOUT) tipo_str = "STDOUT";

            t_queue* cola_tipo = obtener_cola_bloqueados_por_tipo(io->tipo);
            int bloqueados = cola_tipo ? queue_size(cola_tipo) : 0;
            log_info(logger, "  [%d] Tipo: %-8s | Socket: %d | Ocupada: %s | En espera: %d", i + 1, tipo_str, io->socket_interfaz, io->ocupada ? "SI" : "NO", bloqueados);
        }
    }
    
    log_info(logger, "\n");
    pthread_mutex_unlock(&mutex_lista_interfaces);
}
*/