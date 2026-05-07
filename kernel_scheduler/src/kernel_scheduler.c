#include "kernel_scheduler.h"
#include <unistd.h>

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
   free(kernel_scheduler);

}

void conectar_con_kernel_memory(t_kernel_scheduler* kernel_scheduler){

  kernel_scheduler->socket_kernel_memory = crear_conexion(kernel_scheduler->logger,kernel_scheduler->ip_kernel_memory,kernel_scheduler->puerto_kernel_memory);
  
  if(kernel_scheduler->socket_kernel_memory != -1){
    log_info(kernel_scheduler->logger,COLOR_VERDE "## Conexión al Kernel Memory exitosa. IP:%s, Puerto: %s\033[0m",kernel_scheduler->ip_kernel_memory,kernel_scheduler->puerto_kernel_memory);

  }else{
    log_error(kernel_scheduler->logger,"Error al conectar con kernel memory");  
    EXIT_FAILURE;  
  }
  
}

void esperar_conexiones(t_kernel_scheduler* scheduler) {
    int server_fd = iniciar_servidor(scheduler->puerto_escucha);
    log_info(scheduler->logger, "Servidor Scheduler escuchando en puerto %s", scheduler->puerto_escucha);

    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        if (cliente_fd != -1) {
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
        // Recibir el paquete completo
        t_list* paquete = recibir_paquete(datos->socket_cliente);
        if (paquete == NULL) {
            log_error(logger, "El cliente en socket %d se desconectó", socket_cliente);
            break;
        }
        // Obtener el código de operación
        int* cod_op_ptr = (int*) list_get(paquete, 0);
        if (cod_op_ptr == NULL) {
            log_error(logger, "Error al obtener código de operación del paquete");
            list_destroy_and_destroy_elements(paquete, free);
            break;
        }
        int cod_op = *cod_op_ptr;

        switch (cod_op) {
            case CPU_HANDSHAKE:
                int* id_cpu_ptr = (int*) list_get(paquete, 1);
                 if (id_cpu_ptr != NULL) {
                    t_cpu_conectada* nuevaCPU = malloc(sizeof(t_cpu_conectada));
                    nuevaCPU -> socket_cliente = datos -> socket_cliente;
                    nuevaCPU -> id_cpu = *id_cpu_ptr;
                    nuevaCPU -> id_cpu = true;
                    log_info(logger, "Nueva CPU (id: %d) conectada en socket %d", nuevaCPU -> id_cpu, datos->socket_cliente);
                    pthread_mutex_lock(&mutex_CPU);
                    queue_push(colaCPUs, nuevaCPU);
                    pthread_mutex_unlock(&mutex_CPU);
                } else {
                    log_error(logger, "No se pudo obtener id de la cpu con la que se estaba iniciando la conexion");
                }
                break;
            case IO_HANDSHAKE:
                log_debug(datos->logger, "Nuevo módulo de I/O conectado en socket %d", datos->socket_cliente);
                // Lógica para peticiones de dispositivos de entrada/salida
                
                // ========== PRUEBAS DE IO
                /*
                int pid = 123;
                // Ejemplo para enviar una petición de IO de prueba para STDIN
                t_io_operation tipo_op = OP_STDIN;
                uint32_t datos_size = 10;
                //Ejemplo para prueba ,definimos los datos de la petición (ejemplo: SLEEP de 2.5 segundos)
                t_io_operation tipo_op = OP_SLEEP; (Para prueba de SLEEP) 
                uint32_t tiempo_ms = 2500; (Para prueba de SLEEP)
                
                // Ejemplo para enviar una petición de IO de prueba para STDOUT
                t_io_operation tipo_op = OP_STDOUT;
                char* datos_stdout = "Hola Mundo desde STDOUT";
                uint32_t datos_size = strlen(datos_stdout) + 1;
                
                // Creamos el paquete con el código IO_REQUEST
                t_paquete* paquete_request = crear_paquete(IO_REQUEST, crear_buffer());
            
                // Agregamos los campos respetando el orden del receptor 
                agregar_a_paquete(paquete_request, &pid, sizeof(int));
                agregar_a_paquete(paquete_request, &tipo_op, sizeof(t_io_operation));
                agregar_a_paquete(paquete_request, &datos_size, sizeof(uint32_t));
                agregar_a_paquete(paquete_request, datos_stdout, datos_size);

                // Enviamos y liberamos
                enviar_paquete(paquete_request, datos->socket_cliente, datos->logger);
                eliminar_paquete(paquete_request);
                */
                // ==================================================================================
                break;
            case IO_OK: // RESPUESTA DE IO
                // El paquete ya fue recibido y deserializado en "paquete"
                if (list_size(paquete) > 1) {
                    int pid_io = *(int*) list_get(paquete, 1);
                    log_debug(datos->logger, "IO_OK recibido para PID %d en socket %d", pid_io, datos->socket_cliente);
                } else {
                    log_warning(datos->logger, "IO_OK recibido sin PID en socket %d", datos->socket_cliente);
                }
                // Aca iria la lógica de desbloqueo del proceso y envío al planificador
                break;
                   
            default:
                log_warning(datos->logger, "Operación desconocida de cliente en socket %d", datos->socket_cliente);
                break;
        }

        // Liberar el paquete recibido
        list_destroy_and_destroy_elements(paquete, free);

    // Aca el hilo puede continuar en un bucle según la necesidad del protocolo
    }
    free(datos);
    return NULL;
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
