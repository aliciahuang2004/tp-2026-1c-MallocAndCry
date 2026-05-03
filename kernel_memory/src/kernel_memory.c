
#include "kernel_memory.h"

t_list *lista_contextos;
pthread_mutex_t mutex_lista_contextos = PTHREAD_MUTEX_INITIALIZER;

t_list* lista_ms;
pthread_mutex_t mutex_lista_ms = PTHREAD_MUTEX_INITIALIZER;

void esperarConexiones(t_kernel_memory* kernelMemory, int kernel_memory_fd){
    while (1) {

        pthread_t thread;

        int fd_conexion_kernel_memory = esperar_cliente(kernel_memory_fd);
        log_debug(kernelMemory->logger, "Esperando que se conecte un cliente");

        t_hacerConnect* datosConexion = malloc(sizeof(t_hacerConnect));
        datosConexion->logger = kernelMemory->logger;

        datosConexion->socket_conexion = fd_conexion_kernel_memory;
        datosConexion->km = kernelMemory;

        int err= pthread_create(&thread,
                        NULL,
                        atender_conexion, // va a llamar a atender conexion pasando parametros
                        datosConexion); // de este struct
        if (err != 0){
            log_debug(kernelMemory->logger, "Hubo un problema al crear el hilo");
        }
        pthread_detach(thread);
        
    }
}

void* atender_conexion(void* arg) {
    t_hacerConnect* datos = (t_hacerConnect*) arg;

    int socket_cliente = datos->socket_conexion;
    t_log* logger = datos->logger;
    t_kernel_memory* km = datos->km;

    log_info(logger, "Nuevo hilo atendiendo conexión en socket %d", socket_cliente);

    
    while (1) {

    
        t_list* paquete = recibir_paquete(socket_cliente);
        
        if (!paquete) {
            log_error(logger, "Error al recibir paquete o cliente desconectado en socket %d.", socket_cliente);
            break; // Salimos del bucle si el cliente se cae
        }

        int codigo_operacion = *(int*) list_get(paquete, 0);

        switch (codigo_operacion) {
            case CPU_HANDSHAKE:
                int cpu_id = *(int *)list_get(paquete, 1);
                log_info(logger, "CPU ID:%d conectada en socket %d", cpu_id, socket_cliente);
                // CODIGO CPU
                break;

            case MEMORY_STICK_HANDSHAKE:
                int ms_id = *(int*)list_get(paquete,1);
                int ms_tamano = *(int*)list_get(paquete,2);

                log_info(logger, "[Socket %d] MEMORY STICK conectado - ID:%d Tamaño:%d bytes", socket_cliente,ms_id,ms_tamano);
                //AGREGA EN LA LISTA DE MSs
                t_ms_info* ms_info = malloc(sizeof(t_ms_info));
                ms_info->id = ms_id;
                ms_info->tamano = ms_tamano;
                ms_info->socket = socket_cliente;

                pthread_mutex_lock(&mutex_lista_ms);
                list_add(lista_ms,ms_info);
                pthread_mutex_unlock(&mutex_lista_ms);

                log_info(logger,"Memory sticks conectados: %d",list_size(lista_ms));
                
                //***AVISO A KERNEL SCHEDULER TAMAÑO DE NUEVO MEMORY STICK 
                t_paquete *respuesta = crear_paquete(NUEVO_MEMORY_STICK, crear_buffer());
                agregar_a_paquete(respuesta, &ms_tamano, sizeof(int));
                enviar_paquete(respuesta, km->socket_kernel_scheduler, logger);
                eliminar_paquete(respuesta);
                // CODIGO MEMORY STICK
                break;

            case KERNEL_SCHEDULER_HANDSHAKE:
                log_info(logger, "[Socket %d] Operación KERNEL SCHEDULER recibida", socket_cliente);
                km->socket_kernel_scheduler = socket_cliente;
                // CODIGO KERNEL SCHEDULER
                 break; 

            case SWAP_HANDSHAKE: 
                log_info(logger, "[Socket %d] Operación SWAP recibida", socket_cliente);
                // CODIGO SWAP
                break;

            case CREACION_DE_PROCESO: // Asegurate de que este en protocolo.h
            {
                // 1. Extraemos los datos que mandó el Scheduler en el orden acordado
                // Índice 1: PID (int)
                // Índice 2: Path (string)
                int pid_nuevo = *(int*) list_get(paquete, 1);
                char* path_relativo = (char*) list_get(paquete, 2);

                log_info(logger, "## Creación de Proceso - PID: %d", pid_nuevo); 

                inicializar_proceso_memoria(pid_nuevo, path_relativo, km);

                // Según el PDF, acá también deberías inicializar los registros en 0
                // inicializar_contexto_ejecucion(pid_nuevo, km); 

                int resultado = crear_CTX(pid_nuevo);//************AGREGA CONTEXTO EN LA LISTA
                debug_lista_contextos(lista_contextos);
                
                 if (resultado == 0)
                { //************************ENVIO CONFIRMACION O ERROR DE CREACION DE PROCESO A KS********************
                t_paquete *respuesta = crear_paquete(CREACION_DE_PROCESO_OK, crear_buffer());
                enviar_paquete(respuesta, socket_cliente, logger);
                }
                else
                {
                log_error(logger, "ERROR AL EJECUTAR CREACION_DE_PROCESO PARA PID %d", pid_nuevo);
                t_paquete *error = crear_paquete(CREACION_DE_PROCESO_ERROR, crear_buffer());
                enviar_paquete(error, socket_cliente, logger);
                eliminar_paquete(error);
                }

                break;
            }

            case PETICION_INSTRUCCION: 
            {
                int pid_recibido = *(int*) list_get(paquete, 1);
                int pc_recibido  = *(int*) list_get(paquete, 2);
                char* instruccion = obtener_instruccion(pid_recibido, pc_recibido, km);

                if (instruccion != NULL) {
                    t_buffer* buffer_respuesta = crear_buffer();
                    t_paquete* paquete_respuesta = crear_paquete(RESPUESTA_INSTRUCCION, buffer_respuesta);
                    
                    agregar_a_paquete(paquete_respuesta, instruccion, strlen(instruccion) + 1);
                    enviar_paquete(paquete_respuesta, socket_cliente, logger);
                    eliminar_paquete(paquete_respuesta);
                    free(instruccion);
                } else {
                    t_buffer* buffer_error = crear_buffer();
                    t_paquete* paquete_error = crear_paquete(ERROR_INSTRUCCION, buffer_error);
                    enviar_paquete(paquete_error, socket_cliente, logger);
                    eliminar_paquete(paquete_error);
                }
                break;
            }

            default:
                log_error(logger, "[Socket %d] Código de operación desconocido: %d", socket_cliente, codigo_operacion);
                break;

             case REQUEST_CONTEXTO:
        {
            int pid_solicitado = *(int *)list_get(paquete, 1);
            int cpu_id = *(int *)list_get(paquete, 2);

            log_debug(logger, "CPU ID:%d solicitó contexto para PID:%d", cpu_id, pid_solicitado);

            pthread_mutex_lock(&mutex_lista_contextos);
            Contexto *ctx_encontrado = NULL;
            for (int i = 0; i < list_size(lista_contextos); i++)
            {
                Contexto *ctx = (Contexto *)list_get(lista_contextos, i);
                if (ctx->pid == pid_solicitado)
                {
                    ctx_encontrado = ctx;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex_lista_contextos);

            if (ctx_encontrado != NULL)
            { //***********ENVÍO CTX COMPLETO,PROBAR PARA VER SI ES NECESARIO ENVIAR TODO ESTO
                t_paquete *respuesta = crear_paquete(CONTEXT_RESPONSE, crear_buffer());
                agregar_a_paquete(respuesta, &ctx_encontrado->pid, sizeof(int));
                agregar_a_paquete(respuesta, &ctx_encontrado->PC, sizeof(uint32_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->AX, sizeof(uint8_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->BX, sizeof(uint8_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->CX, sizeof(uint8_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->DX, sizeof(uint8_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->EAX, sizeof(uint32_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->EBX, sizeof(uint32_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->ECX, sizeof(uint32_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->EDX, sizeof(uint32_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->SI, sizeof(uint32_t));
                agregar_a_paquete(respuesta, &ctx_encontrado->DI, sizeof(uint32_t));
                enviar_paquete(respuesta, socket_cliente, logger);
                eliminar_paquete(respuesta);

                log_info(logger, "Contexto enviado - PID:%d PC:%d", ctx_encontrado->pid, ctx_encontrado->PC);
            }
            else
            {
                log_error(logger, "No se encontró contexto para PID %d", pid_solicitado);
                t_paquete *error = crear_paquete(CONTEXT_ERROR, crear_buffer());
                enviar_paquete(error, socket_cliente, logger);
                eliminar_paquete(error);
            }

            break;
        }
        case ESCRITURA_DE_DATOS: ///***STDIN
        {
        }
        break;
        case LECTURA_DE_DATOS: ///***STDOUT
        {
        }
        break;
        case FINALIZAR_PROCESO: ///***EXIT
        {
            int pid_recibido = *(int *)list_get(paquete, 1);
            // BORRAR CTX,TABLA DE SEG DEL PROC,ENTRADAS DEL PROC EN LA TABLA DE SEG,PATH A INSTRUCCIONES? AVISAR A KS AL FINALIZAR TODO ESTO?
        }
        break;
        case SUSPENSION_DE_PROCESO:
        {
            // SWAP Y MEMORY STICK
        }
        break;
        case DESUSPENSION_DE_PROCESO:
        {/// SWAP + MS
        }
        break;
        case ELIMINACION_DE_SEGMENTO: ///***MEM_FREE??????
        {
            int pid_recibido = *(int *)list_get(paquete, 1);
            int id_seg_recibido = *(int *)list_get(paquete, 2);
            // int eliminacion_de_seg(pid_recibido,id_seg_recibido);
            // ELIMINA DE LA TABLA DEL PROCESO Y DE LA TABLA DE SEG ESTE ID SEG
        }
        break;
        case CREACION_DE_SEGMENTO: ///***MEM_ALLOC
        
        {   //RECIBO PAQUETE DE PARTE DE KERNEL SCHEDULER CON LOS SIG DATOS
            int pid_recibido = *(int *)list_get(paquete, 1);
            int id_seg_recibido = *(int *)list_get(paquete, 2);
            int tamano_recibido = *(int *)list_get(paquete, 3);

            //creacion_de_segmento(pid_recibido,id_seg_recibido,tamano_recibido);
           
        }
        break;

        }

      
        list_destroy_and_destroy_elements(paquete, free);
    }

  
    close(socket_cliente);
    log_info(logger, "Conexión cerrada en socket %d", socket_cliente);

    free(datos);
    return NULL;
}

t_kernel_memory* iniciar_kernelMemory(char* argv){

    t_kernel_memory* kernelMemory = malloc(sizeof(t_kernel_memory));

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
    
    log_debug(kernelMemory->logger, "Kernel Memory inicializado correctamentew");
    
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

void inicializar_proceso_memoria(int pid, char* path_relativo, t_kernel_memory* km) {
    char* path_absoluto = string_new();
    string_append(&path_absoluto, km->scripts_basePath);
    
    if (!string_ends_with(km->scripts_basePath, "/") && !string_starts_with(path_relativo, "/")) {
        string_append(&path_absoluto, "/");
    }
    string_append(&path_absoluto, path_relativo);

    char pid_str[10];
    sprintf(pid_str, "%d", pid);

    dictionary_put(km->paths_por_pid, pid_str, path_absoluto);
    log_info(km->logger, "Asignado path absoluto [%s] al PID: %d", path_absoluto, pid);
}

char* obtener_instruccion(int pid, int pc, t_kernel_memory* km) {
    char pid_str[10];
    sprintf(pid_str, "%d", pid);

    
    char* path_absoluto = dictionary_get(km->paths_por_pid, pid_str);
    if (path_absoluto == NULL) {
        log_error(km->logger, "No se encontro un archivo asociado al PID %d", pid);
        return NULL;
    }

   
    usleep(km->instruction_delay * 1000); 

   
    FILE* archivo = fopen(path_absoluto, "r");
    if (archivo == NULL) {
        log_error(km->logger, "Error al abrir el archivo de pseudocodigo: %s", path_absoluto);
        return NULL;
    }

    char* linea = NULL;
    size_t len = 0;
    ssize_t read;
    int linea_actual = 0;
    char* instruccion_encontrada = NULL;

  
    while ((read = getline(&linea, &len, archivo)) != -1) {
        if (linea_actual == pc) {
           
            if (linea[read - 1] == '\n') {
                linea[read - 1] = '\0';
            }
            
            instruccion_encontrada = string_duplicate(linea);
            break;
        }
        linea_actual++;
    }

    free(linea);
    fclose(archivo);

    
    if (instruccion_encontrada != NULL) {
        log_info(km->logger, "## Obtener instrucción - PID: %d - Instrucción: %s", pid, instruccion_encontrada);
    } else {
        log_warning(km->logger, "No se encontro la instruccion para el PC %d ", pc);
    }

    return instruccion_encontrada;
}


int crear_CTX(int pid)
{
    Contexto* ctx = malloc(sizeof(Contexto));
    if (ctx == NULL) 
    return -1;

    ctx->pid = pid;
    ctx->PC = 0;
    ctx->AX = 0;
    ctx->BX = 0;
    ctx->CX = 0;
    ctx->DX = 0;
    ctx->EAX = 0;
    ctx->EBX = 0;
    ctx->ECX = 0;
    ctx->EDX = 0;
    ctx->SI = 0;
    ctx->DI = 0;
    pthread_mutex_lock(&mutex_lista_contextos);
    list_add(lista_contextos, ctx);
    pthread_mutex_unlock(&mutex_lista_contextos);
    return 0;
}

void enviar_operacion(int socket_cliente, op_code codigo) {
    send(socket_cliente, &codigo, sizeof(op_code), 0);
}

//**************SOLO PARA VER SI LOS REGISTROS SE CARGARON CORRECTAMENTE
void debug_lista_contextos(t_list* lista) {
    pthread_mutex_lock(&mutex_lista_contextos);
    printf("\033[1;33m=== LISTA CONTEXTOS (size: %d) ===\033[0m\n", list_size(lista));
    for (int i = 0; i < list_size(lista); i++) {
        Contexto* ctx = (Contexto*)list_get(lista, i);
        printf("\033[1;33m--- Nodo %d ---\033[0m\n", i);
        printf("\033[1;33m  PID : %d\033[0m\n",ctx->pid);
        printf("\033[1;33m  PC  : %u\033[0m\n",ctx->PC);
        printf("\033[1;33m  AX  : %u  | BX : %u  | CX : %u  | DX : %u\033[0m\n",ctx->AX, ctx->BX, ctx->CX, ctx->DX);
        printf("\033[1;33m  EAX : %u | EBX: %u | ECX: %u | EDX: %u\033[0m\n",ctx->EAX, ctx->EBX, ctx->ECX, ctx->EDX);
        printf("\033[1;33m  SI  : %u | DI : %u\033[0m\n",ctx->SI, ctx->DI);
    }
    printf("\033[1;33m================================\033[0m\n");
    pthread_mutex_unlock(&mutex_lista_contextos);
}