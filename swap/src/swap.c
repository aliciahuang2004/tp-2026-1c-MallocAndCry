#include "swap.h"

t_swap* inicializar_swap(int argc, char* argv[] ) {

    t_swap* sp= malloc(sizeof(t_swap));

    t_log* logger_temp= iniciar_logger("swap.log", "[SWAP_INIT]", true, LOG_LEVEL_INFO);

    sp-> config= iniciar_config(logger_temp, argv[1]);

    sp-> log_level= strdup(config_get_string_value(sp->config, "LOG_LEVEL"));
    t_log_level nivel = obtener_log_level(sp->log_level);

    sp-> logger= iniciar_logger("swap.log", "[SWAP]", true, nivel);

    log_destroy(logger_temp);

    sp-> swap_file_path= config_get_string_value(sp->config, "SWAP_FILE_PATH");
    sp-> swap_file_size= config_get_int_value(sp->config, "SWAP_FILE_SIZE");
    sp-> block_size= config_get_int_value(sp->config, "BLOCK_SIZE");
    sp-> ip_kernel_memory= config_get_string_value(sp->config, "IP_KERNEL_MEMORY");
    sp-> puerto_kernel_memory= config_get_int_value(sp->config, "PUERTO_KERNEL_MEMORY");
    sp-> socket_kernel_memory= -1;

    // Inicializamos el puntero en NULL por si falla la creación
    sp->archivo_swap = NULL;

    // Ejecutamos la creación del archivo físico de SWAP
    if (crear_archivo_swap(sp) == -1) {
        log_error(sp->logger, "Error crítico al inicializar el archivo físico de SWAP.");
    }

    return sp;
}

int crear_archivo_swap(t_swap* sp) {
    
    sp->archivo_swap = fopen(sp->swap_file_path, "w+b");
    if (sp->archivo_swap == NULL) {
        log_error(sp->logger, "No se pudo abrir/crear el archivo de SWAP en: %s", sp->swap_file_path);
        return -1;
    }

    if (fseek(sp->archivo_swap, sp->swap_file_size - 1, SEEK_SET) != 0) {
        log_error(sp->logger, "Error al usar fseek para reservar espacio");
        fclose(sp->archivo_swap);
        sp->archivo_swap = NULL;
        return -1;
    }

    fputc('\0', sp->archivo_swap);

    rewind(sp->archivo_swap);

    log_info(sp->logger, "Archivo físico de SWAP creado correctamente en: %s", sp->swap_file_path);
    return 0;
}

void verificar_swap(t_swap* sp){
    log_debug(sp->logger, "SWAP inicializado correctamente");
    log_debug(sp->logger, "Archivo de swap: %s", sp->swap_file_path);
    log_debug(sp->logger, "Tamaño del swap: %d bytes", sp->swap_file_size);
    log_debug(sp->logger, "Tamaño del bloque: %d bytes", sp->block_size);
    log_debug(sp->logger, "IP del Kernel Memory: %s", sp->ip_kernel_memory);
    log_debug(sp->logger, "Puerto del Kernel Memory: %d", sp->puerto_kernel_memory);
}

void liberar_swap(t_swap* sp){
    if(!sp) return;

    if (sp->archivo_swap != NULL) {
        fclose(sp->archivo_swap);
    }

    if(sp->logger) log_destroy(sp->logger);
    if(sp->config) config_destroy(sp->config);

    if(sp->log_level) free(sp->log_level);
    free(sp);
}

int conectar_a_kernel_memory(t_swap* sp){
    char* puerto_str = string_itoa(sp->puerto_kernel_memory);

    sp->socket_kernel_memory = crear_conexion(sp->logger, sp->ip_kernel_memory, puerto_str);

    free(puerto_str);

    if(sp->socket_kernel_memory != -1){
        log_info(sp->logger, "## Conectado a Kernel Memory");
        return 0;
    } else {
        log_error(sp->logger, "Error al conectar a Kernel Memory en %s:%d", sp->ip_kernel_memory, sp->puerto_kernel_memory);
        return -1;
      
    }
}

void enviar_handshake(t_swap* sp){
    t_paquete* paquete = crear_paquete(SWAP_HANDSHAKE,crear_buffer());
    
    enviar_paquete(paquete, sp->socket_kernel_memory, sp->logger);
    eliminar_paquete(paquete);
    log_debug(sp->logger, "HANSHAKE KERNEL MEMORY ENVIADO");
}

void enviar_tamanio_bloque(t_swap* sp){
    t_buffer* buffer = crear_buffer();

    t_paquete* paquete =crear_paquete(SWAP_REQUEST, buffer);

    int tamanio_bloque=sp->block_size;
    int tamanio_total=sp->swap_file_size; //VER COMO CALCULO EL TAMAÑO TOTAL 
    
    agregar_a_paquete(paquete, &tamanio_bloque, sizeof(int));
    agregar_a_paquete(paquete, &tamanio_total, sizeof(int));

    int resultado = enviar_paquete(paquete, sp->socket_kernel_memory, sp->logger);

    if (resultado == 0) {
        log_info(sp->logger, "Tamaño de bloque y total enviados correctamente a Kernel Memory");
    } else {
        log_error(sp->logger, "Error al enviar tamaño de bloque y total a Kernel Memory");
    }

    eliminar_paquete(paquete);
}

void escribir_bloque(t_swap* sp, int numero_bloque, void* contenido) {
    
    long offset = numero_bloque * sp->block_size;

    fseek(sp->archivo_swap, offset, SEEK_SET);

    fwrite(contenido, 1, sp->block_size, sp->archivo_swap);
    
    fflush(sp->archivo_swap); 

    log_info(sp->logger, "## Escritura del bloque: %d", numero_bloque);

    t_paquete* paquete_respuesta = crear_paquete(DATOS_LEIDOS, crear_buffer()); 
   
    enviar_paquete(paquete_respuesta, sp->socket_kernel_memory, sp->logger);
    eliminar_paquete(paquete_respuesta);
}

void leer_bloque(t_swap* sp, int numero_bloque) {
   
    long offset = numero_bloque * sp->block_size;

    fseek(sp->archivo_swap, offset, SEEK_SET);

    void* buffer_lectura = malloc(sp->block_size);
    fread(buffer_lectura, 1, sp->block_size, sp->archivo_swap);

    log_info(sp->logger, "## Lectura del bloque: %d", numero_bloque);

    t_paquete* paquete_respuesta = crear_paquete(DATOS_LEIDOS, crear_buffer());
    agregar_a_paquete(paquete_respuesta, buffer_lectura, sp->block_size);
    
    enviar_paquete(paquete_respuesta, sp->socket_kernel_memory, sp->logger);
    
    eliminar_paquete(paquete_respuesta);
    free(buffer_lectura);
}

void atender_kernel_memory(t_swap* sp) {
    bool conectado = true;
    
    log_info(sp->logger, "Esperando peticiones de Kernel Memory...");

    while (conectado) {
        t_list* paquete = recibir_paquete(sp->socket_kernel_memory);

        if (paquete == NULL) {
            log_warning(sp->logger, "El Kernel Memory se ha desconectado.");
            conectado = false;
            break;
        }

        int* op_code_ptr = list_get(paquete, 0);
        int cod_op = *op_code_ptr;

        int* nro_bloque_ptr;
        void* contenido;

        switch ((op_code)cod_op) {
            case ESCRITURA_EN_MS: 
                nro_bloque_ptr = list_get(paquete, 1);
                contenido = list_get(paquete, 2);
                
                escribir_bloque(sp, *nro_bloque_ptr, contenido);
                break;

            case LECTURA_DE_DATOS: 
                nro_bloque_ptr = list_get(paquete, 1);
                
                leer_bloque(sp, *nro_bloque_ptr);
                break;

            default:
                log_warning(sp->logger, "Operación desconocida de Kernel Memory: %d", cod_op);
                break;
        }

        list_destroy_and_destroy_elements(paquete, free);
    }
}