#include "procesos.h"
#include "kernel_memory.h"
#include "estructuras.h"
#include "contextos.h"

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

void iniciar_tabla_procesos()
{
    procesos = dictionary_create();
}