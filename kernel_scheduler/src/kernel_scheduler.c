#include "kernel_scheduler.h"


t_kernell_scheduler* iniciar_kernel_scheduler() {
    t_kernel_scheduler* kernel_scheduler = malloc(sizeof(t_kernel_scheduler));

    kernel_scheduler->config = iniciar_config(kernel_scheduler->logger, "kernel_scheduler.config");
    kernel_scheduler->log_level = config_get_string_value(kernel_scheduler->config, "LOG_LEVEL");
    kernel_scheduler->logger = iniciar_logger("kernel_scheduler.log", "KERNEL_SCHEDULER", 1, log_level);


    kernel_scheduler->puerto_escucha = config_get_int_value(kernel_scheduler->config, "PUERTO_ESCUCHA");
    kernel_scheduler->ip_kernel_memory = config_get_string_value(kernel_scheduler->config, "IP_KERNEL_MEMORY");
    kernel_scheduler->puerto_kernel_memory = config_get_int_value(kernel_scheduler->config, "PUERTO_KERNEL_MEMORY");
    log_debug(kernel_scheduler->logger, "El kernel scheduler se inicializo correctamente");

    return kernel_scheduler;
}

void verificar_kernel_scheduler(t_kernel_scheduler* kernel_scheduler) {
    log_debug(kernel_scheduler->logger, "Puerto de escucha: %d", kernel_scheduler->puerto_escucha);
    log_debug(kernel_scheduler->logger, "IP Kernel Memory: %s", kernel_scheduler->ip_kernel_memory);
    log_debug(kernel_scheduler->logger, "Puerto Kernel Memory: %d", kernel_scheduler->puerto_kernel_memory);
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
   free(kernel_scheduler);

}