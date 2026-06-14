#include "procesos.h"
#include "kernel_memory.h"
#include "estructuras.h"
#include "contextos.h"
#include "huecos.h"

t_dictionary* procesos;
pthread_mutex_t mutex_procesos = PTHREAD_MUTEX_INITIALIZER;

int crear_proceso(int pid)
{
    t_proceso* proceso = malloc(sizeof(t_proceso));

    if(proceso == NULL)
    {
        return -1;
    }

    proceso->pid = pid;

    proceso->suspendido = false;

    proceso->contexto = crear_contexto();

    if(proceso->contexto == NULL)
    {
        free(proceso);
        return -1;
    }

    char pid_str[20];

    sprintf(pid_str, "%d", pid);

    pthread_mutex_lock(&mutex_procesos);

    dictionary_put(procesos,pid_str,proceso);

    pthread_mutex_unlock(&mutex_procesos);

    return 0;
}

void iniciar_tabla_procesos(void)
{
    procesos = dictionary_create();
}


t_proceso* buscar_proceso(int pid) {
    char pid_str[20];
    sprintf(pid_str, "%d", pid);

    return dictionary_get(procesos, pid_str);
}


//FALTA PROBAR 
// guarda la info de los segmentos,borra los registros,destruye la tabla de seg del proc,
//borra el contexto de la lista,elimina el proceso del dictionary,elimina su path de instrucciones y agrega huecos libres
int eliminar_proceso(int pid, t_kernel_memory* km, t_log* logger) {

    pthread_mutex_lock(&mutex_procesos);
    t_proceso* proceso = buscar_proceso(pid);
    if (proceso == NULL) {
        pthread_mutex_unlock(&mutex_procesos);
        log_error(logger, "Proceso PID:%d no encontrado", pid);
        return -1;
    }

    int cant_segmentos = list_size(proceso->contexto->tabla_segmentos);
    uint32_t bases[cant_segmentos];
    uint32_t limites[cant_segmentos];

    for (int i = 0; i < cant_segmentos; i++) {
        t_segmento* seg = list_get(proceso->contexto->tabla_segmentos, i);
        bases[i] = seg->base_global;
        limites[i] = seg->limite_global;
    }

    free(proceso->contexto);

    list_destroy_and_destroy_elements(proceso->contexto->tabla_segmentos, free);

    free(proceso->contexto);

    char pid_str[20];
    sprintf(pid_str, "%d", pid);
    dictionary_remove_and_destroy(procesos, pid_str, free);

    pthread_mutex_unlock(&mutex_procesos);

    char pid_str_path[20];
    sprintf(pid_str_path, "%d", pid);
    dictionary_remove_and_destroy(km->paths_por_pid, pid_str_path, free);
    log_info(logger, "Path eliminado para PID:%d", pid);


    pthread_mutex_lock(&mutex_huecos);
    for (int i = 0; i < cant_segmentos; i++) {
        agregar_hueco_libre(bases[i], limites[i]);
    }
    pthread_mutex_unlock(&mutex_huecos);

    log_info(logger, "Proceso eliminado PID:%d - %d segmentos liberados", pid, cant_segmentos);
    return 1;
}