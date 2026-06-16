#include "contextos.h"
#include "kernel_memory.h"
#include "estructuras.h"
#include <commons/collections/dictionary.h>

t_dictionary* tabla_contextos;
pthread_mutex_t mutex_tabla_contextos = PTHREAD_MUTEX_INITIALIZER;

t_registros* crear_registros()
{
  t_registros* registros = malloc(sizeof(t_registros));

    registros->PC = 0;

    registros->AX = 0;
    registros->BX = 0;
    registros->CX = 0;
    registros->DX = 0;

    registros->EAX = 0;
    registros->EBX = 0;
    registros->ECX = 0;
    registros->EDX = 0;

    registros->SI = 0;
    registros->DI = 0;

    return registros;
}

t_contexto* crear_contexto ()
{
    t_contexto* contexto =
        malloc(sizeof(t_contexto));

    contexto->registros = crear_registros();

    contexto->tabla_segmentos = list_create();

    return contexto;
}


t_registros* solicitud_contexto(int pid)
{
    char pid_str[20];
    sprintf(pid_str, "%d", pid);

    pthread_mutex_lock(&mutex_procesos);

    t_proceso* proceso = dictionary_get(procesos, pid_str);
    if(proceso == NULL)
    { 
        pthread_mutex_unlock(&mutex_procesos);
        return NULL;
    }

    t_registros* copia = malloc(sizeof(t_registros));
    memcpy(copia, proceso->contexto->registros, sizeof(t_registros));

    pthread_mutex_unlock(&mutex_procesos);

      return copia; 
}

void actualizar_contexto(int pid, t_registros* registros_nuevos) {
    char pid_str[20];
    sprintf(pid_str, "%d", pid);

    pthread_mutex_lock(&mutex_procesos);
    t_proceso* proceso = dictionary_get(procesos, pid_str);
    if (proceso != NULL) {
        memcpy(proceso->contexto->registros, registros_nuevos, sizeof(t_registros));
    }
    pthread_mutex_unlock(&mutex_procesos);
}