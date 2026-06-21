#include "cpu.h"
#include "instrucciones.h"
#include <sys/select.h>

sem_t sem_contexto_recibido;
sem_t sem_instruccion_recibida;
t_contexto* buffer_contexto = NULL;
char* buffer_instruccion = NULL;

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

int conectar_memory_stick(t_cpu* cpu, char* ip, char* puerto, int ms_id) {
    if (ip == NULL || puerto == NULL) return -1;

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
        
        // Creamos y guardamos la estructura en la lista
        t_ms_conectado* ms_conectado = malloc(sizeof(t_ms_conectado));
        ms_conectado->id = ms_id;
        ms_conectado->socket = socket_ms;

        list_add(cpu->sockets_memory_sticks, ms_conectado);
        log_info(cpu->logger, "## CPU conectada a Memory Stick (ID: %d) en %s:%s", ms_id, ip, puerto);
        
        return socket_ms;
    }
    return -1;
}

// hilo que atiende permanentemente a Kernel Memory
void* escuchar_kernel_memory(void* arg) {
    t_cpu* cpu = (t_cpu*) arg;
    log_info(cpu->logger, "Hilo de escucha de Kernel Memory iniciado.");

    while (1) {
        t_list* paquete = recibir_paquete(cpu->socket_kernel_memory);
        if (paquete == NULL) {
            log_error(cpu->logger, "Se perdió la conexión con Kernel Memory.");
            break;
        }

        int cod_op = *(int*)list_get(paquete, 0);

        switch (cod_op) {
            case MS_NUEVO_CPU: { //ID, Puerto, IP
                int ms_id = *(int*)list_get(paquete, 1);
                char* ms_puerto = (char*)list_get(paquete, 2);
                char* ms_ip = (char*)list_get(paquete, 3); 

                log_info(cpu->logger, "Aviso de KM: Nuevo Memory Stick %d disponible en %s:%s", ms_id, ms_ip, ms_puerto);
                conectar_memory_stick(cpu, ms_ip, ms_puerto, ms_id);
                break;
            }
            case CONTEXT_RESPONSE: {
                buffer_contexto = malloc(sizeof(t_contexto));
                buffer_contexto->tabla_segmentos = NULL;
                buffer_contexto->pid = *(int*)list_get(paquete, 1);
                buffer_contexto->registros.PC = *(uint32_t*)list_get(paquete, 2);
                buffer_contexto->registros.AX = *(uint8_t*)list_get(paquete, 3);
                buffer_contexto->registros.BX = *(uint8_t*)list_get(paquete, 4);
                buffer_contexto->registros.CX = *(uint8_t*)list_get(paquete, 5);
                buffer_contexto->registros.DX = *(uint8_t*)list_get(paquete, 6);
                buffer_contexto->registros.EAX = *(uint32_t*)list_get(paquete, 7);
                buffer_contexto->registros.EBX = *(uint32_t*)list_get(paquete, 8);
                buffer_contexto->registros.ECX = *(uint32_t*)list_get(paquete, 9);
                buffer_contexto->registros.EDX = *(uint32_t*)list_get(paquete, 10);
                buffer_contexto->registros.SI = *(uint32_t*)list_get(paquete, 11);
                buffer_contexto->registros.DI = *(uint32_t*)list_get(paquete, 12);

                buffer_contexto->tabla_segmentos = list_create();

                if(list_size(paquete) > 13) {
                    int cantidad_segmentos = *(int*)list_get(paquete, 13);
                    int offset = 14; 

                    for(int i = 0; i < cantidad_segmentos; i++) {
                        t_segmento* nuevo_segmento = malloc(sizeof(t_segmento));
                        
                        nuevo_segmento->id_segmento = *(int*)list_get(paquete, offset);
                        nuevo_segmento->base = *(uint32_t*)list_get(paquete, offset + 1);
                        nuevo_segmento->limite = *(uint32_t*)list_get(paquete, offset + 2);
                        nuevo_segmento->memory_stick_id = *(int*)list_get(paquete, offset + 3);

                        list_add(buffer_contexto->tabla_segmentos, nuevo_segmento);
                        offset += 4;
                    }
                }
                
                sem_post(&sem_contexto_recibido);
                break;
            }
            case RESPUESTA_INSTRUCCION: {
                char* str_recibido = (char*)list_get(paquete, 1);
                buffer_instruccion = strdup(str_recibido);
                sem_post(&sem_instruccion_recibida);
                break;
            }
            case SEG_MAX_SIZE: {
                cpu->segment_max_size = *(int*)list_get(paquete, 1);
                log_info(cpu->logger, "Tamaño de segmento recibido: %d", cpu->segment_max_size);
                break;
            }
            case ERROR_INSTRUCCION: {
                log_error(cpu->logger, "Kernel Memory reportó un error al intentar leer la instrucción.");
                buffer_instruccion = NULL; 
                sem_post(&sem_instruccion_recibida); 
                break;
            }
            default:
                log_warning(cpu->logger, "Código de KM desconocido: %d", cod_op);
        }
        list_destroy_and_destroy_elements(paquete, free);
    }
    return NULL;
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

                    if (contexto_actual->tabla_segmentos) {
                        list_destroy_and_destroy_elements(contexto_actual->tabla_segmentos, free);
                    }
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
    
    t_paquete* paquete = crear_paquete(REQUEST_CONTEXTO, crear_buffer());
    int id_cpu_int = atoi(cpu->id); 
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, &id_cpu_int, sizeof(int)); 

    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete,&cpu->id,sizeof(int));//**********AGREGUÉ PARA QUE KM LOGUEE ID DE LA CPU QUE LE SOLICITÓ CTX
    enviar_paquete(paquete, cpu->socket_kernel_memory, cpu->logger);
    eliminar_paquete(paquete);

    sem_wait(&sem_contexto_recibido);

    t_contexto* contexto_recibido = buffer_contexto;
    
    buffer_contexto = NULL; 

    if (contexto_recibido != NULL) {
        log_debug(cpu->logger, "Contexto recibido: PID=%d, PC=%u", contexto_recibido->pid, contexto_recibido->registros.PC);
    } else {
        log_error(cpu->logger, "Error: El buffer_contexto llegó nulo");
    }
    /*
    if(cod_op == CONTEXT_RESPONSE) {
        contexto_recibido = malloc(sizeof(t_contexto));
        
        //pid
        contexto_recibido->pid = *(int*)list_get(respuesta, 1);

        //registros
        contexto_recibido->registros.PC = *(uint32_t*)list_get(respuesta, 2); 
        contexto_recibido->registros.AX = *(uint8_t*)list_get(respuesta, 3);
        contexto_recibido->registros.BX = *(uint8_t*)list_get(respuesta, 4);
        contexto_recibido->registros.CX = *(uint8_t*)list_get(respuesta, 5);
        contexto_recibido->registros.DX = *(uint8_t*)list_get(respuesta, 6);
        contexto_recibido->registros.EAX = *(uint32_t*)list_get(respuesta, 7);
        contexto_recibido->registros.EBX = *(uint32_t*)list_get(respuesta, 8);
        contexto_recibido->registros.ECX = *(uint32_t*)list_get(respuesta, 9);
        contexto_recibido->registros.EDX = *(uint32_t*)list_get(respuesta, 10);
        contexto_recibido->registros.SI = *(uint32_t*)list_get(respuesta, 11);
        contexto_recibido->registros.DI = *(uint32_t*)list_get(respuesta, 12);

        //inicio tabla de segmentos
        contexto_recibido->tabla_segmentos = list_create();

        int cantidad_segmentos = *(int*)list_get(respuesta, 13);

        int offset = 14; 

        for(int i = 0; i < cantidad_segmentos; i++) {
            t_segmento* nuevo_segmento = malloc(sizeof(t_segmento));
            
            nuevo_segmento->id_segmento = *(int*)list_get(respuesta, offset);
            nuevo_segmento->base = *(uint32_t*)list_get(respuesta, offset + 1);
            nuevo_segmento->limite = *(uint32_t*)list_get(respuesta, offset + 2);
            nuevo_segmento->memory_stick_id = *(int*)list_get(respuesta, offset + 3);

            list_add(contexto_recibido->tabla_segmentos, nuevo_segmento);
            offset += 4; // Avanzamos 4 índices para el próximo segmento
        }
        log_debug(cpu->logger, "Contexto recibido: PID=%d, PC=%u, Segmentos=%d", contexto_recibido->pid, contexto_recibido->registros.PC, cantidad_segmentos);
        } else {
        log_warning(cpu->logger, "Código de operación inesperado en respuesta de Kernel Memory: %d", cod_op);
    }
*/
    return contexto_recibido;
}

void ciclo_de_instruccion(t_cpu *cpu,t_contexto* contexto) {
    int ejecutando = 1;// para mantener el ciclo corriendo

    while(ejecutando){

        //FETCH
        log_info(cpu->logger, "##PID: %d - FETCH - Program Counter: %d", contexto->pid, contexto->registros.PC);

        char* cadena_leida = fetch_instruccion(cpu, contexto);

        //sumo uno al PC
        if(cadena_leida != NULL){
            contexto ->registros.PC +=1;
        }
        
        //DECODE
        t_instruccion_decodificada instruccion_actual = decodificar_instruccion(cpu, cadena_leida);
        
        log_info(cpu->logger, "## PID: %d - Ejecutando: %s", contexto->pid, cadena_leida);

        //EXECUTE

        //execute(cpu, contexto, instruccion_actual);

        int estado_ejecucion = execute(cpu, contexto, instruccion_actual);

        if (estado_ejecucion == 0) { //SEGMENTATION FAULT
            log_warning(cpu->logger, "Segmentation Fault detectado en PID %d. Abortando.", contexto->pid);
            
            //aviso a KM y KS
            enviar_contexto_a_memoria(cpu, contexto);
            devolver_proceso_interrumpido(cpu, contexto->pid, SEG_FAULT); 
            
            ejecutando = 0; 
        }

        //  SI FUE UNA SYSCALL, EL PROCESO SE DESALOJA. CORTAMOS EL CICLO.
        else if (instruccion_actual.identificador_operacion >= INST_MUTEX_CREATE && 
            instruccion_actual.identificador_operacion <= INST_EXIT) {
            ejecutando = 0;
        }
        //  SI NO FUE SYSCALL, CHEQUEAMOS SI HAY INTERRUPCIONES PENDIENTES
        else  //CHECK INTERRUPT 
        if (hay_interrupcion_pendiente(cpu->socket_kernel_scheduler)) {
            
            t_list* paquete_interrupcion = recibir_paquete(cpu->socket_kernel_scheduler);
            
            if (paquete_interrupcion) {
                //leo el codigo de interrupción
                int codigo_interrupcion = *(int*)list_get(paquete_interrupcion, 0);

                //leo el pid
                int pid_interrumpido = *(int*)list_get(paquete_interrupcion, 1);

                if (pid_interrumpido == contexto->pid) { //chequeo que sea el PID correcto
                    log_info(cpu->logger, "## Interrupción recibida");

                    enviar_contexto_a_memoria(cpu, contexto);

                    if (codigo_interrupcion == PROCESO_DESALOJADO_QUANTUM) {
                        devolver_proceso_interrumpido(cpu, contexto->pid, PROCESO_DESALOJADO_QUANTUM);
                    } 
                    else if (codigo_interrupcion == PROCESO_DESALOJADO_PRIORIDAD) {
                        devolver_proceso_interrumpido(cpu, contexto->pid, PROCESO_DESALOJADO_PRIORIDAD);
                    }
                    else if (codigo_interrupcion == PROCESO_DESALOJADO_COMPACTACION) {
                        devolver_proceso_interrumpido(cpu, contexto->pid, PROCESO_DESALOJADO_COMPACTACION);
                    }
                    else {
                        log_warning(cpu->logger, "Interrupción desconocida (%d). Se desaloja por precaución.", codigo_interrupcion);
                        devolver_proceso_interrumpido(cpu, contexto->pid, codigo_interrupcion); 
                    }

                    ejecutando = 0; 
                } else {
                    log_debug(cpu->logger, "Interrupción descartada. Iba dirigida al PID %d, pero está ejecutando el PID %d", pid_interrumpido, contexto->pid);
                }
                list_destroy_and_destroy_elements(paquete_interrupcion, free);
            }
        }

        //limpieza
        free(cadena_leida);
        if(instruccion_actual.nombre_operacion) free(instruccion_actual.nombre_operacion);
        if(instruccion_actual.argumento_operando_destino) free(instruccion_actual.argumento_operando_destino);
        if(instruccion_actual.argumento_operando_origen) free(instruccion_actual.argumento_operando_origen);

     
    }
} 

char* fetch_instruccion(t_cpu* cpu, t_contexto* contexto) {

    t_paquete* paquete = crear_paquete(PETICION_INSTRUCCION, crear_buffer());
    agregar_a_paquete(paquete, &contexto->pid, sizeof(int));
    agregar_a_paquete(paquete, &contexto->registros.PC, sizeof(uint32_t)); 
    enviar_paquete(paquete, cpu->socket_kernel_memory, cpu->logger);
    eliminar_paquete(paquete);

    sem_wait(&sem_instruccion_recibida);

    char* instruccion_leida = buffer_instruccion;
    buffer_instruccion = NULL; 
   
    return instruccion_leida;
}

t_instruccion_decodificada decodificar_instruccion(t_cpu* cpu, char* cadena_instruccion_texto) {
    t_instruccion_decodificada instruccion_formateada;
    
    instruccion_formateada.identificador_operacion = INST_DESCONOCIDA;
    instruccion_formateada.nombre_operacion = NULL;
    instruccion_formateada.argumento_operando_destino = NULL;
    instruccion_formateada.argumento_operando_origen = NULL;

    if (!cadena_instruccion_texto) {
        log_error(cpu->logger, "Error: Se recibió una cadena de instrucción nula.");
        return instruccion_formateada;
    }

    char** tokens_de_la_linea = string_split(cadena_instruccion_texto, " ");
    
    if (tokens_de_la_linea[0] != NULL) {
        instruccion_formateada.nombre_operacion = strdup(tokens_de_la_linea[0]);
        
        if (strcmp(tokens_de_la_linea[0], "NOOP") == 0) instruccion_formateada.identificador_operacion = INST_NOOP;
        else if (strcmp(tokens_de_la_linea[0], "SET") == 0) instruccion_formateada.identificador_operacion = INST_SET;
        else if (strcmp(tokens_de_la_linea[0], "MOV_IN") == 0) instruccion_formateada.identificador_operacion = INST_MOV_IN;
        else if (strcmp(tokens_de_la_linea[0], "MOV_OUT") == 0) instruccion_formateada.identificador_operacion = INST_MOV_OUT;
        else if (strcmp(tokens_de_la_linea[0], "SUM") == 0) instruccion_formateada.identificador_operacion = INST_SUM;
        else if (strcmp(tokens_de_la_linea[0], "SUB") == 0) instruccion_formateada.identificador_operacion = INST_SUB;
        else if (strcmp(tokens_de_la_linea[0], "JNZ") == 0) instruccion_formateada.identificador_operacion = INST_JNZ;
        else if (strcmp(tokens_de_la_linea[0], "COPY_MEM") == 0) instruccion_formateada.identificador_operacion = INST_COPY_MEM;
        else if (strcmp(tokens_de_la_linea[0], "MUTEX_CREATE") == 0) instruccion_formateada.identificador_operacion = INST_MUTEX_CREATE;
        else if (strcmp(tokens_de_la_linea[0], "MUTEX_LOCK") == 0) instruccion_formateada.identificador_operacion = INST_MUTEX_LOCK;
        else if (strcmp(tokens_de_la_linea[0], "MUTEX_UNLOCK") == 0) instruccion_formateada.identificador_operacion = INST_MUTEX_UNLOCK;
        else if (strcmp(tokens_de_la_linea[0], "MEM_ALLOC") == 0) instruccion_formateada.identificador_operacion = INST_MEM_ALLOC;
        else if (strcmp(tokens_de_la_linea[0], "MEM_FREE") == 0) instruccion_formateada.identificador_operacion = INST_MEM_FREE;
        else if (strcmp(tokens_de_la_linea[0], "SLEEP") == 0) instruccion_formateada.identificador_operacion = INST_SLEEP;
        else if (strcmp(tokens_de_la_linea[0], "STDOUT") == 0) instruccion_formateada.identificador_operacion = INST_STDOUT;
        else if (strcmp(tokens_de_la_linea[0], "STDIN") == 0) instruccion_formateada.identificador_operacion = INST_STDIN;
        else if (strcmp(tokens_de_la_linea[0], "INIT_PROC") == 0) instruccion_formateada.identificador_operacion = INST_INIT_PROC;
        else if (strcmp(tokens_de_la_linea[0], "EXIT") == 0) instruccion_formateada.identificador_operacion = INST_EXIT;

        if (tokens_de_la_linea[1] != NULL) {
            instruccion_formateada.argumento_operando_destino = strdup(tokens_de_la_linea[1]);
            
            if (tokens_de_la_linea[2] != NULL) {
                instruccion_formateada.argumento_operando_origen = strdup(tokens_de_la_linea[2]);
            }
        }
    } else {
        log_error(cpu->logger, "Error: La etapa Decode no pudo identificar la operación.");
    }

    string_array_destroy(tokens_de_la_linea);

    return instruccion_formateada;
}

bool hay_interrupcion_pendiente(int socket_fd) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int result = select(socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    return (result > 0 && FD_ISSET(socket_fd, &read_fds));
}

void enviar_contexto_a_memoria(t_cpu* cpu, t_contexto* contexto) {
   
    t_paquete* paquete = crear_paquete(ACTUALIZAR_CONTEXTO, crear_buffer());

    agregar_a_paquete(paquete, &(contexto->pid), sizeof(int));
    agregar_a_paquete(paquete, &(contexto->registros.PC), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros.AX), sizeof(uint8_t));
    agregar_a_paquete(paquete, &(contexto->registros.BX), sizeof(uint8_t));
    agregar_a_paquete(paquete, &(contexto->registros.CX), sizeof(uint8_t));
    agregar_a_paquete(paquete, &(contexto->registros.DX), sizeof(uint8_t));
    agregar_a_paquete(paquete, &(contexto->registros.EAX), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros.EBX), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros.ECX), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros.EDX), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros.SI), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros.DI), sizeof(uint32_t));

    if (enviar_paquete(paquete, cpu->socket_kernel_memory, cpu->logger) == -1) {
        log_error(cpu->logger, "Error al enviar el contexto actualizado a Kernel Memory para PID %d", contexto->pid);
    } else {
        log_debug(cpu->logger, "Contexto actualizado enviado a Kernel Memory para PID %d", contexto->pid);
    }

    eliminar_paquete(paquete);
}

void devolver_proceso_interrumpido(t_cpu* cpu, int pid, op_code motivo_desalojo) {

    t_paquete* paquete = crear_paquete(motivo_desalojo, crear_buffer());

    agregar_a_paquete(paquete, &pid, sizeof(int));

    if (enviar_paquete(paquete, cpu->socket_kernel_scheduler, cpu->logger) == -1) {
        log_error(cpu->logger, "Error al devolver el proceso %d al Kernel Scheduler", pid);
    } else {
        log_info(cpu->logger, "Proceso %d devuelto al Kernel Scheduler (Motivo: %d)", pid, motivo_desalojo);
    }

    eliminar_paquete(paquete);
}

int execute(t_cpu* cpu, t_contexto* contexto, t_instruccion_decodificada instruccion) {
    int estado_ejecucion = 1;
    
    switch (instruccion.identificador_operacion) {
        case INST_NOOP:
            ejecutar_NOOP(cpu, contexto);
            break;
            
        case INST_SET: {
            uint32_t valor = (uint32_t)atoi(instruccion.argumento_operando_origen);
            ejecutar_SET(cpu,&(contexto->registros), instruccion.argumento_operando_destino, valor);
            break;
        }
            
        case INST_SUM:
            ejecutar_SUM(cpu,&(contexto->registros), instruccion.argumento_operando_destino, instruccion.argumento_operando_origen);
            break;
            
        case INST_SUB:
            ejecutar_SUB(cpu,&(contexto->registros), instruccion.argumento_operando_destino, instruccion.argumento_operando_origen);
            break;

        case INST_JNZ: {
            uint32_t nueva_instruccion = (uint32_t)atoi(instruccion.argumento_operando_origen);
            ejecutar_JNZ(cpu,&(contexto->registros), instruccion.argumento_operando_destino, nueva_instruccion);
            break;
        }
        
        // instrucciones de memoria
        case INST_MOV_IN:
            estado_ejecucion = ejecutar_MOV_IN(cpu, contexto, instruccion.argumento_operando_destino);
            break;

        case INST_MOV_OUT:
            estado_ejecucion = ejecutar_MOV_OUT(cpu, contexto, instruccion.argumento_operando_destino);
            break;

        case INST_COPY_MEM:
            estado_ejecucion = ejecutar_COPY_MEM(cpu, contexto, instruccion.argumento_operando_destino);
            break;

        //syscalls
        case INST_MUTEX_CREATE:
        case INST_MUTEX_LOCK:
        case INST_MUTEX_UNLOCK:
        case INST_MEM_ALLOC:
        case INST_MEM_FREE:
        case INST_SLEEP:
        case INST_STDOUT:
        case INST_STDIN:
        case INST_INIT_PROC:
        case INST_EXIT:
            // aca deberia avisarle a kernel que ejecute la syscall
            estado_ejecucion=ejecutar_SYSCALL(cpu, contexto, instruccion);
            break;

        case INST_DESCONOCIDA:
        default:
            log_debug(cpu->logger, "Instrucción desconocida o no implementada: %s", instruccion.nombre_operacion);
            break;
    }
    return estado_ejecucion;
}
