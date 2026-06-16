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

void agregar_posicion_ms(t_resultado_hueco r,int ms_id,t_log*logger){
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
        t_ms_pos* ms = list_get(lista_dir_global_ms, i);
        log_info(logger, "  [%d] MS ID:%d | base global:%u | limite global:%u",
                 i, ms->id, ms->base_global, ms->limite_global);
    }
    log_info(logger, "==========================================");
    //****************log temp**************
    pthread_mutex_unlock(&mutex_lista_dir_global_ms);
}

