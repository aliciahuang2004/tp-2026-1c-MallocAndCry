#include "segmentos.h"
#include "kernel_memory.h"
#include "estructuras.h"
#include "huecos.h"
#include "procesos.h"


int crear_segmento(int pid, int id_segmento, uint32_t tamano,t_log* logger) {
    pthread_mutex_lock(&mutex_huecos);

    t_hueco* hueco = buscar_hueco(tamano,logger);

    if(hueco == NULL) {
        pthread_mutex_unlock(&mutex_huecos);
        return -1;
    }

    uint32_t base_segmento = hueco->base;

    consumir_hueco(hueco, tamano);

    pthread_mutex_unlock(&mutex_huecos);

    pthread_mutex_lock(&mutex_procesos);
    t_proceso* proceso = buscar_proceso(pid);

    if(proceso == NULL) {
        pthread_mutex_unlock(&mutex_procesos);
        return -2;
    }

    t_segmento* segmento = malloc(sizeof(t_segmento));

    segmento->id_segmento = id_segmento;
    segmento->base = base_segmento;
    segmento->limite = tamano;
    segmento->memory_stick_id = -1;//VER SI ESTE DATO ES NECESARIO TENERLO ACÁ
    segmento->en_swap = false;
    segmento->bloque_swap = -1;

    list_add(proceso->contexto->tabla_segmentos, segmento);
    pthread_mutex_unlock(&mutex_procesos);
    log_info(logger,"Segmento creado PID:%d SEG:%d BASE:%u LIMITE:%u",pid,id_segmento,segmento->base,segmento->limite);//para probar

    loguear_segmentos_proceso(proceso, logger);//para probar

    return 1;
}

void loguear_segmentos_proceso(t_proceso* proceso, t_log* logger) {//para probar

    log_info(logger,"----- SEGMENTOS PID %d -----",proceso->pid);

    for(int i = 0; i < list_size(proceso->contexto->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proceso->contexto->tabla_segmentos, i);

        log_info(logger,"SEG:%d BASE:%u LIMITE:%u",seg->id_segmento,seg->base,seg->limite);
    }
}

//busca al proceso,busca el seg por id en su tabla,guarda base y limite (dir logica?),elimina al seg de la tabla,agrega hueco libre
int eliminar_segmento(int pid, int id_segmento, t_log* logger) {
    pthread_mutex_lock(&mutex_procesos);
    
    t_proceso* proceso = buscar_proceso(pid);
    if (proceso == NULL) {
        pthread_mutex_unlock(&mutex_procesos);
        log_error(logger, "Proceso PID:%d no encontrado", pid);
        return -1;
    }

    t_segmento* segmento_encontrado = NULL;
    int indice = -1;
    for (int i = 0; i < list_size(proceso->contexto->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proceso->contexto->tabla_segmentos, i);
        if (seg->id_segmento == id_segmento) {
            segmento_encontrado = seg;
            indice = i;
            break;
        }
    }

    if (segmento_encontrado == NULL) {
        pthread_mutex_unlock(&mutex_procesos);
        log_error(logger, "Segmento ID:%d no encontrado en PID:%d", id_segmento, pid);
        return -2;
    }

    uint32_t base = segmento_encontrado->base;
    uint32_t limite = segmento_encontrado->limite;

    list_remove_and_destroy_element(proceso->contexto->tabla_segmentos, indice, free);

    pthread_mutex_unlock(&mutex_procesos);

    pthread_mutex_lock(&mutex_huecos);
    agregar_hueco_libre(base, limite);
    pthread_mutex_unlock(&mutex_huecos);

    log_info(logger, "Segmento eliminado PID:%d SEG:%d BASE:%u LIMITE:%u", pid, id_segmento, base, limite);

    return 1;
}
