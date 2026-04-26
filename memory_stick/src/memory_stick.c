#include "memory_stick.h"

t_memory_stick* iniciar_memory_stick(){
    t_memory_stick* ms = malloc(sizeof(t_memory_stick));

    //abajo cargo los campos del struct t_memory_stick
   ms ->logger = iniciar_logger("memory_stick.log","MEMORY_STICK",1,LOG_LEVEL_INFO);
   ms ->config = iniciar_config(ms->logger,"memory_stick.config");
 // ms ->config = iniciar_config(ms->logger,argv[1]); ACA EL
 // ARCHIVO CONFIG NO ESTA HARDCODEADO,PUEDE SERVIR SI USAMOS
 // DISTINTOS ARCHIVOS CONFIG,PREGUNTAR
   ms ->log_level = config_get_string_value(ms->config,"LOG_LEVEL");
    
   //abajo lleno los campos que vienen del archivo de configuracion memory_stick.config
   
   ms ->puerto_escucha = config_get_string_value(ms->config,"PUERTO_ESCUCHA");
   ms ->ip_kernel_memory = config_get_string_value(ms->config,"IP_KERNEL_MEMORY");
   ms ->puerto_kernel_memory = config_get_string_value(ms->config,"PUERTO_KERNEL_MEMORY");

   //loggeo que todo se cargò correctamente
   log_debug(ms->logger, "El mòdulo Memory Stick se inicializò correctamente");

   return ms;
}


void verificar_memory_stick(t_memory_stick* ms) {
    log_debug(ms->logger, "Puerto de escucha: %s", ms->puerto_escucha);
    log_debug(ms->logger, "IP Kernel Memory: %s", ms->ip_kernel_memory);
    log_debug(ms->logger, "Puerto Kernel Memory: %s", ms->puerto_kernel_memory);
    log_debug(ms->logger, "Log Level: %s", ms->log_level);
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
enviar_paquete(paquete,ms->kernel_mem_socket,ms->logger);
eliminar_paquete(paquete);
log_debug(ms->logger,"**HANDSHAKE KERNEL MEMORY");
}

void rutina_recepcion(t_memory_stick* ms ,int servidor_fd){

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

      t_cpu_context* ctx = malloc(sizeof(t_cpu_context));
      ctx->cpu_id = cpu_id;
      ctx->socket_cliente = temp_socket_cpu;


      if (pthread_create(&hilo_exec,NULL,rutina_operaciones,ctx) != 0){
        log_error(ms->logger , "ERROR AL CREAR HILO SERVIDOR");
        free(ctx);
        close(temp_socket_cpu);
      }else{
      pthread_detach(hilo_exec);
      log_debug(ms->logger , "HILO SERVIDOR CREADO");
      }
      
    }

 }

void* rutina_operaciones (void* args){
  printf("hola!");
  return NULL;
}