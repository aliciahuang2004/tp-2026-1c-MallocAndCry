#include "memory_stick.h"
#include <unistd.h>

t_memory_stick* iniciar_memory_stick(char* ms_config,int tamano_ms,int id){
    t_memory_stick* ms = malloc(sizeof(t_memory_stick));

   char log_name[50];
   snprintf(log_name, sizeof(log_name),"ms_%d.log",id);

    //abajo cargo los campos del struct t_memory_stick
    ms ->logger = iniciar_logger(log_name,"MEMORY_STICK",1,LOG_LEVEL_INFO);
    ms ->config = iniciar_config(ms->logger,ms_config);
    ms ->log_level = config_get_string_value(ms->config,"LOG_LEVEL");
    
    ms->logger->detail = obtener_log_level(ms->log_level);
    ms->id = id;
    ms->tamano = tamano_ms;

    ms->memoria = malloc(ms->tamano);
    if(ms->memoria == NULL){
      log_error(ms->logger,"Error al reservar %d bytes de memoria",ms->tamano);
      return NULL;
    }
    memset(ms->memoria,0,ms->tamano);

    //abajo lleno los campos que vienen del archivo de configuracion memory_stick.config
    ms ->puerto_escucha = config_get_string_value(ms->config,"PUERTO_ESCUCHA");
    ms ->ip_kernel_memory = config_get_string_value(ms->config,"IP_KERNEL_MEMORY");
    ms ->puerto_kernel_memory = config_get_string_value(ms->config,"PUERTO_KERNEL_MEMORY");

   //loggeo que todo se cargò correctamente
   log_debug(ms->logger, "El mòdulo Memory Stick se inicializò correctamente");

   return ms;
}


void verificar_memory_stick(t_memory_stick* ms) {
    log_debug(ms->logger, "ID: %d", ms->id);
    log_debug(ms->logger, "Tamaño: %d bytes", ms->tamano);
    log_debug(ms->logger, "Log Level: %s", ms->log_level);
    log_debug(ms->logger, "Puerto de escucha: %s", ms->puerto_escucha);
    log_debug(ms->logger, "IP Kernel Memory: %s", ms->ip_kernel_memory);
    log_debug(ms->logger, "Puerto Kernel Memory: %s", ms->puerto_kernel_memory);
}

int conectar_al_kernelmem(t_memory_stick* ms){
  
  //char* puerto_str = string_itoa(ms->puerto_kernel_memory);

  ms->kernel_mem_socket = crear_conexion(ms->logger,ms->ip_kernel_memory,ms->puerto_kernel_memory);
  
  if(ms->kernel_mem_socket != -1){
    log_info(ms->logger,COLOR_VERDE "## Conexiòn al Kernel Memory exitosa. IP:%s, Puerto: %s\033[0m" 
                        ,ms->ip_kernel_memory,ms->puerto_kernel_memory);

    free(ms->puerto_kernel_memory);      // ← después del log
    ms->puerto_kernel_memory = NULL;

    return 0;

  }else{
    log_error(ms->logger,"**Error al conectar a kernel memory**");

    free(ms->puerto_kernel_memory);      // ← después del log
    ms->puerto_kernel_memory = NULL;

    return -1;  
  }
  
}

void destruir_memory_stick(t_memory_stick* ms){
    if(!ms) return;

    if(ms->logger) log_destroy(ms->logger);
    if(ms->ip_kernel_memory) free(ms->ip_kernel_memory);
    if(ms->log_level) free(ms->log_level);
    if(ms->config) config_destroy(ms->config);
    if(ms->kernel_mem_socket != -1) close(ms->kernel_mem_socket);
    if(ms->puerto_kernel_memory) free(ms->puerto_kernel_memory);
    free(ms);
}

void enviar_handshake(t_memory_stick* ms){

  t_paquete* paquete = crear_paquete(MEMORY_STICK_HANDSHAKE, crear_buffer());
  agregar_a_paquete(paquete,&ms->id, sizeof(int));
  agregar_a_paquete(paquete,&ms->tamano,  sizeof(int));
  agregar_a_paquete(paquete,ms->puerto_escucha,strlen(ms->puerto_escucha) + 1);
  enviar_paquete(paquete,ms->kernel_mem_socket,ms->logger);
  eliminar_paquete(paquete);
  log_info(ms->logger,"**HANDSHAKE ENVIADO A KERNEL MEMORY - ID:%d TAMAÑO:%d",ms->id,ms->tamano);
}

 void rutina_recepcion(t_memory_stick* ms ,int servidor_fd){

    if (servidor_fd < 0) {
        log_error(ms->logger, "Servidor inválido en rutina_recepcion: %d", servidor_fd);
        return;
    }

    pthread_t hilo_exec;
    int temp_socket_cpu;

    log_debug(ms->logger,"Hilo servidor listo");

    while(1){
      temp_socket_cpu = esperar_cliente(servidor_fd);
      if(temp_socket_cpu < 0){
        log_error(ms->logger,"ERROR AL ESPERAR CLIENTE");
        continue;
      }

      t_list* paquete_ID_CPU = recibir_paquete(temp_socket_cpu);
      if(!paquete_ID_CPU){
        log_error(ms->logger,"ERROR AL RECIBIR PAQUETE ID CPU [CPU %d]", temp_socket_cpu);
        close(temp_socket_cpu);
        continue;
      }

      int cpu_id = *(int*) list_get(paquete_ID_CPU,1);
      log_debug(ms->logger,"CLIENTE CONECTADO");

      // <-- AQUÍ AGREGAS ms AL CONTEXTO -->
      t_cpu_context* ctx = malloc(sizeof(t_cpu_context));
      ctx->cpu_id = cpu_id;
      ctx->socket_cliente = temp_socket_cpu;
      ctx->ms = ms; 

      if (pthread_create(&hilo_exec,NULL,rutina_operaciones,ctx) != 0){
        log_error(ms->logger , "ERROR AL CREAR HILO SERVIDOR");
        free(ctx);
        close(temp_socket_cpu);
      } else {
        pthread_detach(hilo_exec);
        log_debug(ms->logger , "HILO SERVIDOR CREADO");
      }
    }
}

// Reemplaza toda la función "rutina_operaciones" por esto:
void* rutina_operaciones (void* args){
    t_cpu_context* ctx = (t_cpu_context*) args;
    int socket_cpu = ctx->socket_cliente;
    int cpu_id = ctx->cpu_id;
    t_memory_stick* ms = ctx->ms;
    
    log_info(ms->logger, "[CPU %d] Inicia hilo de operaciones en socket %d", cpu_id, socket_cpu);

    while (1) {
        t_list* paquete = recibir_paquete(socket_cpu);
        
        if (paquete == NULL) {
            log_error(ms->logger, "[CPU %d] Se ha desconectado o hubo un error en la conexión.", cpu_id);
            break; 
        }

        int cod_op = *(int*) list_get(paquete, 0);

        switch (cod_op) {
            case LECTURA_DE_DATOS: {
                log_info(ms->logger, "[CPU %d] Petición de LECTURA recibida", cpu_id);
                // Ejemplo de simulación de retardo (MEMORY_DELAY viene en microsegundos si lo multiplicas x1000)
                usleep(1000 * config_get_int_value(ms->config, "MEMORY_DELAY"));
                
                // TODO: Logica de lectura
                break;
            }
            case ESCRITURA_DE_DATOS: {
                log_info(ms->logger, "[CPU %d] Petición de ESCRITURA recibida", cpu_id);
                usleep(1000 * config_get_int_value(ms->config, "MEMORY_DELAY"));
                
                // TODO: Logica de escritura
                break;
            }
            default:
                log_warning(ms->logger, "[CPU %d] Código de operación desconocido: %d", cpu_id, cod_op);
                break;
        }
        
        list_destroy_and_destroy_elements(paquete, free);
    }

    close(socket_cpu);
    free(ctx);
    return NULL;
}