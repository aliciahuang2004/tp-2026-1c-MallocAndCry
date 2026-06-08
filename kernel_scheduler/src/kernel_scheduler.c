#include "kernel_scheduler.h"
#include <unistd.h>
#include <string.h>

t_list* lista_interfaces_io;
t_queue* cola_bloqueados_sleep;
t_queue* cola_bloqueados_stdin;
t_queue* cola_bloqueados_stdout;
pthread_mutex_t mutex_lista_interfaces = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_sleep = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_stdin = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_stdout = PTHREAD_MUTEX_INITIALIZER;

t_kernel_scheduler* iniciar_kernel_scheduler(char* path_config) {
    t_kernel_scheduler* kernel_scheduler = malloc(sizeof(t_kernel_scheduler));

   t_config* tmp_config = config_create(path_config);
    if (!tmp_config) return NULL; 

    char* level_str = config_get_string_value(tmp_config, "LOG_LEVEL");
    kernel_scheduler->logger = iniciar_logger("kernel_scheduler.log", "KERNEL_SCHEDULER", true, obtener_log_level(level_str));
    config_destroy(tmp_config);

    
    kernel_scheduler->config = iniciar_config(kernel_scheduler->logger, path_config);
    kernel_scheduler->puerto_escucha = config_get_string_value(kernel_scheduler->config, "PUERTO_ESCUCHA");
    kernel_scheduler->ip_kernel_memory = config_get_string_value(kernel_scheduler->config, "IP_KERNEL_MEMORY");
    kernel_scheduler->puerto_kernel_memory = config_get_string_value(kernel_scheduler->config, "PUERTO_KERNEL_MEMORY");
    kernel_scheduler->planification_algorithm = config_get_string_value(kernel_scheduler->config, "PLANIFICATION_ALGORITHM");
    kernel_scheduler->rr_quantum = config_get_int_value(kernel_scheduler->config,"RR_QUANTUM");
    kernel_scheduler->procesoInicialCreado = false;
    lista_interfaces_io = list_create(); // Inicializamos la lista de interfaces IO
    cola_bloqueados_sleep = queue_create();
    cola_bloqueados_stdin = queue_create();
    cola_bloqueados_stdout = queue_create();
    log_debug(kernel_scheduler->logger, "El kernel scheduler se inicializo correctamente");
    
    return kernel_scheduler;
}

void verificar_kernel_scheduler(t_kernel_scheduler* kernel_scheduler) {
    log_debug(kernel_scheduler->logger, "Puerto de escucha: %s", kernel_scheduler->puerto_escucha);
    log_debug(kernel_scheduler->logger, "IP Kernel Memory: %s", kernel_scheduler->ip_kernel_memory);
    log_debug(kernel_scheduler->logger, "Puerto Kernel Memory: %s", kernel_scheduler->puerto_kernel_memory);
    log_debug(kernel_scheduler->logger, "Log Level: %s", kernel_scheduler->log_level);
}

void destruir_kernel_scheduler(t_kernel_scheduler* kernel_scheduler) {
   if (!kernel_scheduler) return; // Verificar que el puntero no sea NULL antes de destruir

   if (kernel_scheduler->logger) {
       log_destroy(kernel_scheduler->logger);
   }
   if (kernel_scheduler->config) {
       config_destroy(kernel_scheduler->config);
   }
   if (kernel_scheduler->puerto_escucha) {
       free(kernel_scheduler->puerto_escucha);
   }
   if (kernel_scheduler->ip_kernel_memory) {
       free(kernel_scheduler->ip_kernel_memory);
   }
   if (kernel_scheduler->log_level) {
       free(kernel_scheduler->log_level);
   }
   if(kernel_scheduler->puerto_kernel_memory){
    free(kernel_scheduler->puerto_kernel_memory);
   }
   if(kernel_scheduler->planification_algorithm){
    free(kernel_scheduler->planification_algorithm);
   }
   
   free(kernel_scheduler);

}

void conectar_con_kernel_memory(t_kernel_scheduler* kernel_scheduler){

  kernel_scheduler->socket_kernel_memory = crear_conexion(kernel_scheduler->logger,kernel_scheduler->ip_kernel_memory,kernel_scheduler->puerto_kernel_memory);
  
    if(kernel_scheduler->socket_kernel_memory == -1){
        log_error(kernel_scheduler->logger, "Error al conectar con kernel memory");  
        exit(EXIT_FAILURE); 
    }

    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(KERNEL_SCHEDULER_HANDSHAKE, buffer);
    enviar_paquete(paquete, kernel_scheduler->socket_kernel_memory, kernel_scheduler->logger);
    eliminar_paquete(paquete);

    log_info(kernel_scheduler->logger, "## Conectado a Kernel Memory");
  
}

void esperar_conexiones(t_kernel_scheduler* scheduler) {
    int server_fd = iniciar_servidor(scheduler->puerto_escucha);
    if (server_fd == -1) {
        log_error(scheduler->logger, "No se pudo iniciar el servidor Scheduler en puerto %s", scheduler->puerto_escucha);
        return;
    }
    log_info(scheduler->logger, "Servidor Scheduler escuchando en puerto %s", scheduler->puerto_escucha);

    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        if (cliente_fd != -1) {
            log_info(scheduler->logger, "Cliente conectado en socket %d", cliente_fd);
            pthread_t hilo_atencion;
            t_atencion_cliente* datos = malloc(sizeof(t_atencion_cliente));
            datos->socket_cliente = cliente_fd;
            datos->logger = scheduler->logger;

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
            list_destroy_and_destroy_elements(paquete, free);
            break;
        }
        int cod_op = *cod_op_ptr;
        log_info(logger, "Código de operación recibido: %d", cod_op);

        switch (cod_op) {
            case CPU_HANDSHAKE:
                {   //Lee el ID de la CPU (si lo envió, si no se asigna uno por defecto)
                    int* id_cpu_ptr = (int*) list_get(paquete, 1);
                    int id_cpu = 1;
                    if (id_cpu_ptr != NULL) {
                        id_cpu = *id_cpu_ptr;
                    }
                    //Crea la estructura que representa a esa CPU
                    t_cpu_conectada* nuevaCPU = malloc(sizeof(t_cpu_conectada));
                    nuevaCPU->socket_cliente = datos->socket_cliente;
                    nuevaCPU->id_cpu = id_cpu;
                    nuevaCPU->libre = true;
                    nuevaCPU->pidEjecutando = -1; // no ejecuta ninguno
                    // La agrega a la cola de CPUs disponibles 
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
                    sem_post(&sem_hayCPUs);
                }
        
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
                imprimir_lista_interfaces_io(logger);
                break;

            case IO_OK: {
                int pid_io = -1;

                // Extraer de forma segura el PID que envió el módulo de I/O
                if (list_size(paquete) > 1) {
                    pid_io = *(int*) list_get(paquete, 1);
                    log_debug(datos->logger, "IO_OK recibido para PID %d en socket %d", pid_io, datos->socket_cliente);
                } else {
                    log_warning(datos->logger, "IO_OK recibido sin PID en socket %d", datos->socket_cliente);
                    break;
                }

                // Localizar la interfaz asociada al socket que envió el mensaje
                t_interfaz_conectada* interfaz = buscar_interfaz_por_socket(datos->socket_cliente);
                if (interfaz == NULL) {
                    log_warning(datos->logger, "No se encontró la interfaz IO asociada al socket %d", datos->socket_cliente);
                    break;
                }

                interfaz->ocupada = false;
                log_info(logger, "## Interfaz [%s] liberada por IO_OK.", interfaz->nombre);

                // Buscar y extraer el PCB para realizar la transición de estados

                t_queue* cola_tipo = obtener_cola_bloqueados_por_tipo(interfaz->tipo);
                pthread_mutex_t* mutex_tipo = obtener_mutex_cola_por_tipo(interfaz->tipo);

                t_pcb* pcb_a_desbloquear = NULL;

                // RECOLECTAMOS LA SOLICITUD QUE YA TERMINÓ
                pthread_mutex_lock(mutex_tipo);
                if (!queue_is_empty(cola_tipo)) {
                    t_solicitud_io* solicitud_terminada = queue_pop(cola_tipo); // <-- Ahora sí sacamos la que terminó
                    if (solicitud_terminada != NULL) {
                        pcb_a_desbloquear = solicitud_terminada->pcb;
                        liberar_solicitud_io(solicitud_terminada); // <-- Recién acá la limpiamos de la memoria
                    }
                }
                pthread_mutex_unlock(mutex_tipo);

                // Devolvemos el proceso recuperado a READY
                if (pcb_a_desbloquear != NULL) {
                    log_info(logger, "## PID: %d - Estado Anterior: BLOCK - Estado Actual: READY", pcb_a_desbloquear->pid);
                    pcb_a_desbloquear->estado = READY;

                    pthread_mutex_lock(&mutex_READY);
                    queue_push(colaREADY, pcb_a_desbloquear);
                    pthread_mutex_unlock(&mutex_READY);

                    sem_post(&sem_procesosReady); // Notificar al corto plazo
                } else {
                    log_error(logger, "Error: El PID %d terminó pero no había nada en la cola.", pid_io);
                }

                
                // Si la cola no quedó vacía, significa que hay otro proceso esperando el dispositivo
                pthread_mutex_lock(mutex_tipo);
                if (!queue_is_empty(cola_tipo)) {
                    // Usamos queue_peek para mirar cuál es la siguiente solicitud sin sacarla de la cola
                    t_solicitud_io* solicitud_siguiente = queue_peek(cola_tipo);
                    if (solicitud_siguiente != NULL) {
                        interfaz->ocupada = true;
                        pthread_mutex_unlock(mutex_tipo);

                        log_info(logger, "## Interfaz [%s] ocupada de inmediato. Despachando siguiente PID en cola: %d.", 
                                 interfaz->nombre, solicitud_siguiente->pid);
                        
                        enviar_operacion_a_io(interfaz, solicitud_siguiente);
                    } else {
                        pthread_mutex_unlock(mutex_tipo);
                        log_error(logger, "Error interno: cola IO no vacía pero queue_peek devolvió NULL.");
                    }
                } else {
                    pthread_mutex_unlock(mutex_tipo);
                }
                break;
            }
            default:
                log_warning(datos->logger, "Operación desconocida de cliente en socket %d", datos->socket_cliente);
                break;
        }
        list_destroy_and_destroy_elements(paquete, free);
    // Aca el hilo puede continuar en un bucle según la necesidad del protocolo
    }
    free(datos);
    return NULL;
}

t_queue* obtener_cola_bloqueados_por_tipo(t_tipo_io tipo) {
    switch (tipo) {
        case IO_SLEEP: return cola_bloqueados_sleep;
        case IO_STDIN: return cola_bloqueados_stdin;
        case IO_STDOUT: return cola_bloqueados_stdout;
        default: return NULL;
    }
}

pthread_mutex_t* obtener_mutex_cola_por_tipo(t_tipo_io tipo) {
    switch (tipo) {
        case IO_SLEEP: return &mutex_cola_sleep;
        case IO_STDIN: return &mutex_cola_stdin;
        case IO_STDOUT: return &mutex_cola_stdout;
        default: return NULL;
    }
}

t_interfaz_conectada* buscar_interfaz_libre_por_tipo(t_tipo_io tipo) {
    pthread_mutex_lock(&mutex_lista_interfaces);

    t_interfaz_conectada* encontrada = NULL;
    int cantidad = list_size(lista_interfaces_io);
    for (int i = 0; i < cantidad; i++) {
        t_interfaz_conectada* io = list_get(lista_interfaces_io, i);
        if (io != NULL && io->tipo == tipo && !io->ocupada) {
            encontrada = io;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_lista_interfaces);
    return encontrada;
}

t_interfaz_conectada* buscar_interfaz_por_socket(int socket_interfaz) {
    pthread_mutex_lock(&mutex_lista_interfaces);

    t_interfaz_conectada* encontrada = NULL;
    int cantidad = list_size(lista_interfaces_io);
    for (int i = 0; i < cantidad; i++) {
        t_interfaz_conectada* io = list_get(lista_interfaces_io, i);
        if (io != NULL && io->socket_interfaz == socket_interfaz) {
            encontrada = io;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_lista_interfaces);
    return encontrada;
}

t_solicitud_io* crear_solicitud_io(int pid,t_pcb* pcb, t_io_operation tipo_operacion, uint32_t datos_size, void* datos) {
    t_solicitud_io* solicitud = malloc(sizeof(t_solicitud_io));
    solicitud->pid = pid;
    solicitud->pcb = pcb;
    solicitud->tipo_operacion = tipo_operacion;
    solicitud->datos_size = datos_size;
    solicitud->datos = datos;
    return solicitud;
}

void liberar_solicitud_io(t_solicitud_io* solicitud) {
    if (!solicitud) return;
    if (solicitud->datos) free(solicitud->datos);
    free(solicitud);
}

void enviar_operacion_a_io(t_interfaz_conectada* interfaz, t_solicitud_io* solicitud) {
    op_code codigo = SLEEP;
    if (solicitud->tipo_operacion == OP_STDIN) {
        codigo = STDIN;
    } else if (solicitud->tipo_operacion == OP_STDOUT) {
        codigo = STDOUT;
    }

    t_paquete* paquete_a_io = crear_paquete(codigo, crear_buffer());
    agregar_a_paquete(paquete_a_io, &solicitud->pid, sizeof(int));

    if (solicitud->tipo_operacion == OP_SLEEP) {
        agregar_a_paquete(paquete_a_io, solicitud->datos, solicitud->datos_size);
        int tiempo = *(int*)solicitud->datos;
        log_debug(kernel->logger, "Enviando orden SLEEP de %d ms a la interfaz %s", tiempo, interfaz->nombre);
    } else if (solicitud->tipo_operacion == OP_STDIN) {
        uint32_t* params = solicitud->datos;
        uint32_t dir_logica = params[0];
        uint32_t tamano = params[1];
        agregar_a_paquete(paquete_a_io, &dir_logica, sizeof(uint32_t));
        agregar_a_paquete(paquete_a_io, &tamano, sizeof(uint32_t));
        log_debug(kernel->logger, "Enviando orden STDIN (Dir: %u, Tam: %u) a la interfaz %s", dir_logica, tamano, interfaz->nombre);
    } else if (solicitud->tipo_operacion == OP_STDOUT) {
        uint32_t* params = solicitud->datos;
        uint32_t dir_logica = params[0];
        uint32_t tamano = params[1];
        agregar_a_paquete(paquete_a_io, &dir_logica, sizeof(uint32_t));
        agregar_a_paquete(paquete_a_io, &tamano, sizeof(uint32_t));
        log_debug(kernel->logger, "Enviando orden STDOUT (Dir: %u, Tam: %u) a la interfaz %s", dir_logica, tamano, interfaz->nombre);
    }

    enviar_paquete(paquete_a_io, interfaz->socket_interfaz, kernel->logger);
    eliminar_paquete(paquete_a_io);
}

// Función de debug para visualizar el estado actual de las interfaces IO registradas
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