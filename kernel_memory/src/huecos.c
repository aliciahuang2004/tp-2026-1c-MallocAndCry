#include "huecos.h"
#include "kernel_memory.h"
#include "estructuras.h"
#include <commons/collections/dictionary.h>
#include "compactacion.h"

t_resultado_hueco agregar_hueco_libre(uint32_t base, uint32_t tamano)
{
    t_resultado_hueco resultado = {0, 0}; // valores por defecto si falla

    t_hueco* hueco = malloc(sizeof(t_hueco));
    if(hueco == NULL)
        return resultado;

    hueco->base   = base;
    hueco->tamano = tamano;
    hueco->limite = base + tamano - 1;

    resultado.base   = hueco->base;
    resultado.limite = hueco->limite;

    pthread_mutex_lock(&mutex_huecos);
    list_add(lista_huecos_libres, hueco);
    pthread_mutex_unlock(&mutex_huecos);

    return resultado;
}


void consumir_hueco(t_hueco* hueco, uint32_t tamano) {//revisar que hace esta funcion

    hueco->base += tamano;
    hueco->tamano -= tamano;
    hueco->limite = hueco->base + hueco->tamano - 1;
    
    if(hueco->tamano == 0) {
        list_remove_element(lista_huecos_libres, hueco);
        free(hueco);
    }

}

t_hueco* buscar_hueco(uint32_t tamano,t_kernel_memory* km,t_log* logger) {

    if(strcmp(km->allocation_strategy, "BEST") == 0)
        return buscar_hueco_best_fit(tamano,km,logger);

    return buscar_hueco_worst_fit(tamano,km,logger);
}

t_hueco* buscar_hueco_best_fit(uint32_t tamano,t_kernel_memory* km, t_log* logger) {
    t_hueco* mejor = NULL;
    uint32_t total_huecos = 0;

    for (int i = 0; i < list_size(lista_huecos_libres); i++) {
        t_hueco* hueco = list_get(lista_huecos_libres, i);
        total_huecos += hueco->tamano;  // ← acumulás siempre
        if (hueco->tamano >= tamano) {
            if (mejor == NULL || hueco->tamano < mejor->tamano) {
                mejor = hueco;
            }
        }
    }

    if (mejor != NULL) {
        log_debug(logger, "ALGORITMO BEST FIT eligió hueco Base:%u Tamaño:%u", mejor->base, mejor->tamano);
    } else {
        log_warning(logger, "BEST FIT no encontro hueco");
        if (total_huecos >= tamano) {
            log_debug(logger, "Hay %u bytes libres no contiguos - Se requiere compactación", total_huecos);
            avisar_compactacion(km,logger);
        } else {
            log_warning(logger, "No hay memoria suficiente. Disponible: %u bytes, Requerido: %u bytes", total_huecos, tamano);
        }
    }
    return mejor;
}

t_hueco* buscar_hueco_worst_fit(uint32_t tamano,t_kernel_memory* km ,t_log* logger) {
    t_hueco* peor = NULL;
    uint32_t total_huecos = 0;

    for (int i = 0; i < list_size(lista_huecos_libres); i++) {
        t_hueco* hueco = list_get(lista_huecos_libres, i);
        total_huecos += hueco->tamano;  // acumulo todos los huecos libres en caso de necesitarlo
        if (hueco->tamano >= tamano) {
            if (peor == NULL || hueco->tamano > peor->tamano) {
                peor = hueco;
            }
        }
    }

    if (peor != NULL) {
        log_debug(logger, "ALGORITMO WORST FIT eligió hueco con Base:%u Tamaño:%u", peor->base, peor->tamano);
    } else {
        log_warning(logger, "WORST FIT no encontro hueco");
        if (total_huecos >= tamano) {
            log_debug(logger, "Hay %u bytes libres no contiguos - Se requiere compactación", total_huecos);
            avisar_compactacion(km,logger);
        } else {
            log_warning(logger, "No hay memoria suficiente. Disponible: %u bytes, Requerido: %u bytes", total_huecos, tamano);
            //ver "qué hacer" cuando no hay espacio disponible (SEGUN  ISSUE NO VA A PASAR QUE NO HAYA ESPACIO DISPONIBLE PARA CREAR UN SEGMENTO)
        }
    }
    return peor;
}

void vaciar_lista_de_huecos(t_log* logger) {
    
    pthread_mutex_lock(&mutex_huecos);
    list_clean_and_destroy_elements(lista_huecos_libres, free);
    pthread_mutex_unlock(&mutex_huecos);
    log_debug(logger, "[COMPACTACIÓN] Lista de huecos liberada y vaciada por completo para compactar.");
}

void loguear_huecos(t_log* logger)
{
    log_debug(logger, "----- LISTA DE HUECOS -----");
    for(int i = 0; i < list_size(lista_huecos_libres); i++)
    {
        t_hueco* hueco = list_get(lista_huecos_libres, i);
        log_debug(logger,"Hueco[%d] Base:%u Limite:%u Tamaño:%u",i,hueco->base,hueco->limite,hueco->tamano);
    }
} 