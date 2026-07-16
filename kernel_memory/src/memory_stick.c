#include "memory_stick.h"
#include "kernel_memory.h"
#include "estructuras.h"


int nuevo_memory_stick(int ms_id, int ms_tamano, int socket_cliente) {
    t_ms_info* ms_info = malloc(sizeof(t_ms_info));

    if (ms_info == NULL) {
        return 0;
    }

    memset(ms_info, 0, sizeof(t_ms_info));
    ms_info->id = ms_id;
    ms_info->tamano = ms_tamano;
    ms_info->socket = socket_cliente;
    ms_info->ultimo_codigo_op = 0;
    // INICIALIZACIÓN DE SINCRONIZACIÓN
    pthread_mutex_init(&ms_info->mutex_socket, NULL);
    pthread_cond_init(&ms_info->cond_respuesta, NULL);
    ms_info->respuesta_lista = false;

    pthread_mutex_lock(&mutex_lista_ms);
    list_add(lista_ms, ms_info);
    pthread_mutex_unlock(&mutex_lista_ms);
    return 1;
}

int guardar_ms_conexion(int id, char* ip, char* puerto) {
    t_ms_conexion* ms_conexion = malloc(sizeof(t_ms_conexion));

    if (ms_conexion == NULL)
        return 0;
    memset(ms_conexion, 0, sizeof(t_ms_conexion));
    ms_conexion->id = id;
    ms_conexion->ip = strdup(ip);
    ms_conexion->puerto = strdup(puerto);

    if (ms_conexion->ip == NULL || ms_conexion->puerto == NULL) {
        free(ms_conexion->ip);
        free(ms_conexion->puerto);
        free(ms_conexion);
        return 0;
    }

    pthread_mutex_lock(&mutex_lista_ms_conexion);
    list_add(lista_ms_conexion, ms_conexion);
    pthread_mutex_unlock(&mutex_lista_ms_conexion);

    return 1;
}

void enviar_ms_a_cpu(int socket_cpu, t_log* logger) {
    pthread_mutex_lock(&mutex_lista_dir_global_ms);
    pthread_mutex_lock(&mutex_lista_ms_conexion);

    int total_ms = list_size(lista_dir_global_ms);
    log_debug(logger, "Enviando %d Memory Sticks ya conectados a la nueva CPU (Socket: %d)", total_ms, socket_cpu);

    for (int i = 0; i < total_ms; i++) {
        t_ms_pos* ms_pos = list_get(lista_dir_global_ms, i);
        
        char* ms_ip = NULL;
        char* ms_puerto = NULL;

        for (int j = 0; j < list_size(lista_ms_conexion); j++) {
            t_ms_conexion* ms_con = list_get(lista_ms_conexion, j);
            if (ms_con->id == ms_pos->id) {
                ms_ip = ms_con->ip;
                ms_puerto = ms_con->puerto;
                break;
            }
        }

        if (ms_ip == NULL || ms_puerto == NULL) {
            log_error(logger, "No se encontraron datos de conexión para MS ID: %d", ms_pos->id);
            continue;
        }

        t_paquete* paquete = crear_paquete(MS_NUEVO_CPU, crear_buffer());
        
        agregar_a_paquete(paquete, &(ms_pos->id), sizeof(int));
        agregar_a_paquete(paquete, ms_puerto, strlen(ms_puerto) + 1);
        agregar_a_paquete(paquete, ms_ip, strlen(ms_ip) + 1);
        agregar_a_paquete(paquete, &(ms_pos->base_global), sizeof(uint32_t));
        agregar_a_paquete(paquete, &(ms_pos->limite_global), sizeof(uint32_t));
        log_debug(logger, "Antes de enviar a CPU: ip='%s' puerto='%s'", ms_ip, ms_puerto);
        enviar_paquete(paquete, socket_cpu, logger);
        log_debug(logger, "Enviando Memory Stick: Puerto:%s e IP:%s a la nueva CPU (Socket: %d)",ms_puerto,ms_ip, socket_cpu);
        eliminar_paquete(paquete);

        log_debug(logger, "  [%d/%d] MS ID:%d enviado exitosamente a CPU", i + 1, total_ms, ms_pos->id);
    }

    pthread_mutex_unlock(&mutex_lista_ms_conexion);
    pthread_mutex_unlock(&mutex_lista_dir_global_ms);
}

t_ms_info *buscar_ms_por_socket(int socket)
{
    pthread_mutex_lock(&mutex_lista_ms);

    for (int i = 0; i < list_size(lista_ms); i++)
    {
        t_ms_info *ms = list_get(lista_ms, i);

        if (ms->socket == socket)
        {
            pthread_mutex_unlock(&mutex_lista_ms);
            return ms;
        }
    }

    pthread_mutex_unlock(&mutex_lista_ms);
    return NULL;
}

void manejar_desconexion_memory_stick(t_ms_info* ms, t_kernel_memory* km, t_log* logger) {
    log_error(logger, "Memory Stick ID:%d desconectado - Informando corrupción al Kernel Scheduler", ms->id);

    t_paquete* aviso = crear_paquete(CORRUPCION_MEMORIA, crear_buffer());
    enviar_paquete(aviso, km->socket_kernel_scheduler, logger);
    eliminar_paquete(aviso);
}

void agregar_posicion_ms(t_resultado_hueco r,int ms_id,t_log* logger){
    t_ms_pos* ms_pos = malloc(sizeof(t_ms_pos));
    memset(ms_pos, 0, sizeof(t_ms_pos));
    ms_pos->id = ms_id;
    ms_pos->base_global = r.base;
    ms_pos->limite_global = r.limite;
    pthread_mutex_lock(&mutex_lista_dir_global_ms);
    list_add(lista_dir_global_ms, ms_pos);
    //****************log temp************
    log_debug(logger, "=== Lista de Memory Sticks (%d en total) ===", list_size(lista_dir_global_ms));
    for(int i = 0; i < list_size(lista_dir_global_ms); i++) {
        t_ms_pos *ms = list_get(lista_dir_global_ms, i);
        log_debug(logger, "  [%d] MS ID:%d | base global:%u | limite global:%u",
                 i, ms->id, ms->base_global, ms->limite_global);
    }
    log_debug(logger, "==========================================");
    //****************log temp**************
    pthread_mutex_unlock(&mutex_lista_dir_global_ms);
}

int buscar_socket_ms_por_id(int ms_id)
{
    pthread_mutex_lock(&mutex_lista_ms);

    for (int i = 0; i < list_size(lista_ms); i++)
    {
        t_ms_info *ms = list_get(lista_ms, i);

        if (ms->id == ms_id)
        {
            int socket_encontrado = ms->socket;

            pthread_mutex_unlock(&mutex_lista_ms);
            return socket_encontrado;
        }
    }

    pthread_mutex_unlock(&mutex_lista_ms);
    return -1;
}
t_ms_info* buscar_ms_por_id(int ms_id) {
    pthread_mutex_lock(&mutex_lista_ms);

    for (int i = 0; i < list_size(lista_ms); i++) {
        t_ms_info* ms = list_get(lista_ms, i);

        if (ms->id == ms_id) {
            pthread_mutex_unlock(&mutex_lista_ms);
            return ms; 
        }
    }

    pthread_mutex_unlock(&mutex_lista_ms);
    return NULL;
}
t_list* calcular_dir_local_ms(uint32_t dir_fisica_global, uint32_t tamano_contenido, t_log* logger) {

    log_debug(logger, "== Iniciando mapeo global -> local: Dir Global: %u | Tamaño Total: %d bytes ==",dir_fisica_global, tamano_contenido);

    t_list* lista_fragmentos_temp = list_create();
    int bytes_restantes = tamano_contenido;
    uint32_t dir_actual_global = dir_fisica_global;
    int contador_fragmentos = 0;

    pthread_mutex_lock(&mutex_lista_dir_global_ms);

    while (bytes_restantes > 0) {
        t_ms_pos* ms_encontrado = NULL;

        for (int i = 0; i < list_size(lista_dir_global_ms); i++) {
            t_ms_pos* ms = list_get(lista_dir_global_ms, i);

            if (dir_actual_global >= ms->base_global && dir_actual_global <= ms->limite_global) {
                ms_encontrado = ms;
                break; 
            }
        }
        if (ms_encontrado == NULL) {
            log_error(logger, "ERROR CRÍTICO: La dirección global %u no pertenece a ningún Memory Stick activo. Faltaron %d bytes por mapear.", 
                      dir_actual_global, bytes_restantes);
            
            list_destroy_and_destroy_elements(lista_fragmentos_temp, free);
            pthread_mutex_unlock(&mutex_lista_dir_global_ms);
            return NULL;
        }

        uint32_t dir_local = dir_actual_global - ms_encontrado->base_global;
        uint32_t espacio_disponible_ms = (ms_encontrado->limite_global - ms_encontrado->base_global + 1) - dir_local;
        uint32_t bytes_a_copiar = (bytes_restantes < espacio_disponible_ms) ? bytes_restantes : espacio_disponible_ms;

        if (bytes_a_copiar < (uint32_t)bytes_restantes) {
            log_warning(logger, "Desborde detectado en MS ID %d! Espacio disponible (%u bytes) es menor que los bytes restantes (%d bytes). Se cortará el contenido.", 
                        ms_encontrado->id, espacio_disponible_ms, bytes_restantes);
        }

        t_fragmento_memoria* frag = malloc(sizeof(t_fragmento_memoria));
        memset(frag, 0, sizeof(t_fragmento_memoria)); 
        frag->ms_id = ms_encontrado->id;
        frag->dir_local = dir_local;
        frag->tamano = bytes_a_copiar;
        frag->datos_bloque = NULL; 
        list_add(lista_fragmentos_temp, frag);
        
        contador_fragmentos++;

        log_debug(logger, "Fragmento %d generado -> MS ID: %d | Dir Local: %u | Tamaño: %u bytes", contador_fragmentos, frag->ms_id, frag->dir_local, frag->tamano);

        bytes_restantes -= bytes_a_copiar;       
        dir_actual_global += bytes_a_copiar;     
    }

    pthread_mutex_unlock(&mutex_lista_dir_global_ms);

    log_debug(logger, "Mapeo finalizado con éxito. El contenido se dividió en %d fragmento(s).",contador_fragmentos);

    return lista_fragmentos_temp;
}
void enviar_fragmentos_escritura(t_list *lista_fragmentos, char *contenido_a_escribir, t_log *logger, t_kernel_memory* km) {
    int offset_contenido = 0;

    for (int i = 0; i < list_size(lista_fragmentos); i++) {
        t_fragmento_memoria *frag = list_get(lista_fragmentos, i);
        
        // Buscamos la estructura de control del MS (que ahora tiene el mutex y cond_var)
        //int socket_ms = buscar_socket_ms_por_id(frag->ms_id);
        t_ms_info* ms = buscar_ms_por_id(frag->ms_id); 

        if (ms == NULL) {
            log_error(logger, "No se encontró el MS ID:%d", frag->ms_id);
            continue;
        }

        pthread_mutex_lock(&ms->mutex_socket); // Bloqueamos el acceso al socket del MS

        char *datos_fragmentados = contenido_a_escribir + offset_contenido;
        t_paquete *paquete_ms = crear_paquete(ESCRITURA_DE_DATOS, crear_buffer());
        agregar_a_paquete(paquete_ms, &(frag->dir_local), sizeof(uint32_t));
        agregar_a_paquete(paquete_ms, &(frag->tamano), sizeof(int));
        agregar_a_paquete(paquete_ms, datos_fragmentados, frag->tamano);
        
        enviar_paquete(paquete_ms, ms->socket, logger);
        eliminar_paquete(paquete_ms);

        log_debug(logger, "Enviado fragmento %d al MS ID:%d. Esperando confirmación...", i, ms->id);

        ms->respuesta_lista = false; 
        while (!ms->respuesta_lista) {
            pthread_cond_wait(&ms->cond_respuesta, &ms->mutex_socket);
        }

        if (ms->ultimo_codigo_op == IO_OK) {
            log_debug(logger, "MS ID:%d confirmó la operación vía Dispatcher.", ms->id);
        } else {
            log_error(logger, "Error en la operación del MS ID:%d", ms->id);
        }

        pthread_mutex_unlock(&ms->mutex_socket); // Liberamos el socket para el siguiente fragmento
        offset_contenido += frag->tamano;
    }
}

void* enviar_fragmentos_lectura(t_list* lista_fragmentos, uint32_t tamano_total, t_log* logger, t_kernel_memory* km)
{
    void* buffer_completo = malloc(tamano_total);
    if (buffer_completo == NULL) {
        log_error(logger, "Error: No se pudo asignar memoria para el buffer de lectura");
        return NULL;
    }
    memset(buffer_completo, 0, tamano_total);
    int offset_armado = 0;

    for (int i = 0; i < list_size(lista_fragmentos); i++) {
        t_fragmento_memoria* frag = list_get(lista_fragmentos, i);

        t_ms_info* ms = buscar_ms_por_id(frag->ms_id);
        if (ms == NULL) {
            log_error(logger, "No se encontró el MS ID:%d", frag->ms_id);
            free(buffer_completo);
            return NULL;
        }

        pthread_mutex_lock(&ms->mutex_socket);

        t_paquete* paquete_peticion = crear_paquete(LECTURA_DE_DATOS, crear_buffer());
        agregar_a_paquete(paquete_peticion, &(frag->dir_local), sizeof(uint32_t));
        agregar_a_paquete(paquete_peticion, &(frag->tamano), sizeof(int));

        log_debug(logger, "Solicitando fragmento %d al MS %d (%d bytes)", i, ms->id, frag->tamano);
        enviar_paquete(paquete_peticion, ms->socket, logger);
        eliminar_paquete(paquete_peticion);

        ms->respuesta_lista = false;
        while (!ms->respuesta_lista) {
            pthread_cond_wait(&ms->cond_respuesta, &ms->mutex_socket);
        }

        if (ms->ultimo_codigo_op == DATOS_LEIDOS && ms->buffer_respuesta != NULL) {
            memcpy((char*)buffer_completo + offset_armado, ms->buffer_respuesta, frag->tamano);
            log_debug(logger, "Fragmento %d recibido vía Dispatcher y copiado", i);
            
            free(ms->buffer_respuesta);
            ms->buffer_respuesta = NULL;
        } else {
            log_error(logger, "Error en lectura: El MS %d se desconectó o falló", ms->id);
            pthread_mutex_unlock(&ms->mutex_socket);
            free(buffer_completo);
            return NULL;
        }

        offset_armado += frag->tamano;
        pthread_mutex_unlock(&ms->mutex_socket);
    }

    log_debug(logger, "Lectura fragmentada unificada con éxito (%u bytes)", tamano_total);
    return buffer_completo;
}