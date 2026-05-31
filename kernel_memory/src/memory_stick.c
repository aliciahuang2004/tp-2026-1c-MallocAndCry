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