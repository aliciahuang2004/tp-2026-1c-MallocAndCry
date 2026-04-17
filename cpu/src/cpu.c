#include "cpu.h"

t_cpu* iniciar_cpu(char* path_config, char* id_cpu) {
    t_cpu* cpu = malloc(sizeof(t_cpu));
    cpu->id = strdup(id_cpu); 
    char log_name[50];
    snprintf(log_name, sizeof(log_name), "cpu_%s.log", cpu->id);
    cpu->logger = iniciar_logger(log_name, "CPU", true, LOG_LEVEL_INFO);
    cpu->config = iniciar_config(cpu->logger, path_config);
    cpu->log_level = config_get_string_value(cpu->config, "LOG_LEVEL");
    cpu->logger->detail = obtener_log_level(cpu->log_level);

    cpu->ip_kernel_scheduler = config_get_string_value(cpu->config, "IP_KERNEL_SCHEDULER");
    cpu->puerto_kernel_scheduler = config_get_string_value(cpu->config, "PUERTO_KERNEL_SCHEDULER");

    cpu->ip_kernel_memory = config_get_string_value(cpu->config, "IP_KERNEL_MEMORY");
    cpu->puerto_kernel_memory = config_get_string_value(cpu->config, "PUERTO_KERNEL_MEMORY");

    // --- AGREGAR ESTO ---
    if (config_has_property(cpu->config, "IP_MEMORY_STICK") && config_has_property(cpu->config, "PUERTO_MEMORY_STICK")) {
        cpu->ip_memory_stick_inicial = config_get_string_value(cpu->config, "IP_MEMORY_STICK");
        cpu->puerto_memory_stick_inicial = config_get_string_value(cpu->config, "PUERTO_MEMORY_STICK");
    } else {
        cpu->ip_memory_stick_inicial = NULL;
        cpu->puerto_memory_stick_inicial = NULL;
    }
    // --------------------

    cpu->sockets_memory_sticks = list_create();

    return cpu;
}

int conectar_kernel_memory(t_cpu* cpu) {
    cpu->socket_kernel_memory = crear_conexion(cpu->logger, cpu->ip_kernel_memory, cpu->puerto_kernel_memory);
    
    if (cpu->socket_kernel_memory != -1) {
        int id_cpu_int = atoi(cpu->id);
        
    
        t_buffer* buffer = crear_buffer();
        t_paquete* paquete = crear_paquete(CPU_HANDSHAKE, buffer); 
        
        agregar_a_paquete(paquete, &id_cpu_int, sizeof(int));
        
        if (enviar_paquete(paquete, cpu->socket_kernel_memory, cpu->logger) == -1) {
             log_error(cpu->logger, "Fallo el envío del Handshake a Kernel Memory");
             eliminar_paquete(paquete);
             return -1;
        }
        
        eliminar_paquete(paquete);
        log_info(cpu->logger, "## CPU conectada a Kernel Memory en %s:%s", cpu->ip_kernel_memory, cpu->puerto_kernel_memory);
        return 1;
    }
    return -1;
}

int conectar_kernel_scheduler(t_cpu* cpu) {
    cpu->socket_kernel_scheduler = crear_conexion(cpu->logger, cpu->ip_kernel_scheduler, cpu->puerto_kernel_scheduler);
    
    if (cpu->socket_kernel_scheduler != -1) {
        int id_cpu_int = atoi(cpu->id);
        
        t_buffer* buffer = crear_buffer();
        t_paquete* paquete = crear_paquete(CPU_HANDSHAKE, buffer); 
        
        agregar_a_paquete(paquete, &id_cpu_int, sizeof(int));
        
        if (enviar_paquete(paquete, cpu->socket_kernel_scheduler, cpu->logger) == -1) {
             log_error(cpu->logger, "Fallo el envío del Handshake a Kernel Scheduler");
             eliminar_paquete(paquete);
             return -1;
        }
        
        eliminar_paquete(paquete);
        log_info(cpu->logger, "## CPU conectada a Kernel Scheduler en %s:%s", cpu->ip_kernel_scheduler, cpu->puerto_kernel_scheduler);
        return 1;
    }
    return -1;
}

int conectar_memory_stick(t_cpu* cpu, char* ip, char* puerto) {
    if (ip == NULL || puerto == NULL) return -1; // No hay stick configurado

    int socket_ms = crear_conexion(cpu->logger, ip, puerto);
    
    if (socket_ms != -1) {
        int id_cpu_int = atoi(cpu->id);

        t_buffer *buffer = crear_buffer();
        t_paquete *paquete = crear_paquete(CPU_HANDSHAKE, buffer);

        agregar_a_paquete(paquete, &id_cpu_int, sizeof(int));
        
        if (enviar_paquete(paquete, socket_ms, cpu->logger) == -1) {
             log_error(cpu->logger, "Fallo el envío del Handshake a Memory Stick %s:%s", ip, puerto);
             eliminar_paquete(paquete);
             return -1;
        }
        
        eliminar_paquete(paquete);
        
        // Guardamos el socket en nuestra lista para usarlo en el futuro
        list_add(cpu->sockets_memory_sticks, (void*)(intptr_t)socket_ms);
        log_info(cpu->logger, "## CPU conectada a Memory Stick en %s:%s", ip, puerto);
        
        return 1;
    }
    return -1;
}

void liberar_cpu(t_cpu* cpu) {
    if (!cpu) return;
    if (cpu->config) terminar_programa(cpu->logger, cpu->config); 
    if (cpu->id) free(cpu->id);
    if (cpu->sockets_memory_sticks) list_destroy(cpu->sockets_memory_sticks);
    
    free(cpu);
}