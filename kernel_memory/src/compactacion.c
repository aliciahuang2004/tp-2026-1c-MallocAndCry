#include "compactacion.h"
#include "kernel_memory.h"
#include "estructuras.h"
#include "huecos.h"
#include "memory_stick.h"
#include "conexiones.h"

void avisar_compactacion(t_kernel_memory* km, t_log* logger) {
    log_debug(logger, "## Solicitando compactación al Kernel Scheduler");
    t_paquete* aviso = crear_paquete(INICIAR_COMPACTACION, crear_buffer());
    enviar_paquete(aviso, km->socket_kernel_scheduler, logger);
    eliminar_paquete(aviso);
}

bool ordenar_por_base_global(void* a, void* b) {
    t_segmento* seg_a = (t_segmento*)a;
    t_segmento* seg_b = (t_segmento*)b;

    return seg_a->base_global < seg_b->base_global;
}

t_list* obtener_lista_temp_ord(t_list* lista_segmentos_temp, t_log* logger) {
    
    pthread_mutex_lock(&mutex_tabla_contextos);
    t_list* lista_contextos_temp = dictionary_elements(tabla_contextos);
    pthread_mutex_unlock(&mutex_tabla_contextos);

    for (int i = 0; i < list_size(lista_contextos_temp); i++) {
        t_contexto* contexto = list_get(lista_contextos_temp, i);

        for (int j = 0; j < list_size(contexto->tabla_segmentos); j++) {
            t_segmento* seg = list_get(contexto->tabla_segmentos, j);

            if (seg->en_swap) {
                continue;
            }
            list_add(lista_segmentos_temp, seg);
        }
    }

    list_destroy(lista_contextos_temp);

    log_debug(logger, "[COMPACTACIÓN] Segmentos totales cargados: %d", list_size(lista_segmentos_temp));


    if (list_size(lista_segmentos_temp) > 1) {
        list_sort(lista_segmentos_temp, ordenar_por_base_global);
        log_debug(logger, "[COMPACTACIÓN] Lista de segmentos ordenada con éxito.");
    }

    return lista_segmentos_temp;
}
int iniciar_compactacion(t_log* logger,t_kernel_memory* km) {
    log_debug(logger, "== [COMPACTACIÓN] Solicitud de compactación recibida ==");
    usleep(km->compaction_delay * 1000); 
    t_list* lista_ordenada = obtener_lista_temp_ord(list_create(), logger);

    uint32_t proxima_base_libre = 0;

    for (int i = 0; i < list_size(lista_ordenada); i++) {
        t_segmento* seg = list_get(lista_ordenada, i);
        uint32_t tamano_segmento = (seg->limite_global - seg->base_global) + 1;

        if (seg->base_global == proxima_base_libre) {
            proxima_base_libre = seg->limite_global + 1;
            continue; 
        }

        uint32_t base_vieja = seg->base_global;

        t_list* frag_lectura = calcular_dir_local_ms(base_vieja, tamano_segmento, logger);
        void* contenido_temporal = enviar_fragmentos_lectura(frag_lectura, tamano_segmento, logger,km);
        list_destroy_and_destroy_elements(frag_lectura, free);

        if (contenido_temporal == NULL) {
            log_error(logger, "[COMPACTACIÓN] Error al leer el contenido del Segmento ID %d", seg->id_segmento);
            list_destroy(lista_ordenada); 
            return -1;
        }

        seg->base_global = proxima_base_libre;
        seg->limite_global = proxima_base_libre + (tamano_segmento - 1);

        t_list* frag_escritura = calcular_dir_local_ms(seg->base_global, tamano_segmento, logger);
        enviar_fragmentos_escritura(frag_escritura, contenido_temporal, logger,km);
        list_destroy_and_destroy_elements(frag_escritura, free);

        free(contenido_temporal);
        proxima_base_libre = seg->limite_global + 1;
    }
    
    vaciar_lista_de_huecos(logger);

    uint32_t tamano_gran_hueco = memoria_total - proxima_base_libre;

    if (tamano_gran_hueco > 0) {
        log_debug(logger, "[COMPACTACIÓN] Creando el gran hueco libre. Base: %u | Tamaño: %u bytes",proxima_base_libre, tamano_gran_hueco);
                 
        agregar_hueco_libre(proxima_base_libre, tamano_gran_hueco);
    } else {
        log_warning(logger, "[COMPACTACIÓN] La memoria está 100%% llena. No quedó espacio libre para un hueco.");
    }

    list_destroy(lista_ordenada);

    log_debug(logger, "== [COMPACTACIÓN] Proceso finalizado de forma exitosa ==");
    return 1;
}
