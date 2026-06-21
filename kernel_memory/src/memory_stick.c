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
    pthread_mutex_lock(&mutex_lista_ms);
    list_add(lista_ms, ms_info);
    pthread_mutex_unlock(&mutex_lista_ms);
    return 1;
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
    log_info(logger, "=== Lista de Memory Sticks (%d en total) ===", list_size(lista_dir_global_ms));
    for(int i = 0; i < list_size(lista_dir_global_ms); i++) {
        t_ms_pos *ms = list_get(lista_dir_global_ms, i);
        log_info(logger, "  [%d] MS ID:%d | base global:%u | limite global:%u",
                 i, ms->id, ms->base_global, ms->limite_global);
    }
    log_info(logger, "==========================================");
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

t_list* calcular_dir_local_ms(uint32_t dir_fisica_global, uint32_t tamano_contenido, t_log* logger) {

    log_info(logger, "== Iniciando mapeo global -> local: Dir Global: %u | Tamaño Total: %d bytes ==",dir_fisica_global, tamano_contenido);

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

    log_info(logger, "Mapeo finalizado con éxito. El contenido se dividió en %d fragmento(s).",contador_fragmentos);

    return lista_fragmentos_temp;
}

void enviar_fragmentos_escritura(t_list *lista_fragmentos, char *contenido_a_escribir, t_log *logger)
{
    int offset_contenido = 0;

    for (int i = 0; i < list_size(lista_fragmentos); i++)
    {
        t_fragmento_memoria *frag = list_get(lista_fragmentos, i);

        int socket_ms = buscar_socket_ms_por_id(frag->ms_id);

        if (socket_ms == -1)
        {
            log_error(logger, "No se encontró el socket para el Memory Stick ID:%d", frag->ms_id);
            continue;
        }

        char *datos_fragmentados = contenido_a_escribir + offset_contenido;

        t_paquete *paquete_ms = crear_paquete(ESCRITURA_EN_MS, crear_buffer());
        agregar_a_paquete(paquete_ms, &(frag->dir_local), sizeof(uint32_t));
        agregar_a_paquete(paquete_ms, &(frag->tamano), sizeof(int));
        agregar_a_paquete(paquete_ms, datos_fragmentados, frag->tamano);
        enviar_paquete(paquete_ms, socket_ms, logger);
        eliminar_paquete(paquete_ms);

        log_info(logger, "Enviado fragmento %d al MS ID:%d | Tam: %d bytes en Dir Local: %u",
                 i, frag->ms_id, frag->tamano, frag->dir_local);

        offset_contenido += frag->tamano;
    }
}

void* enviar_fragmentos_lectura(t_list* lista_fragmentos, uint32_t tamano_total, t_log* logger) {
    
    void* buffer_completo = malloc(tamano_total);
    if (buffer_completo == NULL) {
        log_error(logger, "Error: No se pudo asignar memoria para el buffer de lectura completo");
        return NULL;
    }

    int offset_armado = 0; 

    for (int i = 0; i < list_size(lista_fragmentos); i++) {
        t_fragmento_memoria* frag = list_get(lista_fragmentos, i);

        int socket_ms = buscar_socket_ms_por_id(frag->ms_id);
        if (socket_ms == -1) {
            log_error(logger, "Error: No se encontró el socket para el MS ID:%d", frag->ms_id);
            free(buffer_completo);
            return NULL;
        }

        t_paquete* paquete_peticion = crear_paquete(LECTURA_DE_DATOS, crear_buffer());//este es el protocolo correcto?
        agregar_a_paquete(paquete_peticion, &(frag->dir_local), sizeof(uint32_t));
        agregar_a_paquete(paquete_peticion, &(frag->tamano), sizeof(uint32_t));

        log_debug(logger, "Solicitando fragmento %d al MS ID %d (%u bytes desde dir local %u)", 
                  i, frag->ms_id, frag->tamano, frag->dir_local);

        enviar_paquete(paquete_peticion, socket_ms, logger);
        eliminar_paquete(paquete_peticion);

        uint32_t cod_op;
        if (recv(socket_ms, &cod_op, sizeof(uint32_t), MSG_WAITALL) <= 0) {
            log_error(logger, "Error al recibir código de operación del MS ID:%d", frag->ms_id);
            free(buffer_completo);
            return NULL;
        }

        t_list* paquete_respuesta = recibir_paquete(socket_ms);
        
        void* datos_leidos_ms = list_get(paquete_respuesta, 1);

        memcpy(buffer_completo + offset_armado, datos_leidos_ms, frag->tamano);

        log_debug(logger, "Fragmento %d recibido y acoplado en el offset %d", i, offset_armado);

        offset_armado += frag->tamano;

        list_destroy_and_destroy_elements(paquete_respuesta, free);
    }

    log_info(logger, "Lectura fragmentada unificada con éxito (%d bytes totales)", tamano_total);

    return buffer_completo;
}
