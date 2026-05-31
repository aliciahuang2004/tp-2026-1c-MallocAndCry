#include "conexiones.h"
#include "kernel_memory.h"
#include "contextos.h"
#include "instrucciones.h"
#include "procesos.h"
#include "memory_stick.h"

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

            case MEMORY_STICK_HANDSHAKE: {
                int ms_id = *(int *)list_get(paquete, 1);
                int ms_tamano = *(int *)list_get(paquete, 2);

                log_info(logger, "[Socket %d] MEMORY STICK conectado - ID:%d Tamaño:%d bytes", socket_cliente, ms_id, ms_tamano);

                uint32_t base_nuevo_ms = aumentar_memoria_total(ms_tamano);
                //ver si me sirve de algo tener una lista de ms
                int resultado = nuevo_memory_stick(ms_id, ms_tamano, socket_cliente);

                if(resultado) {
                    agregar_hueco_libre(base_nuevo_ms, ms_tamano);

                    //logs temporales para pruebas
                    pthread_mutex_lock(&mutex_huecos);
                    log_info(logger, "Cantidad de huecos libres: %d", list_size(lista_huecos_libres));
                    pthread_mutex_unlock(&mutex_huecos);
                    pthread_mutex_lock(&mutex_lista_ms);
                    log_info(logger, "Memory sticks conectados: %d", list_size(lista_ms));
                    pthread_mutex_unlock(&mutex_lista_ms);
                    pthread_mutex_lock(&mutex_memoria_total);
                    log_info(logger, "Memoria total disponible: %u bytes", memoria_total);
                    pthread_mutex_unlock(&mutex_memoria_total);
                    log_info(logger, "Hueco agregado -> Base:%u Tamaño:%u", base_nuevo_ms, ms_tamano);

                    //descomentar cuando ks reciba o espere nuevo tamaño de memoria
                    /*t_paquete* respuesta = crear_paquete(NUEVO_MEMORY_STICK, crear_buffer());
                    agregar_a_paquete(respuesta, &ms_tamano, sizeof(int));
                    enviar_paquete(respuesta, km->socket_kernel_scheduler, logger);
                    eliminar_paquete(respuesta);*/
                } else {
                    log_error(logger, "Error al agregar Memory Stick ID:%d a la lista", ms_id);
                }
                //tambien tengo que avisar a las cpus sobre nuevo ms,enviarles el puerto
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
                enviar_paquete(respuesta, socket_cliente, logger);
                eliminar_paquete(respuesta);
                }
                else
                {
                log_error(logger, "ERROR AL EJECUTAR CREACION_DE_PROCESO PARA PID %d", pid_nuevo);
                t_paquete *error = crear_paquete(CREACION_DE_PROCESO_ERROR, crear_buffer());
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
                int cpu_id = *(int *)list_get(paquete, 2);

                log_debug(logger, "CPU ID:%d solicitó contexto para PID:%d", cpu_id, pid_solicitado);

                t_registros* regs =solicitud_contexto(pid_solicitado);

                if(regs != NULL)
                {
                    t_paquete* respuesta = crear_paquete(CONTEXT_RESPONSE,crear_buffer());
                    agregar_a_paquete(respuesta, &pid_solicitado, sizeof(int));
                    agregar_a_paquete(respuesta,&regs->PC,sizeof(uint32_t));
                    agregar_a_paquete(respuesta,&regs->AX,sizeof(uint8_t));
                    agregar_a_paquete(respuesta,&regs->BX,sizeof(uint8_t));
                    agregar_a_paquete(respuesta,&regs->CX,sizeof(uint8_t));
                    agregar_a_paquete(respuesta,&regs->DX,sizeof(uint8_t));
                    agregar_a_paquete(respuesta,&regs->EAX,sizeof(uint32_t));
                    agregar_a_paquete(respuesta,&regs->EBX,sizeof(uint32_t));
                    agregar_a_paquete(respuesta,&regs->ECX,sizeof(uint32_t));
                    agregar_a_paquete(respuesta,&regs->EDX,sizeof(uint32_t));
                    agregar_a_paquete(respuesta,&regs->SI,sizeof(uint32_t));
                    agregar_a_paquete(respuesta,&regs->DI,sizeof(uint32_t));

                    enviar_paquete(respuesta,socket_cliente,logger);

                    eliminar_paquete(respuesta);

                    free(regs);
                    
                    log_info(logger,"Contexto enviado - PID:%d",pid_solicitado);
                }
                else
                {
                    log_error(logger,"No se encontró contexto para PID %d",pid_solicitado);

                    t_paquete* error =crear_paquete(CONTEXT_ERROR,crear_buffer());

                    enviar_paquete(error,socket_cliente,logger);

                    eliminar_paquete(error);
                }
            break;
            }
            case ESCRITURA_DE_DATOS: ///***ESPERO STDIN DE KS
            {
            }
            break;
            case LECTURA_DE_DATOS: ///***ESPERO STDOUT DE KS
            {
            }
            break;
            case FINALIZAR_PROCESO: ///***ESPERO EXIT DE KS
            {
                int pid_recibido = *(int *)list_get(paquete, 1);
                int eliminar_proceso(int pid, t_kernel_memory* km, t_log* logger);
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
                int eliminar_segmento(int pid, int id_segmento, t_log* logger);
            }
            break;
            case CREACION_DE_SEGMENTO: ///***ESPERO MEM_ALLOC DE KS
            
            {   //RECIBO PAQUETE DE PARTE DE KERNEL SCHEDULER CON LOS SIG DATOS
                int pid_recibido = *(int *)list_get(paquete, 1);
                int id_seg_recibido = *(int *)list_get(paquete, 2);
                int tamano_recibido = *(int *)list_get(paquete, 3);
                int crear_segmento(int pid_recibido, int id_seg_recibido, uint32_t tamano_recibido,t_log* logger); //EL TIPO DE DATO DE TAMAÑO DEBERIA SER INT O UINT32_T?
             //deberia enviar confirmacion a ks de que se creó correctamente el segmento???
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
                
                //logs temporales solo para verificar********
                log_info(logger, "## Contexto recibido - PID: %d", pid);
                log_info(logger, "   PC=%u AX=%u BX=%u CX=%u DX=%u", 
                    registros_nuevos.PC, registros_nuevos.AX, registros_nuevos.BX, 
                    registros_nuevos.CX, registros_nuevos.DX);
                log_info(logger, "   EAX=%u EBX=%u ECX=%u EDX=%u SI=%u DI=%u",
                    registros_nuevos.EAX, registros_nuevos.EBX, registros_nuevos.ECX,
                    registros_nuevos.EDX, registros_nuevos.SI, registros_nuevos.DI);
                //*********

                actualizar_contexto(pid, &registros_nuevos);
                log_info(logger, "Contexto actualizado - PID: %d", pid);
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

    