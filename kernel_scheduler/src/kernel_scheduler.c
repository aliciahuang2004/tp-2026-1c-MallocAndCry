#include "kernel_scheduler.h"


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
int conectar_con_kernel_memory(t_kernel_scheduler* kernel_scheduler){
  

  kernel_scheduler->socket_kernel_memory = crear_conexion(kernel_scheduler->logger,kernel_scheduler->ip_kernel_memory,kernel_scheduler->puerto_kernel_memory);
  
  if(kernel_scheduler->socket_kernel_memory != -1){
    log_info(kernel_scheduler->logger,COLOR_VERDE "## Conexiòn al Kernel Memory exitosa. IP:%s, Puerto: %s\033[0m",kernel_scheduler->ip_kernel_memory,kernel_scheduler->puerto_kernel_memory);

  }else{
    log_error(kernel_scheduler->logger,"Error al conectar con kernel memory");  
    return -1;  
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
            pthread_create(&hilo_atencion, NULL, (void*)atender_cliente_scheduler, datos);
            pthread_detach(hilo_atencion);
        }
    }
}
void atender_cliente_scheduler(void* arg) {
    t_atencion_cliente* datos = (t_atencion_cliente*) arg;
    
    // Identificación del cliente mediante el primer paquete
    int cod_op = recibir_operacion(datos->socket_cliente);

    switch (cod_op) {
        case CPU_HANDSHAKE:
            log_info(datos->logger, "Nueva CPU conectada en socket %d", datos->socket_cliente);
            // Lógica para gestionar ciclos de instrucción de la CPU
            break;
        case IO_HANDSHAKE:
            log_info(datos->logger, "Nuevo módulo de I/O conectado en socket %d", datos->socket_cliente);
            // Lógica para peticiones de dispositivos de entrada/salida
            break;
        default:
            log_warning(datos->logger, "Operación desconocida de cliente en socket %d", datos->socket_cliente);
            break;
    }

    // Aquí el hilo puede continuar en un bucle según la necesidad del protocolo
    free(datos);
}
int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		pclose(socket_cliente);
		return -1;
	}
}