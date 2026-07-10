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
        
        if (!paquete) {
            log_error(logger, "Error al recibir paquete o cliente desconectado en socket %d.", socket_cliente);
            break; // Salimos del bucle si el cliente se cae
        }

        int codigo_operacion = *(int*) list_get(paquete, 0);

        switch (codigo_operacion) {
            case CPU_HANDSHAKE:
                es_cpu = true;
                int cpu_id = *(int *)list_get(paquete, 1);
                log_debug(logger, "[Socket %d] Conexión con CPU ID:%d exitosa", socket_cliente,cpu_id);
            
                agregar_cpu_conectada(cpu_id, socket_cliente);
                //enviar datos de los ms ya conectados antes que esta cpu
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
                pthread_mutex_unlock(&mutex_memoria_total);
                free(datos);
                list_destroy_and_destroy_elements(paquete, free);
                return NULL;
                       
            }

            case KERNEL_SCHEDULER_HANDSHAKE:
                log_info(logger, "[Socket %d] Conexión con KERNEL SCHEDULER exitosa", socket_cliente);
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
                log_info(logger, "## Creación de Proceso - PID: %d", pid_nuevo); 
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
                    log_info(logger, "Contexto enviado - PID:%d | Segmentos:%d", pid_solicitado, cant_segmentos);
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
                //ms me retorna lo leido acá,envío lo leido a ks acá ó si ms se desconecta se detecta acá y tambien envia aviso a ks-
                void* contenido_leido_completo = enviar_fragmentos_lectura(lista_fragmentos_temp, tamano, logger,km);

                if (contenido_leido_completo != NULL) {
                    
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
                int respuesta = eliminar_proceso(pid_recibido,km,logger);
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
            
            pthread_mutex_lock(&mutex_procesos);
            t_proceso* proceso = buscar_proceso(pid_a_suspender);
            
            if (proceso != NULL && !proceso->suspendido) {
                log_info(logger, "Iniciando suspensión de PID: %d", pid_a_suspender);
                
                t_list* tabla_segmentos = proceso->contexto->tabla_segmentos;
                pthread_mutex_unlock(&mutex_procesos); // Liberamos para que las CPUs sigan respondiendo FETCH
                log_info(logger,"PID %d tiene %d segmentos",pid_a_suspender,list_size(tabla_segmentos));
                for (int i = 0; i < list_size(tabla_segmentos); i++) {
                    pthread_mutex_lock(&mutex_procesos);
                    t_segmento* seg = list_get(tabla_segmentos, i);
                    bool en_swap = seg->en_swap;
                    uint32_t tam_seg = seg->tamanio;
                    uint32_t base_global = seg->base_global;
                    pthread_mutex_unlock(&mutex_procesos);
                    
                    if (!en_swap) {
                        t_list* fragmentos = calcular_dir_local_ms(base_global, tam_seg, logger);
                        void* contenido = enviar_fragmentos_lectura(fragmentos, tam_seg, logger,km);
                        list_destroy_and_destroy_elements(fragmentos, free);
                        
                        int bloques_necesarios = (tam_seg + swap_block_size - 1) / swap_block_size;
                        int bloque_inicio = -1;
                        
                        pthread_mutex_lock(&mutex_huecos); // Protegemos acceso a bitarray_swap si es compartido
                        for (int bit = 0; bit <= bitarray_get_max_bit(bitmap_swap) - bloques_necesarios; bit++) {
                            bool hay_espacio = true;
                            for (int b = 0; b < bloques_necesarios; b++) {
                                if (bitarray_test_bit(bitmap_swap, bit + b)) {
                                    hay_espacio = false; 
                                    break;
                                }
                            }
                            if (hay_espacio) { 
                                bloque_inicio = bit; 
                                break; 
                            }
                        }
                        
                        if (bloque_inicio != -1 && contenido != NULL) {
                            for(int b = 0; b < bloques_necesarios; b++) {
                                bitarray_set_bit(bitmap_swap, bloque_inicio + b);
                            }
                            pthread_mutex_unlock(&mutex_huecos);
                        
                            int offset = 0;
                            bool transaccion_exitosa = true;
                            
                            for (int b = 0; b < bloques_necesarios; b++) {
                                int bytes_restantes = tam_seg - offset;
                                int tamano_a_escribir = (bytes_restantes > swap_block_size) ? swap_block_size : bytes_restantes;

                                t_paquete* p_swap = crear_paquete(ESCRITURA_SWAP, crear_buffer());
                                int bloque_actual = bloque_inicio + b;
                                agregar_a_paquete(p_swap, &bloque_actual, sizeof(int));
                                agregar_a_paquete(p_swap, contenido + offset, tamano_a_escribir);
                                
                                pthread_mutex_lock(&mutex_socket_swap);
                                int err_envio = enviar_paquete(p_swap, km_socket_swap, logger);
                                eliminar_paquete(p_swap);
                                
                                if (err_envio == -1) {
                                    log_error(logger, "Error al enviar bloque %d a SWAP.", bloque_actual);
                                    pthread_mutex_unlock(&mutex_socket_swap);
                                    transaccion_exitosa = false;
                                    break;
                                }
                                
                                t_list* resp_swap = recibir_paquete(km_socket_swap);
                                pthread_mutex_unlock(&mutex_socket_swap);
                                
                                if (resp_swap == NULL) {
                                    log_error(logger, "SWAP desconectado o respuesta inválida.");
                                    transaccion_exitosa = false;
                                    break;
                                }
                                list_destroy_and_destroy_elements(resp_swap, free);
                                
                                offset += tamano_a_escribir;
                            }
                            
                            if (transaccion_exitosa) {
                                pthread_mutex_lock(&mutex_procesos);
                                seg->en_swap = true;
                                seg->bloque_swap = bloque_inicio;
                                pthread_mutex_unlock(&mutex_procesos);
                                
                                agregar_hueco_libre(base_global, tam_seg);
                                
                                pthread_mutex_lock(&mutex_procesos);
                                seg->base_global = 0; 
                                seg->limite_global = 0;
                                pthread_mutex_unlock(&mutex_procesos);
                            }
                        } else {
                            pthread_mutex_unlock(&mutex_huecos);
                        }
                        if(contenido) free(contenido);
                    }
                }
                
                pthread_mutex_lock(&mutex_procesos);
                proceso->suspendido = true;
                pthread_mutex_unlock(&mutex_procesos);
                
                t_paquete* resp = crear_paquete(SUSPENSION_OK, crear_buffer());
                enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                eliminar_paquete(resp);
            } else {
                pthread_mutex_unlock(&mutex_procesos);
            }
            break;
        }

        case DESUSPENSION_DE_PROCESO: {
            int pid_a_desuspender = *(int*)list_get(paquete, 1);
            
            pthread_mutex_lock(&mutex_procesos);
            t_proceso* proceso = buscar_proceso(pid_a_desuspender);
            
            if (proceso != NULL && proceso->suspendido) {
                log_info(logger, "Iniciando desuspensión de PID: %d", pid_a_desuspender);
                bool necesita_compactar = false;
                t_list* tabla_segmentos = proceso->contexto->tabla_segmentos;
                pthread_mutex_unlock(&mutex_procesos);
                
                for (int i = 0; i < list_size(tabla_segmentos); i++) {
                    pthread_mutex_lock(&mutex_procesos);
                    t_segmento* seg = list_get(tabla_segmentos, i);
                    bool en_swap = seg->en_swap;
                    uint32_t tam_seg = seg->tamanio;
                    int bloque_swap = seg->bloque_swap;
                    pthread_mutex_unlock(&mutex_procesos);
                    
                    if(en_swap) {
                        pthread_mutex_lock(&mutex_huecos);
                        t_hueco* hueco = buscar_hueco(tam_seg, km, logger);
                        
                        if(hueco == NULL) {
                            necesita_compactar = true;
                            pthread_mutex_unlock(&mutex_huecos);
                            break;
                        }
                        
                        uint32_t nueva_base = hueco->base;
                        consumir_hueco(hueco, tam_seg);
                        pthread_mutex_unlock(&mutex_huecos);
                        
                        int bloques_necesarios = (tam_seg + swap_block_size - 1) / swap_block_size;
                        void* contenido_recuperado = malloc(tam_seg);
                        int offset = 0;
                        bool lectura_completa = true;
                        
                        for(int b = 0; b < bloques_necesarios; b++) {
                            int bytes_restantes = tam_seg - offset;
                            int tamano_a_leer = (bytes_restantes > swap_block_size) ? swap_block_size : bytes_restantes;

                            t_paquete* p_swap = crear_paquete(LECTURA_SWAP, crear_buffer());
                            int bloque_actual = bloque_swap + b;
                            agregar_a_paquete(p_swap, &bloque_actual, sizeof(int));
                            
                            pthread_mutex_lock(&mutex_socket_swap);
                            int err_envio = enviar_paquete(p_swap, km_socket_swap, logger);
                            eliminar_paquete(p_swap);
                            
                            if (err_envio == -1) {
                                log_error(logger, "Error de red solicitando lectura a SWAP.");
                                pthread_mutex_unlock(&mutex_socket_swap);
                                lectura_completa = false;
                                break;
                            }
                            
                            t_list* resp_swap = recibir_paquete(km_socket_swap);
                            pthread_mutex_unlock(&mutex_socket_swap);
                            
                            if (resp_swap == NULL) {
                                log_error(logger, "Error: No se recibió respuesta de lectura desde SWAP.");
                                lectura_completa = false;
                                break;
                            }
                            
                            void* datos_recibidos = list_get(resp_swap, 1);
                            if (datos_recibidos != NULL) {
                                memcpy(contenido_recuperado + offset, datos_recibidos, tamano_a_leer);
                                
                                pthread_mutex_lock(&mutex_huecos);
                                bitarray_clean_bit(bitmap_swap, bloque_actual); 
                                pthread_mutex_unlock(&mutex_huecos);
                            }
                            
                            list_destroy_and_destroy_elements(resp_swap, free);
                            offset += tamano_a_leer;
                        }
                        
                        if (lectura_completa) {
                            t_list* fragmentos = calcular_dir_local_ms(nueva_base, tam_seg, logger);
                            enviar_fragmentos_escritura(fragmentos, contenido_recuperado, logger,km);
                            list_destroy_and_destroy_elements(fragmentos, free);
                            
                            pthread_mutex_lock(&mutex_procesos);
                            seg->en_swap = false;
                            seg->base_global = nueva_base;
                            seg->limite_global = nueva_base + tam_seg - 1;
                            seg->bloque_swap = -1;
                            pthread_mutex_unlock(&mutex_procesos);
                        }
                        
                        free(contenido_recuperado);
                    }
                }
                
                if (necesita_compactar) {
                    avisar_compactacion(km, logger);
                } else {
                    pthread_mutex_lock(&mutex_procesos);
                    proceso->suspendido = false;
                    pthread_mutex_unlock(&mutex_procesos);
                    
                    t_paquete* resp = crear_paquete(DESUSPENSION_OK, crear_buffer());
                    agregar_a_paquete(resp, &pid_a_desuspender, sizeof(int));
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);
                }
            } else {
                pthread_mutex_unlock(&mutex_procesos);
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
            {
                int rta = iniciar_compactacion(logger,km);
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