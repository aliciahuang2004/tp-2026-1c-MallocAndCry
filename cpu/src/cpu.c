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


    if (config_has_property(cpu->config, "IP_MEMORY_STICK") && config_has_property(cpu->config, "PUERTO_MEMORY_STICK")) {
        cpu->ip_memory_stick_inicial = config_get_string_value(cpu->config, "IP_MEMORY_STICK");
        cpu->puerto_memory_stick_inicial = config_get_string_value(cpu->config, "PUERTO_MEMORY_STICK");
    } else {
        cpu->ip_memory_stick_inicial = NULL;
        cpu->puerto_memory_stick_inicial = NULL;
    }


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
    if (ip == NULL || puerto == NULL) return -1; // cuando no hay stick configurado

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

void esperar_proceso(t_cpu* cpu) {
    log_info(cpu->logger, "CPU esperando procesos del Kernel Scheduler");
    
    while(1){
        t_list* paquete = recibir_paquete(cpu->socket_kernel_scheduler);

        if(!paquete){
            log_error(cpu->logger, "Error al recibir paquete del Kernel Scheduler. Desconectado.");
            break;
        }
        int cod_op = *(int*)list_get(paquete, 0); //el cod_op siempre va a estar en la posicion 0, devuelve un puntero void*, por eso el casteo a int* y luego se desreferencia para obtener el valor

        if(cod_op ==PROCESO_A_PROCESAR){
                int pid_a_ejecutar = *(int*)list_get(paquete, 1);
                log_info(cpu->logger, "CPU recibió proceso a procesar con PID: %d", pid_a_ejecutar);
                
                // aca pido contexto a KERNEL MEMORY
                t_contexto* contexto_actual = solicitar_contexto(cpu, pid_a_ejecutar);

                if (contexto_actual!=NULL) {
                    log_info(cpu->logger,"Contexto recibido para PID %d", contexto_actual->pid);

                    //inicia el ciclo de instruccion
                    ciclo_de_instruccion(cpu,contexto_actual);
                    free(contexto_actual);

                } else {
                    log_warning(cpu->logger, "Código de operación desconocido: %d", cod_op);
                }
            list_destroy_and_destroy_elements(paquete, free);

        }
    }
}

t_contexto* solicitar_contexto(t_cpu* cpu, int pid) {
    log_debug(cpu->logger, "Solicitando contexto para PID %d a Kernel Memory", pid);
    
    //armo paquete
    t_paquete* paquete = crear_paquete(REQUEST_CONTEXTO, crear_buffer());

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete,&cpu->id,sizeof(int));//**********AGREGUÉ PARA QUE KM LOGUEE ID DE LA CPU QUE LE SOLICITÓ CTX
    enviar_paquete(paquete, cpu->socket_kernel_memory, cpu->logger);
    eliminar_paquete(paquete);

    //espero respuesta
    t_list* respuesta = recibir_paquete(cpu->socket_kernel_memory);
    if (!respuesta) {
        log_error(cpu->logger, "Error al recibir contexto de Kernel Memory para PID %d", pid);
        return NULL;
    }

    int cod_op = *(int*)list_get(respuesta, 0);
    t_contexto* contexto_recibido = NULL;

    if(cod_op == CONTEXT_RESPONSE) {
        contexto_recibido = malloc(sizeof(t_contexto));
        //TODO aca deberia recibir tmb los registros, consultar qué datos mas deberia recibir para el contexto
        contexto_recibido->pid= pid; //el pid lo tengo
        contexto_recibido->registros.PC = *(uint32_t*)list_get(respuesta, 1); 
        contexto_recibido->registros.AX = *(uint8_t*)list_get(respuesta, 2);
        contexto_recibido->registros.BX = *(uint8_t*)list_get(respuesta, 3);
        contexto_recibido->registros.CX = *(uint8_t*)list_get(respuesta, 4);
        contexto_recibido->registros.DX = *(uint8_t*)list_get(respuesta, 5);
        contexto_recibido->registros.EAX = *(uint32_t*)list_get(respuesta, 6);
        contexto_recibido->registros.EBX = *(uint32_t*)list_get(respuesta, 7);
        contexto_recibido->registros.ECX = *(uint32_t*)list_get(respuesta, 8);
        contexto_recibido->registros.EDX = *(uint32_t*)list_get(respuesta, 9);
        contexto_recibido->registros.SI = *(uint32_t*)list_get(respuesta, 10);
        contexto_recibido->registros.DI = *(uint32_t*)list_get(respuesta, 11);
        log_debug(cpu->logger, "Contexto recibido: PID=%d, PC=%s", contexto_recibido->pid, contexto_recibido->registros);
    } else {
        log_warning(cpu->logger, "Código de operación inesperado en respuesta de Kernel Memory: %d", cod_op);
    }

    list_destroy_and_destroy_elements(respuesta, free);
    return contexto_recibido;

}

void ciclo_de_instruccion(t_cpu *cpu,t_contexto* contexto) {
    int ejecutando = 1;// para mantener el ciclo corriendo

    while(ejecutando){

        //FETCH
        log_info(cpu->logger, "##PID: %d - FETCH - Program Counter: %d", contexto->pid, contexto->registros.PC);

        //DECODE

        //EXECUTE

        //CHECK INTERRUPT 

        ejecutando = 0; // por ahora solo hago una iteracion del ciclo para probar, luego esto va a depender de la lógica de interrupciones y finalización del proceso
    }
} 

char* fetch_instruccion(t_cpu* cpu, t_contexto* contexto) {
    
    log_info(cpu->logger, "##PID: %d - FETCH - Program Counter: %d", contexto->pid, contexto->registros.PC);

    t_paquete* paquete = crear_paquete(PETICION_INSTRUCCION, crear_buffer());
    agregar_a_paquete(paquete, &contexto->pid, sizeof(int));
    agregar_a_paquete(paquete, &contexto->registros.PC, sizeof(uint32_t)); // es necesario pasarle lo registros?

    enviar_paquete(paquete, cpu->socket_kernel_memory, cpu->logger);
    eliminar_paquete(paquete);

    //espero respuesta
    t_list* respuesta = recibir_paquete(cpu->socket_kernel_memory);
    if (!respuesta) {
        log_error(cpu->logger, "Error al recibir instrucción de Kernel Memory para PID %d", contexto->pid);
        return NULL;
    }

    int cod_op = *(int*)list_get(respuesta, 0);
    char* instruccion_leida = NULL;

    if(cod_op == RESPUESTA_INSTRUCCION){ //todavia no tengo este tipo operacion
        char* str_recibido = (char*)list_get(respuesta, 1); // asumo que el string viene en la posicion 1 del paquete
        instruccion_leida = strdup(str_recibido); // duplico el string para devolverlo, luego se debe liberar
    }
    else {
        log_error(cpu->logger, "Código de operación inesperado en respuesta de Kernel Memory: %d", cod_op);
    }

    list_destroy_and_destroy_elements(respuesta, free);

    // deberia sumar un 1 al PC pero lo deberia hacer en la otra funcion


   
    return instruccion_leida;
}