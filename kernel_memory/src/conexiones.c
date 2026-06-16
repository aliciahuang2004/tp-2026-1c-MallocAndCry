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

uint32_t memoria_total = 0;
t_list* lista_huecos_libres;
pthread_mutex_t mutex_huecos = PTHREAD_MUTEX_INITIALIZER;

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

void avisar_cpus_conectadas(int ms_id, char* ms_puerto, char* ms_ip,t_log* logger)
{
    pthread_mutex_lock(&mutex_cpus_conectadas);

    int total = list_size(cpus_conectadas);
    log_info(logger, "Avisando nuevo MS ID:%d a %d CPUs conectadas", ms_id, total);

    for(int i = 0; i < list_size(cpus_conectadas); i++) {
        t_cpu* cpu = list_get(cpus_conectadas, i);

        t_paquete* paquete = crear_paquete(MS_NUEVO_CPU, crear_buffer());
        agregar_a_paquete(paquete, &ms_id,sizeof(int));
        agregar_a_paquete(paquete, ms_puerto,strlen(ms_puerto) + 1);
        agregar_a_paquete(paquete, &ms_ip,strlen(ms_ip) + 1);
        enviar_paquete(paquete, cpu->socket, logger);
        eliminar_paquete(paquete);

        log_info(logger, "  [%d/%d] Aviso enviado a CPU ID:%d socket:%d",i + 1, total, cpu->id, cpu->socket);
    }
    pthread_mutex_unlock(&mutex_cpus_conectadas);
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
        t_ms_info* ms = buscar_ms_por_socket(socket_cliente);

            if(ms != NULL) {
                
                //para caso de corrupcion de memoria
               manejar_desconexion_memory_stick(ms, km, logger);
            } else {
                log_warning(logger, "Se desconectó un módulo no identificado. Socket:%d", socket_cliente);
            }
            //DEBERIA TENER EN CUENTA QUÉ MODULO SE DESCONECTÓ? MAS ALLA DE LOS MS (aunque no se dijo nada sobre la desconexion de otros módulos),si deberia ver cuando una cpu se desconecta
            log_error(logger, "Error al recibir paquete o cliente desconectado en socket %d.", socket_cliente);

            break; // Salimos del bucle si el cliente se cae
        }

        int codigo_operacion = *(int*) list_get(paquete, 0);

        switch (codigo_operacion) {
            case CPU_HANDSHAKE:
                int cpu_id = *(int *)list_get(paquete, 1);
                log_info(logger, "CPU ID:%d conectada en socket %d", cpu_id, socket_cliente);
            //ALMACENO EL CPU CON ID Y SOCKET EN "cpus_conectadas"
               agregar_cpu_conectada(cpu_id, socket_cliente);
            //*********ENVIA SEGMENT MAX SIZE APENAS SE CONECTA CPU*************************DESCOMENTAR CUANDO CPU ESPERE SEG_MAX_SIZE
                t_paquete *respuesta = crear_paquete(SEG_MAX_SIZE, crear_buffer());
                agregar_a_paquete(respuesta, &km->segment_max_size, sizeof(int));
                enviar_paquete(respuesta, socket_cliente, logger);
                eliminar_paquete(respuesta);
                
                break;

            case MEMORY_STICK_HANDSHAKE: {
                int ms_id = *(int *)list_get(paquete, 1);
                uint32_t ms_tamano = *(int *)list_get(paquete, 2);
                char* ms_puerto = (char*) list_get(paquete, 3);
                char* ms_ip = (char*)list_get(paquete, 4);

                log_info(logger,"[Socket %d] NUEVO MEMORY STICK conectado - ID:%d Tamaño:%d bytes Puerto:%s",socket_cliente,ms_id,ms_tamano,ms_puerto);

                uint32_t base_nuevo_ms = aumentar_memoria_total(ms_tamano);

                int resultado = nuevo_memory_stick(ms_id, ms_tamano, socket_cliente);

                if(resultado) {
                    t_resultado_hueco r = agregar_hueco_libre(base_nuevo_ms, ms_tamano);//acá obtengo base y limite global del ms
                    loguear_huecos(logger);
                    agregar_posicion_ms(r,ms_id,logger);//ACA GUARDA BASE Y LIMITE GLOBAL DE LOS MS,FALTA PROBAR.ACA KM BUSCA A QUÉ MS ENVIAR PETICION DE ESCRITURA,LECTURA

                    pthread_mutex_lock(&mutex_memoria_total);
                    log_info(logger, "Memoria total disponible: %u bytes", memoria_total);
                    pthread_mutex_unlock(&mutex_memoria_total);
                   
                   //AVISO A KS QUE HAY MAS MEMORIA DISPONIBLE:
                    t_paquete *respuesta = crear_paquete(AUMENTO_DE_MEMORIA, crear_buffer());
                    agregar_a_paquete(respuesta,&memoria_total,sizeof(int));
                    enviar_paquete(respuesta,km->socket_kernel_scheduler, logger);
                    eliminar_paquete(respuesta);
                    //AVISO A TODAS LAS CPUS:
                    //int ms_ip = 127001;
                    avisar_cpus_conectadas(ms_id,ms_puerto,ms_ip,logger);              
                } else {
                    log_error(logger, "Error al agregar Memory Stick ID:%d a la lista", ms_id);
                }
                break;
            }

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

                inicializar_proceso_memoria(pid_nuevo, path_relativo, km);
            
                int resultado = crear_proceso(pid_nuevo);//************AGREGA PROCESO AL DICTIONARY**********
                
                 if (resultado == 0)
                { //************************ENVIO CONFIRMACION O ERROR DE CREACION DE PROCESO A KS********************
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

            case REQUEST_CONTEXTO://descomentar cuando cpu espere tabla
            {
                int pid_solicitado = *(int *)list_get(paquete, 1);
                int cpu_id         = *(int *)list_get(paquete, 2);
                log_debug(logger, "CPU ID:%d solicitó contexto para PID:%d", cpu_id, pid_solicitado);

                //t_list* tabla_segmentos = NULL;
                //t_registros* regs = solicitud_contexto(pid_solicitado, &tabla_segmentos);
                t_registros* regs = solicitud_contexto(pid_solicitado);

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

                    /*
                    // Cantidad de segmentos (para que la CPU sepa cuántos leer)
                    int cant_segmentos = list_size(tabla_segmentos);
                    agregar_a_paquete(respuesta, &cant_segmentos, sizeof(int));

                    // Cada segmento campo por campo
                    for (int i = 0; i < cant_segmentos; i++) {
                        t_segmento* seg = list_get(tabla_segmentos, i);
                        agregar_a_paquete(respuesta, &seg->id_segmento,    sizeof(int));
                        agregar_a_paquete(respuesta, &seg->base_global,    sizeof(uint32_t));
                        agregar_a_paquete(respuesta, &seg->limite_global,  sizeof(uint32_t));
                        agregar_a_paquete(respuesta, &seg->memory_stick_id,sizeof(int));//sirve a cpu cuando ejecuta mov out ,mov in?
                       // agregar_a_paquete(respuesta, &seg->en_swap,        sizeof(bool));
                       //agregar_a_paquete(respuesta, &seg->bloque_swap,    sizeof(int));
                    }
                    */
                    enviar_paquete(respuesta, socket_cliente, logger);
                    eliminar_paquete(respuesta);

                    // Liberar copia local
                    free(regs);
                    //list_destroy_and_destroy_elements(tabla_segmentos, free);

                    //log_info(logger, "Contexto enviado - PID:%d | Segmentos:%d", pid_solicitado, cant_segmentos);
                } else {
                    log_error(logger, "No se encontró contexto para PID %d", pid_solicitado);
                    t_paquete* error = crear_paquete(CONTEXT_ERROR, crear_buffer());
                    enviar_paquete(error, socket_cliente, logger);
                    eliminar_paquete(error);
                }
            }  
            break;
        
           case ESCRITURA_DE_DATOS: ///***ESPERO STDIN DE KS
            {
                uint32_t direccion_fisica_global = *(int*) list_get(paquete, 1);
                int pid_recibido  = *(int*) list_get(paquete, 2);
                char* contenido_a_escribir = (char*) list_get(paquete, 3);

                // Calculamos el tamaño del contenido que nos mandó la CPU
                int tamano_contenido = strlen(contenido_a_escribir) + 1; 

                // 1. LLAMADA: La función crea internamente la lista y nos la devuelve llena
                t_list* lista_fragmentos_temp = calcular_dir_local_ms(direccion_fisica_global,tamano_contenido,logger);

                if (lista_fragmentos_temp != NULL) {
                    // Paso 2: Enviar cada fragmento a su respectivo Memory Stick
                    enviar_fragmentos_escritura(lista_fragmentos_temp, contenido_a_escribir, logger);

                    // Paso 3: Esperar las respuestas de confirmación de los MS y responder a CPU...
                    // (Aquí agregarías la lógica para recibir los "IO_OK" de los MS antes de responderle a la CPU)

                    // Paso 4: Limpieza absoluta de la memoria temporal del hilo
                    list_destroy_and_destroy_elements(lista_fragmentos_temp, free);
                } else {
                    log_error(logger, "Error de segmentación global para PID:%d", pid_recibido);
                    // Enviar código de error a la CPU si corresponde...
                }   
            }
            break;
            case LECTURA_DE_DATOS: ///***ESPERO STDOUT DE KS
            {
            }
            break;
            case FINALIZAR_PROCESO: ///***ESPERO EXIT DE KS
            {
                int pid_recibido = *(int *)list_get(paquete, 1);
                int respuesta = eliminar_proceso(pid_recibido,km,logger);
            
                if(respuesta == 1) {
                    t_paquete* resp = crear_paquete(FIN_PROC_OK, crear_buffer());
                    agregar_a_paquete(resp,&pid_recibido, sizeof(int));
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);

                } else {//******************************NO ES OBLIGATORIO PODRIA SIMPLEMENTE AGREGAR UN LOG PARA QUE EN KS NO TENGA QUE ESPERAR ESTE MSJ**********
                    t_paquete* resp = crear_paquete(FIN_PROC_ERROR, crear_buffer());
                    agregar_a_paquete(resp,&pid_recibido, sizeof(int));
                    enviar_paquete(resp, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(resp);
                }
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
            case ELIMINACION_DE_SEGMENTO: ///***ESPERO MEM_FREE DE KS
            {
                int pid_recibido = *(int *)list_get(paquete, 1);
                int id_seg_recibido = *(int *)list_get(paquete, 2);
                int respuesta = eliminar_segmento(pid_recibido,id_seg_recibido,logger);

                if(respuesta == 1) {
                    t_paquete* resp = crear_paquete(ELIMINACION_DE_SEG_OK, crear_buffer());
                    agregar_a_paquete(resp,&pid_recibido, sizeof(int));
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
                
                //logs temporales solo para pruebas***********
                log_info(logger, "## Contexto actualizado recibido - PID: %d", pid);
                log_info(logger, "   PC=%u  AX=%u   BX=%u   CX=%u   DX=%u", 
                    registros_nuevos.PC, registros_nuevos.AX, registros_nuevos.BX, 
                    registros_nuevos.CX, registros_nuevos.DX);
                log_info(logger, "   EAX=%u     EBX=%u    ECX=%u   EDX=%u   SI=%u   DI=%u",
                    registros_nuevos.EAX, registros_nuevos.EBX, registros_nuevos.ECX,
                    registros_nuevos.EDX, registros_nuevos.SI, registros_nuevos.DI);
                //********************************************

                actualizar_contexto(pid, &registros_nuevos);
                log_info(logger, "Contexto actualizado - PID: %d", pid);
            }
            break;
            case CPUS_DESALOJADAS://ANTES DEBERIA DESARROLLAR LAS ESCRITURAS,LECTURAS DE SEGMENTOS
            {/// KS DEBERIA ENVIAR "CPUS_DESALOJADAS" CUANDO TERMINA DE DESALOJAR A TODAS LAS CPUS,
            //PARA COMENZAR CON LA COMPACTACION EN KM 
            //(CREO TEMPORALMENTE UNA TABLA GLOBAL DE SEGMENTOS EN "comenzar_compactacion")
            }
            break;
        
        default:
          log_error(logger, "[Socket %d] Código de operación desconocido: %d", socket_cliente, codigo_operacion);
          break;
        }
        list_destroy_and_destroy_elements(paquete, free);
    }
    close(socket_cliente);
    log_info(logger, "Conexión cerrada en socket %d", socket_cliente);

    free(datos);
    return NULL;
}

    