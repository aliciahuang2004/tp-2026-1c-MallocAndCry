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
    char* colas_desalojan = config_get_string_value(kernel_scheduler->config, "QUEUE_PREEMPTION");
    if(strcmp(colas_desalojan, "TRUE") == 0){
        kernel_scheduler->queues_preemption = true;
    }else{
        kernel_scheduler->queues_preemption = false;
    }
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

    log_info(kernel->logger, "## Conectado a Kernel Memory");

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
            case CPU_HANDSHAKE:{
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
            }
            case IO_HANDSHAKE:{
                log_info(logger, "Nuevo módulo de I/O detectado en socket %d. Leyendo datos...", socket_cliente);
                int* tipo_interfaz_ptr = (int*) list_get(paquete, 1);

                if (tipo_interfaz_ptr == NULL) {
                    log_error(logger, "Handshake de IO inválido en socket %d", socket_cliente);
                    break;
                }

                t_tipo_io tipo_interfaz = *tipo_interfaz_ptr;

                if (tipo_interfaz < 0 || tipo_interfaz > 2) {
                    log_error(kernel->logger, "Tipo de IO inválido (%d)", tipo_interfaz);
                    return NULL;
                }

                pthread_mutex_lock(&mutex_interfaces[tipo_interfaz]);
                if(interfaces[tipo_interfaz].socket_interfaz == -1){
                    interfaces[tipo_interfaz].socket_interfaz = socket_cliente;
                    interfaces[tipo_interfaz].ocupada = false;
                    log_info(kernel->logger, "## Interfaz registrada: Tipo: %s. Socket: %d", interfaces[tipo_interfaz].nombre, interfaces[tipo_interfaz].socket_interfaz);
                    pthread_mutex_unlock(&mutex_interfaces[tipo_interfaz]);
                }else{
                    log_error(kernel->logger, "ERROR: Tipo %s, ya conectado en socket: %d", interfaces[tipo_interfaz].nombre, interfaces[tipo_interfaz].socket_interfaz);
                    close(socket_cliente);
                    return NULL ;
                }
                
                pthread_t hilo_io;
                int* socket_io_ptr = malloc(sizeof(int));
                *socket_io_ptr = interfaces[tipo_interfaz].socket_interfaz;
                pthread_create(&hilo_io, NULL, atender_io, socket_io_ptr);
                pthread_detach(hilo_io);

                list_destroy_and_destroy_elements(paquete,free);
                return NULL; 
                break;
            }
        }
        list_destroy_and_destroy_elements(paquete,free);
    }
    return NULL;
}

void inicializar_interfaces() {
    interfaces[IO_SLEEP].nombre = "SLEEP";
    interfaces[IO_SLEEP].tipo = IO_SLEEP;
    interfaces[IO_SLEEP].socket_interfaz = -1;
    interfaces[IO_SLEEP].ocupada = false;
    interfaces[IO_SLEEP].pidAsignado = -1;
    interfaces[IO_SLEEP].solicitudes = queue_create();

    interfaces[IO_STDIN].nombre = "STDIN";
    interfaces[IO_STDIN].tipo = IO_STDIN;
    interfaces[IO_STDIN].socket_interfaz = -1;
    interfaces[IO_STDIN].ocupada = false;
    interfaces[IO_STDIN].pidAsignado = -1;
    interfaces[IO_STDIN].solicitudes = queue_create();

    interfaces[IO_STDOUT].nombre = "STDOUT";
    interfaces[IO_STDOUT].tipo = IO_STDOUT;
    interfaces[IO_STDOUT].socket_interfaz = -1;
    interfaces[IO_STDOUT].ocupada = false;
    interfaces[IO_STDOUT].pidAsignado = -1;
    interfaces[IO_STDOUT].solicitudes = queue_create();

    for (int i = 0; i < 3; i++) {
        pthread_mutex_init(&mutex_interfaces[i], NULL);
    }
    sem_init(&sem_hayIO,0,0);

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