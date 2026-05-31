#include "huecos.h"
#include "kernel_memory.h"
#include "estructuras.h"
#include <commons/collections/dictionary.h>

void agregar_hueco_libre(uint32_t base, uint32_t tamano)
{
    t_hueco* hueco = malloc(sizeof(t_hueco));

    if(hueco == NULL)
        return;

    hueco->base = base;
    hueco->tamano = tamano;

    pthread_mutex_lock(&mutex_huecos);
    list_add(lista_huecos_libres, hueco);
    pthread_mutex_unlock(&mutex_huecos);

}


void consumir_hueco(t_hueco* hueco, uint32_t tamano) {

    hueco->base += tamano;
    hueco->tamano -= tamano;

    if(hueco->tamano == 0) {
        list_remove_element(lista_huecos_libres, hueco);
        free(hueco);
    }

}

t_hueco* buscar_hueco(uint32_t tamano,t_log* logger) {

    t_kernel_memory* km ;//??????????revisar si esta bien
    if(strcmp(km->allocation_strategy, "BEST") == 0)
        return buscar_hueco_best_fit(tamano,logger);

    return buscar_hueco_worst_fit(tamano,logger);
}

t_hueco* buscar_hueco_best_fit(uint32_t tamano,t_log* logger) {
    t_hueco* mejor = NULL;

    for(int i = 0; i < list_size(lista_huecos_libres); i++) {
        t_hueco* hueco = list_get(lista_huecos_libres, i);

        if(hueco->tamano >= tamano) {
            if(mejor == NULL || hueco->tamano < mejor->tamano) {
                mejor = hueco;
            }
        }
    }
        if(mejor != NULL)
            log_info(logger,
                    "BEST FIT eligio hueco Base:%u Tamaño:%u",mejor->base,mejor->tamano);
        else
            log_info(logger,"BEST FIT no encontro hueco");
    return mejor;
}

t_hueco* buscar_hueco_worst_fit(uint32_t tamano,t_log* logger) {
    t_hueco* peor = NULL;


    for(int i = 0; i < list_size(lista_huecos_libres); i++) {
        t_hueco* hueco = list_get(lista_huecos_libres, i);

        if(hueco->tamano >= tamano) {
            if(peor == NULL || hueco->tamano > peor->tamano) {
                peor = hueco;
            }
        }
    }
        if(peor != NULL)
            log_info(logger,"WORST FIT eligio hueco Base:%u Tamaño:%u",peor->base,peor->tamano);
        else
            log_info(logger,"WORST FIT no encontro hueco");
    return peor;
}

void loguear_huecos(t_log* logger) {//para probar que la lista de huecos se use correctamente
    log_info(logger, "----- LISTA DE HUECOS -----");
    for(int i = 0; i < list_size(lista_huecos_libres); i++) {
        t_hueco* hueco = list_get(lista_huecos_libres, i);
        log_info(logger, "Hueco[%d] Base:%u Tamaño:%u", i, hueco->base, hueco->tamano);
    }
}