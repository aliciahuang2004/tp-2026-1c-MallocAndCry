#include "conexiones.h"
#include "kernel_memory.h"
#include "contextos.h"
#include "instrucciones.h"
#include "procesos.h"
#include "memory_stick.h"
#include "estructuras.h"
#include "huecos.h"
#include "compactacion.h"
#include "segmentos.h"
#include "errno.h"

uint32_t memoria_total = 0;

void esperarConexiones(t_kernel_memory* kernelMemory, int kernel_memory_fd){
    while (1) {

        pthread_t thread;

        int fd_conexion_kernel_memory = esperar_cliente(kernel_memory_fd);
        log_debug(kernelMemory->logger, "Esperando que se conecte un cliente");
        if (fd_conexion_kernel_memory == -1) {
            log_error(kernelMemory->logger, "Error en accept(): %s", strerror(errno));
            break;
        }
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

void agregar_cpu_conectada(int cpu_id, int socket_cliente)
{
    t_cpu* cpu = malloc(sizeof(t_cpu));
    if(cpu == NULL)
        return;
    memset(cpu, 0, sizeof(t_cpu));
    cpu->id     = cpu_id;
    cpu->socket = socket_cliente;

    pthread_mutex_lock(&mutex_cpus_conectadas);
    list_add(cpus_conectadas, cpu);
    pthread_mutex_unlock(&mutex_cpus_conectadas);
}

void avisar_cpus_conectadas(int ms_id, char* ms_puerto, char* ms_ip,t_log* logger,t_resultado_hueco resultado)
{
    pthread_mutex_lock(&mutex_cpus_conectadas);

    int total = list_size(cpus_conectadas);
    log_debug(logger, "Avisando nuevo MS ID:%d a %d CPUs conectadas", ms_id, total);

    for(int i = 0; i < total; i++) {
        t_cpu* cpu = list_get(cpus_conectadas, i);

        t_paquete* paquete = crear_paquete(MS_NUEVO_CPU, crear_buffer());
        agregar_a_paquete(paquete, &ms_id,sizeof(int));
        agregar_a_paquete(paquete, ms_puerto,strlen(ms_puerto) + 1);
        agregar_a_paquete(paquete, ms_ip,strlen(ms_ip) + 1);
        agregar_a_paquete(paquete, &resultado.base,sizeof(uint32_t));
        agregar_a_paquete(paquete, &resultado.limite,sizeof(uint32_t));
        enviar_paquete(paquete, cpu->socket, logger);
        log_debug(logger, "Antes de destruir paquete: ip='%s' puerto='%s'", ms_ip, ms_puerto);
        eliminar_paquete(paquete);

        log_debug(logger, "  [%d/%d] Aviso enviado a CPU ID:%d socket:%d",i + 1, total, cpu->id, cpu->socket);
    }
    pthread_mutex_unlock(&mutex_cpus_conectadas);
}

void* atender_conexion(void* arg) {
    t_hacerConnect* datos = (t_hacerConnect*) arg;

    int socket_cliente = datos->socket_conexion;
    t_log* logger = datos->logger;
    t_kernel_memory* km = datos->km;

    log_debug(logger, "Nuevo hilo atendiendo conexión en socket %d", socket_cliente);

    bool es_cpu = false;
          while (1) {
            t_list* paquete = recibir_paquete(socket_cliente);
            
            if (!paquete) { // Detección instantánea de desconexión
                log_error(logger, "Cliente desconectado en socket %d", socket_cliente);
                
                t_ms_info* ms = buscar_ms_por_socket(socket_cliente);
                if (ms != NULL) {
                    manejar_desconexion_memory_stick(ms, km, logger);
                    
                    // Notificamos error a TODOS los hilos que esperaban a este MS
                    pthread_mutex_lock(&ms->mutex_socket);
                    ms->ultimo_codigo_op = ERROR_CONEXION_MS; 
                    ms->respuesta_lista = true;
                    pthread_cond_broadcast(&ms->cond_respuesta); // BROADCAST es mejor aquí
                    pthread_mutex_unlock(&ms->mutex_socket);
                }
                break; 
            }

              
        int codigo_operacion = *(int*) list_get(paquete, 0);

        switch (codigo_operacion) {
            case CPU_HANDSHAKE:
                es_cpu = true;
                int cpu_id = *(int *)list_get(paquete, 1);
                log_debug(logger, "[Socket %d] Conexión con CPU ID:%d exitosa", socket_cliente,cpu_id);
                log_info(logger," ## CPU <%d> Conectada ",cpu_id);
                agregar_cpu_conectada(cpu_id, socket_cliente);
                //envia los datos de los ms ya conectados antes que esta cpu
                enviar_ms_a_cpu(socket_cliente,logger);
                t_paquete *respuesta = crear_paquete(SEG_MAX_SIZE, crear_buffer());
                agregar_a_paquete(respuesta, &km->segment_max_size, sizeof(int));
                enviar_paquete(respuesta, socket_cliente, logger);
                eliminar_paquete(respuesta);
                
                break;

            case MEMORY_STICK_HANDSHAKE: 
            { 
                int ms_id = *(int *)list_get(paquete, 1);
                uint32_t ms_tamano = *(int *)list_get(paquete, 2);
                char* ms_puerto = (char*) list_get(paquete, 3);
                char* ms_ip = (char*)list_get(paquete, 4);

                log_debug(logger,"[Socket %d] NUEVO MEMORY STICK conectado - ID:%d Tamaño:%d bytes Puerto:%s",socket_cliente,ms_id,ms_tamano,ms_puerto);
                log_info(logger," ## Memory Stick de <%d> bytes Conectada ",ms_tamano);

                uint32_t base_nuevo_ms = aumentar_memoria_total(ms_tamano);

                int resultado = nuevo_memory_stick(ms_id, ms_tamano, socket_cliente);
                if (!resultado) {
                    log_error(logger, "Error al registrar el nuevo Memory Stick");
                    break;   
                }

                int rta = guardar_ms_conexion(ms_id, ms_ip, ms_puerto);
                if (!rta) {
                    log_error(logger, "Error al guardar la información de conexión del Memory Stick");
                    break;
                }
                
                t_resultado_hueco r = agregar_hueco_libre(base_nuevo_ms, ms_tamano);//acá obtengo base y limite global del ms
                loguear_huecos(logger);
                agregar_posicion_ms(r,ms_id,logger);//ACA GUARDA BASE Y LIMITE GLOBAL DE LOS MS,FALTA PROBAR.ACA KM BUSCA A QUÉ MS ENVIAR PETICION DE ESCRITURA,LECTURA
                avisar_cpus_conectadas(ms_id,ms_puerto,ms_ip,logger,r); 

                pthread_mutex_lock(&mutex_memoria_total);
                log_debug(logger, "Memoria total disponible: %u bytes", memoria_total);
                uint32_t copia_memoria_total = memoria_total;
                pthread_mutex_unlock(&mutex_memoria_total);

                //AVISO A KS QUE HAY MAS MEMORIA DISPONIBLE:
                t_paquete *respuesta = crear_paquete(AUMENTO_DE_MEMORIA, crear_buffer());
                agregar_a_paquete(respuesta,&copia_memoria_total,sizeof(uint32_t));
                enviar_paquete(respuesta,km->socket_kernel_scheduler, logger);
                eliminar_paquete(respuesta);
                 break;                      
            }

            case KERNEL_SCHEDULER_HANDSHAKE:
            
                log_info(logger, " ##  Kernel Scheduler Conectado  -  FD del socekt : <%d> ", socket_cliente);
                km->socket_kernel_scheduler = socket_cliente;
                // CODIGO KERNEL SCHEDULER
                 break; 

            case SWAP_HANDSHAKE: 
                log_debug(logger, "[Socket %d] Operación SWAP recibida", socket_cliente);
                // CODIGO SWAP
                break;

            case CREACION_DE_PROCESO:
            {
                int pid_nuevo = *(int*) list_get(paquete, 1);
                char* path_relativo = (char*) list_get(paquete, 2);

                inicializar_proceso_memoria(pid_nuevo, path_relativo, km);
            
                int resultado = crear_proceso(pid_nuevo);
                
                 if (resultado == 0)
                { 
                t_paquete *respuesta = crear_paquete(CREACION_DE_PROCESO_OK, crear_buffer());   
                agregar_a_paquete(respuesta, &pid_nuevo, sizeof(int));
                enviar_paquete(respuesta, socket_cliente, logger);
                eliminar_paquete(respuesta);
                }
                else
                {
                log_error(logger, "ERROR AL EJECUTAR CREACION_DE_PROCESO PARA PID %d", pid_nuevo);
                t_paquete *error = crear_paquete(CREACION_DE_PROCESO_ERROR, crear_buffer());
                agregar_a_paquete(error, &pid_nuevo, sizeof(int));
                enviar_paquete(error, socket_cliente, logger);
                eliminar_paquete(error);
                }
                log_info(logger, " ## PID: <%d> - Proceso Creado ", pid_nuevo); 
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

            case REQUEST_CONTEXTO:
            {
                int pid_solicitado = *(int *)list_get(paquete, 1);
                int cpu_id         = *(int *)list_get(paquete, 2);
                log_debug(logger, "CPU ID:%d solicitó contexto para PID:%d", cpu_id, pid_solicitado);

                t_list* tabla_segmentos = NULL;
                t_registros* regs = solicitud_contexto(pid_solicitado, &tabla_segmentos);

                if (regs != NULL) {
                    t_paquete* respuesta = crear_paquete(CONTEXT_RESPONSE, crear_buffer());

                    agregar_a_paquete(respuesta, &pid_solicitado, sizeof(int));

                    agregar_a_paquete(respuesta, &regs->PC,  sizeof(uint32_t));
                    agregar_a_paquete(respuesta, &regs->AX,  sizeof(uint8_t));
                    agregar_a_paquete(respuesta, &regs->BX,  sizeof(uint8_t));
                    agregar_a_paquete(respuesta, &regs->CX,  sizeof(uint8_t));
                    agregar_a_paquete(respuesta, &regs->DX,  sizeof(uint8_t));
                    agregar_a_paquete(respuesta, &regs->EAX, sizeof(uint32_t));
                    agregar_a_paquete(respuesta, &regs->EBX, sizeof(uint32_t));
                    agregar_a_paquete(respuesta, &regs->ECX, sizeof(uint32_t));
                    agregar_a_paquete(respuesta, &regs->EDX, sizeof(uint32_t));
                    agregar_a_paquete(respuesta, &regs->SI,  sizeof(uint32_t));
                    agregar_a_paquete(respuesta, &regs->DI,  sizeof(uint32_t));
                    
                    int cant_segmentos = list_size(tabla_segmentos);
                    agregar_a_paquete(respuesta, &cant_segmentos, sizeof(int));

                    for (int i = 0; i < cant_segmentos; i++) {
                        t_segmento* seg = list_get(tabla_segmentos, i);
                        agregar_a_paquete(respuesta, &seg->id_segmento,    sizeof(int));
                        agregar_a_paquete(respuesta, &seg->base_global,    sizeof(uint32_t));
                        agregar_a_paquete(respuesta, &seg->limite_global,  sizeof(uint32_t));
                        agregar_a_paquete(respuesta, &seg->memory_stick_id,sizeof(int));//sirve a cpu cuando ejecuta mov out ,mov in?
                    }
                    enviar_paquete(respuesta, socket_cliente, logger);
                    eliminar_paquete(respuesta);

                    free(regs);
                    list_destroy_and_destroy_elements(tabla_segmentos, free);
                    log_debug(logger, "Contexto enviado - PID:%d | Segmentos:%d", pid_solicitado, cant_segmentos);
                } else {
                    log_error(logger, "No se encontró contexto para PID %d", pid_solicitado);
                    t_paquete* error = crear_paquete(CONTEXT_ERROR, crear_buffer());
                    enviar_paquete(error, socket_cliente, logger);
                    eliminar_paquete(error);
                }
            }  
            break;
        
           case ESCRITURA_DE_DATOS: 
            { 
                int pid_recibido  = *(int*) list_get(paquete, 1);
                uint32_t direccion_fisica_global = *(uint32_t*) list_get(paquete, 2);
                uint32_t tamano_contenido = *(uint32_t*) list_get(paquete,3); 
                char* contenido_a_escribir = (char*) list_get(paquete, 4);
               

                t_list* lista_fragmentos_temp = calcular_dir_local_ms(direccion_fisica_global,tamano_contenido,logger);

                if (lista_fragmentos_temp != NULL) {
            
                    enviar_fragmentos_escritura(lista_fragmentos_temp, contenido_a_escribir, logger,km);
                    list_destroy_and_destroy_elements(lista_fragmentos_temp, free);
                } else {
                    log_error(logger, "Error de segmentación global para PID:%d", pid_recibido);
                }
                log_info(logger," ## PID: - <%d> - <Escritura> - Dir. Física: <%u> - Tamaño: <%u> ",pid_recibido,direccion_fisica_global,tamano_contenido);
                t_paquete* confirmacion = crear_paquete(ESCRITURA_DE_DATOS_OK, crear_buffer());
                agregar_a_paquete(confirmacion, &pid_recibido, sizeof(int));
                enviar_paquete(confirmacion, km->socket_kernel_scheduler, logger);
                eliminar_paquete(confirmacion);      
            }
            break;
            case LECTURA_DE_DATOS: 
            {   
                int pid_recibido = *(int*) list_get(paquete, 1);             
                uint32_t direccion_fisica_global = *(uint32_t*) list_get(paquete, 2);
                uint32_t tamano = *(uint32_t*) list_get(paquete,3); 

                t_list* lista_fragmentos_temp = calcular_dir_local_ms(direccion_fisica_global,tamano,logger);
  
                if (lista_fragmentos_temp != NULL) {
             
                void* contenido_leido_completo = enviar_fragmentos_lectura(lista_fragmentos_temp, tamano, logger,km);

                if (contenido_leido_completo != NULL) {
                    log_info(logger," ## PID: - <%d> - <Lectura> - Dir. Física: <%u> - Tamaño: <%u> ",pid_recibido,direccion_fisica_global,tamano);
                    t_paquete* respuesta_final = crear_paquete(RTA_LECTURA, crear_buffer());
                    agregar_a_paquete(respuesta_final, &pid_recibido, sizeof(int));
                    agregar_a_paquete(respuesta_final,contenido_leido_completo,tamano);
                    enviar_paquete(respuesta_final,km->socket_kernel_scheduler,logger);
                    eliminar_paquete(respuesta_final);

                    free(contenido_leido_completo);
                } else {
                    log_error(logger, "Error al leer los fragmentos de los Memory Sticks");
                }
                
                list_destroy_and_destroy_elements(lista_fragmentos_temp, free);
                } else {
                    log_error(logger, "Error de segmentación en dirección global: %u", direccion_fisica_global);
                }
            }
            break;
            case FINALIZAR_PROCESO: 
            {
                int pid_recibido = *(int *)list_get(paquete, 1);
                eliminar_proceso(pid_recibido,km,logger);
            /*DESCOMENTAR CUANDO KS ESPERE ESTE PROTOCOLO*******
                if(respuesta == 1) {
                   t_paquete* resp = crear_paquete(FIN_PROC_OK, crear_buffer());
                    agregar_a_paquete(resp,&pid_recibido, sizeof(int));
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);

                } else {
                    t_paquete* resp = crear_paquete(FIN_PROC_ERROR, crear_buffer());
                    agregar_a_paquete(resp,&pid_recibido, sizeof(int));
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);
                }
             */
            }
            break;

          case SWAP_REQUEST: {
            swap_block_size = *(int*)list_get(paquete, 1);
            int tamanio_total = *(int*)list_get(paquete, 2);
            km_socket_swap = socket_cliente; 
            
            int total_bloques = tamanio_total / swap_block_size;
            int bytes_bitmap = (total_bloques + 7) / 8; 
            
            void* puntero_bitmap = malloc(bytes_bitmap);
            memset(puntero_bitmap, 0, bytes_bitmap);
            bitmap_swap = bitarray_create_with_mode(puntero_bitmap, bytes_bitmap, LSB_FIRST);
            
            log_info(logger, "SWAP configurado: %d bloques de %d bytes", total_bloques, swap_block_size);
            free(datos);
            list_destroy_and_destroy_elements(paquete, free);
            return NULL;

        }

        case SUSPENSION_DE_PROCESO: {
            int pid_a_suspender = *(int*)list_get(paquete, 1);
            
            int rta = suspender_proceso_memoria(pid_a_suspender, km, logger);
            
            if (rta == 1) {
                t_paquete* resp = crear_paquete(SUSPENSION_OK, crear_buffer());
                agregar_a_paquete(resp, &pid_a_suspender, sizeof(int));
                enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                eliminar_paquete(resp);
            }
            break;
        }

        case DESUSPENSION_DE_PROCESO: {
            int pid_a_desuspender = *(int*)list_get(paquete, 1);
            
            int rta = desuspender_proceso_memoria(pid_a_desuspender, km, logger);
            
            if (rta == 1) {
                // todo cupo y se trajo desde el SWAP a la RAM
                t_paquete* resp = crear_paquete(DESUSPENSION_OK, crear_buffer());
                agregar_a_paquete(resp, &pid_a_desuspender, sizeof(int));
                enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                eliminar_paquete(resp);
            } else if (rta == 0) {
                // fallo por falta de espacio contiguo. El proceso SIGUE SUSPENDIDO en disco.
                //avisar al Kernel Scheduler para que ignore a este PID por ahora.
                t_paquete* resp = crear_paquete(DESUSPENSION_ERROR, crear_buffer());
                agregar_a_paquete(resp, &pid_a_desuspender, sizeof(int));
                enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                eliminar_paquete(resp);
            }
            break;
        }
            case ELIMINACION_DE_SEGMENTO: 
            {
                int pid_recibido = *(int *)list_get(paquete, 1);
                int id_seg_recibido = *(int *)list_get(paquete, 2);
                int respuesta = eliminar_segmento(pid_recibido,id_seg_recibido,logger);

                if(respuesta == 1) {
                    t_paquete* resp = crear_paquete(ELIMINACION_DE_SEG_OK, crear_buffer());
                    agregar_a_paquete(resp,&pid_recibido, sizeof(int));
                    agregar_a_paquete(resp,&id_seg_recibido, sizeof(int));
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);

                } else {
                    t_paquete* resp = crear_paquete(ELIMINACION_DE_SEG_ERROR, crear_buffer());
                    agregar_a_paquete(resp,&pid_recibido, sizeof(int));
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);
                }
            }
            break;
            case CREACION_DE_SEGMENTO: 
            {   
                int pid_recibido = *(int *)list_get(paquete, 1);
                int id_seg_recibido = *(int *)list_get(paquete, 2);
                uint32_t tamano_recibido = *(int *)list_get(paquete, 3);

                int respuesta =  crear_segmento(pid_recibido, id_seg_recibido,tamano_recibido,logger,km); //EL TIPO DE DATO DE TAMAÑO DEBERIA SER INT O UINT32_T?

                if(respuesta == 1) {
                    t_paquete* resp = crear_paquete(CREACION_DE_SEGMENTO_OK, crear_buffer());
                    agregar_a_paquete(resp,&pid_recibido, sizeof(int));
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);

                } else {
                    //cuando no hay ms conectados por lo que no se pudo crear el segmento?
                    t_paquete* resp = crear_paquete(CREACION_DE_SEGMENTO_ERROR, crear_buffer());
                    agregar_a_paquete(resp,&pid_recibido, sizeof(int));
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);
                }
             }
             break;
            case ACTUALIZAR_CONTEXTO:
            {
                int pid = *(int*)list_get(paquete, 1);
                t_registros registros_nuevos;
                registros_nuevos.PC  = *(uint32_t*)list_get(paquete, 2);
                registros_nuevos.AX  = *(uint8_t*)list_get(paquete, 3);
                registros_nuevos.BX  = *(uint8_t*)list_get(paquete, 4);
                registros_nuevos.CX  = *(uint8_t*)list_get(paquete, 5);
                registros_nuevos.DX  = *(uint8_t*)list_get(paquete, 6);
                registros_nuevos.EAX = *(uint32_t*)list_get(paquete, 7);
                registros_nuevos.EBX = *(uint32_t*)list_get(paquete, 8);
                registros_nuevos.ECX = *(uint32_t*)list_get(paquete, 9);
                registros_nuevos.EDX = *(uint32_t*)list_get(paquete, 10);
                registros_nuevos.SI  = *(uint32_t*)list_get(paquete, 11);
                registros_nuevos.DI  = *(uint32_t*)list_get(paquete, 12);
                actualizar_contexto(pid, &registros_nuevos,logger);
    
            }
            break;
            case CPUS_DESALOJADAS:
            {//deberia agregar dos mutex lock de procesos en todo este case? para evitar que cpu me solicite ctx o me envie ctx
                int pid = *(int*) list_get(paquete, 1);
                int id_seg = *(int*) list_get(paquete, 2);
                uint32_t tamano = *(uint32_t *)list_get(paquete, 3);
                int rta = iniciar_compactacion(logger,km,pid,id_seg,tamano);
                usleep(km->compaction_delay * 1000); 
                 if(rta == 1) {
                    t_paquete* resp = crear_paquete(COMPACTACION_OK, crear_buffer());
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);

                } else {
                    t_paquete* resp = crear_paquete(COMPACTACION_ERROR, crear_buffer());
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);
                }
                
            }
            break;
            case IO_OK: {//agrego para que hilo dispatcher de ms maneje la comunicacion entre los hilos de km y los ms conectados
                log_debug(logger, "Dispatcher: Recibido IO_OK del socket %d", socket_cliente);
                
                // Identificamos a qué MS pertenece este socket
                t_ms_info* ms = buscar_ms_por_socket(socket_cliente);
                
                pthread_mutex_lock(&ms->mutex_socket);
                ms->ultimo_codigo_op = IO_OK;
                ms->respuesta_lista = true;
                pthread_cond_signal(&ms->cond_respuesta); // Despertamos al hilo que ejecutó enviar_fragmentos
                pthread_mutex_unlock(&ms->mutex_socket);
                break;
            }
            case DATOS_LEIDOS: {
                log_debug(logger, "Dispatcher: Recibidos datos de lectura del socket %d", socket_cliente);

                t_ms_info* ms = buscar_ms_por_socket(socket_cliente);
                if (ms == NULL) break;

                pthread_mutex_lock(&ms->mutex_socket);

                int tamano_recibido = *(int*) list_get(paquete, 1);
                void* datos_en_paquete = list_get(paquete, 2);

                ms->buffer_respuesta = malloc(tamano_recibido);
                if (ms->buffer_respuesta != NULL) {
                    memcpy(ms->buffer_respuesta, datos_en_paquete, tamano_recibido);
                    ms->ultimo_codigo_op = DATOS_LEIDOS;
                } else {
                    ms->ultimo_codigo_op = ERROR_CONEXION_MS;
                }

                ms->respuesta_lista = true;
                pthread_cond_signal(&ms->cond_respuesta); // Despertamos a enviar_fragmentos_lectura
                pthread_mutex_unlock(&ms->mutex_socket);
                break;
            }
             case ERROR_OPERACION://ms lo envia cuando falla stdin o stdout 
            {   //no deberia ir aca?
                //qué hago si falla? creo que no se considera en las pruebas
                log_info(logger, "[Socket %d] Error en operación de lectura/escritura en Memory Stick recibida", socket_cliente);
            }
            break;
            
        default:
          log_error(logger, "[Socket %d] Código de operación desconocido: %d", socket_cliente, codigo_operacion);
          break;
        }
        list_destroy_and_destroy_elements(paquete, free);
    }

    if (es_cpu) {
        remover_cpu_conectada(socket_cliente, logger);
    }

    close(socket_cliente);
    log_info(logger, "Conexión cerrada en socket %d", socket_cliente);

    free(datos);
    return NULL;
}

void remover_cpu_conectada(int socket_cliente, t_log* logger) {
    pthread_mutex_lock(&mutex_cpus_conectadas);
    
    for(int i = 0; i < list_size(cpus_conectadas); i++) {
        t_cpu* cpu = list_get(cpus_conectadas, i);
        
        if (cpu->socket == socket_cliente) {
            log_warning(logger, "CPU ID:%d desconectada. Removiendo de la lista (Socket: %d)", cpu->id, socket_cliente);
            
            list_remove(cpus_conectadas, i);
            free(cpu);
            break; 
        }
    }
    
    pthread_mutex_unlock(&mutex_cpus_conectadas);
}

int suspender_proceso_memoria(int pid, t_kernel_memory* km, t_log* logger) {
    pthread_mutex_lock(&mutex_procesos);
    t_proceso* proceso = buscar_proceso(pid);
    if (proceso == NULL || proceso->suspendido) {
        pthread_mutex_unlock(&mutex_procesos);
        return -1;
    }
    
    // duplicar la lista para iterar sin retener el mutex global
    t_list* segmentos = list_duplicate(proceso->contexto->tabla_segmentos);
    pthread_mutex_unlock(&mutex_procesos);

    log_info(logger, "Iniciando suspensión de PID: %d con %d segmentos", pid, list_size(segmentos));

    for (int i = 0; i < list_size(segmentos); i++) {
        t_segmento* seg = list_get(segmentos, i);

        pthread_mutex_lock(&mutex_procesos);
        bool en_swap = seg->en_swap;
        uint32_t tam_seg = seg->tamanio;
        uint32_t base_global = seg->base_global;
        pthread_mutex_unlock(&mutex_procesos);

        if (!en_swap) {
            // leer los datos de la memoria fisica (Memory Sticks)
            t_list* fragmentos = calcular_dir_local_ms(base_global, tam_seg, logger);
            void* contenido = enviar_fragmentos_lectura(fragmentos, tam_seg, logger, km);
            list_destroy_and_destroy_elements(fragmentos, free);

            if (contenido == NULL) continue;

            // buscar espacio en SWAP
            int bloques_necesarios = (tam_seg + swap_block_size - 1) / swap_block_size;
            int bloque_inicio = buscar_bloques_libres_swap(bloques_necesarios);

            if (bloque_inicio != -1) {
                // escribir en disco (SWAP)
                bool exito = escribir_segmento_en_swap(contenido, tam_seg, bloque_inicio, bloques_necesarios, logger);

                if (exito) {
                    // actualizar el segmento y liberamos la RAM
                    pthread_mutex_lock(&mutex_procesos);
                    seg->en_swap = true;
                    seg->bloque_swap = bloque_inicio;
                    seg->base_global = 0; 
                    seg->limite_global = 0;
                    pthread_mutex_unlock(&mutex_procesos);

                    agregar_hueco_libre(base_global, tam_seg);
                }
            } else {
                log_error(logger, "No se encontró espacio en SWAP para segmento del PID %d", pid);
            }
            free(contenido);
        }
    }

    pthread_mutex_lock(&mutex_procesos);
    proceso->suspendido = true;
    pthread_mutex_unlock(&mutex_procesos);
    
    list_destroy(segmentos);
    return 1;
}

// busqueda silenciosa (No dispara compactacion)
t_hueco* buscar_hueco_silencioso(uint32_t tamano, t_kernel_memory* km) {
    t_hueco* seleccionado = NULL;
    for (int i = 0; i < list_size(lista_huecos_libres); i++) {
        t_hueco* hueco = list_get(lista_huecos_libres, i);
        if (hueco->tamano >= tamano) {
            if (strcmp(km->allocation_strategy, "BEST") == 0) {
                if (seleccionado == NULL || hueco->tamano < seleccionado->tamano) seleccionado = hueco;
            } else {
                if (seleccionado == NULL || hueco->tamano > seleccionado->tamano) seleccionado = hueco;
            }
        }
    }
    return seleccionado;
}

int desuspender_proceso_memoria(int pid, t_kernel_memory* km, t_log* logger) {
    pthread_mutex_lock(&mutex_procesos);
    t_proceso* proceso = buscar_proceso(pid);
    if (proceso == NULL || !proceso->suspendido) {
        pthread_mutex_unlock(&mutex_procesos);
        return -1;
    }
    t_list* segmentos = list_duplicate(proceso->contexto->tabla_segmentos);
    pthread_mutex_unlock(&mutex_procesos);

    log_info(logger, "Iniciando PRE-CHECK de desuspensión para PID: %d", pid);

    t_list* reservas = list_create();
    bool entra_todo = true;

    for (int i = 0; i < list_size(segmentos); i++) {
        t_segmento* seg = list_get(segmentos, i);
        
        pthread_mutex_lock(&mutex_procesos);
        bool esta_en_swap = seg->en_swap;
        pthread_mutex_unlock(&mutex_procesos);

        if (esta_en_swap) {
            pthread_mutex_lock(&mutex_huecos);
            t_hueco* hueco = buscar_hueco_silencioso(seg->tamanio, km); 
            
            if (hueco != NULL) {
                t_reserva* reserva = malloc(sizeof(t_reserva));
                reserva->base = hueco->base;
                reserva->tamanio = seg->tamanio;
                reserva->segmento = seg;
                
                consumir_hueco(hueco, seg->tamanio);
                list_add(reservas, reserva);
            } else {
                entra_todo = false;
            }
            pthread_mutex_unlock(&mutex_huecos);

            if (!entra_todo) break; // falló un segmento, rompemos el ciclo
        }
    }

    if (!entra_todo) {
        // devolver los huecos que reservamos porque el proceso no entra completo
        log_warning(logger, "PID %d no cuenta con espacio contiguo. Abortando desuspensión (Rollback).", pid);
        
        pthread_mutex_lock(&mutex_huecos);
        for (int i = 0; i < list_size(reservas); i++) {
            t_reserva* r = list_get(reservas, i);
            agregar_hueco_libre(r->base, r->tamanio); //devolver como huecos libres
        }
        pthread_mutex_unlock(&mutex_huecos);
        
        list_destroy_and_destroy_elements(reservas, free);
        list_destroy(segmentos);
        return 0; // fallo por espacio (proceso sigue suspendido)
    }

    // el espacio está asegurado
    for (int i = 0; i < list_size(reservas); i++) {
        t_reserva* r = list_get(reservas, i);
        t_segmento* seg = r->segmento;

        int bloques_necesarios = (seg->tamanio + swap_block_size - 1) / swap_block_size;
        void* contenido = leer_segmento_de_swap(seg->bloque_swap, bloques_necesarios, seg->tamanio, logger);

        if (contenido != NULL) {
            // mandar los datos a los Memory Sticks
            t_list* fragmentos = calcular_dir_local_ms(r->base, seg->tamanio, logger);
            enviar_fragmentos_escritura(fragmentos, contenido, logger, km);
            list_destroy_and_destroy_elements(fragmentos, free);

            // actualizar el segmento
            pthread_mutex_lock(&mutex_procesos);
            seg->en_swap = false;
            seg->base_global = r->base;
            seg->limite_global = r->base + seg->tamanio - 1;
            seg->bloque_swap = -1;
            pthread_mutex_unlock(&mutex_procesos);
            
            free(contenido);
        }
    }

    pthread_mutex_lock(&mutex_procesos);
    proceso->suspendido = false;
    pthread_mutex_unlock(&mutex_procesos);

    list_destroy_and_destroy_elements(reservas, free);
    list_destroy(segmentos);
    return 1; 
}
