#include "contextos.h"
#include "kernel_memory.h"
#include "estructuras.h"
#include <commons/collections/dictionary.h>

t_dictionary* tabla_contextos;
pthread_mutex_t mutex_tabla_contextos = PTHREAD_MUTEX_INITIALIZER;

/*ver mas adelante si realmente no me sirve
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
*/

t_contexto* crear_contexto() {
    t_contexto* contexto = malloc(sizeof(t_contexto));
    memset(&contexto->registros, 0, sizeof(t_registros)); // inicializa todo en 0
    contexto->tabla_segmentos = list_create();
    return contexto;
}

//----descomentar cuando cpu espere tabla de segmentos en case context response

t_registros* solicitud_contexto(int pid, t_list** tabla_out)
{
    char pid_str[20];
    sprintf(pid_str, "%d", pid);

    pthread_mutex_lock(&mutex_procesos);
    t_proceso* proceso = dictionary_get(procesos, pid_str);
    if (proceso == NULL) {
        pthread_mutex_unlock(&mutex_procesos);
        *tabla_out = NULL;
        return NULL;
    }

    t_registros* copia = malloc(sizeof(t_registros));
    memcpy(copia,&(proceso->contexto->registros),sizeof(t_registros));

    // Copiar la tabla de segmentos
    t_list* tabla_copia = list_create();
    for (int i = 0; i < list_size(proceso->contexto->tabla_segmentos); i++) {
        t_segmento* seg_original = list_get(proceso->contexto->tabla_segmentos, i);
        t_segmento* seg_copia    = malloc(sizeof(t_segmento));
        memcpy(seg_copia, seg_original, sizeof(t_segmento));
        list_add(tabla_copia, seg_copia);
    }
    *tabla_out = tabla_copia;

    pthread_mutex_unlock(&mutex_procesos);
    return copia;
}

void actualizar_contexto(int pid, t_registros* registros_nuevos, t_log* logger) {
    char pid_str[20];
    sprintf(pid_str, "%d", pid);

    pthread_mutex_lock(&mutex_procesos);
        t_proceso* proceso = dictionary_get(procesos, pid_str);
    if (proceso != NULL) {
        memcpy(&proceso->contexto->registros, registros_nuevos, sizeof(t_registros));
                log_debug(logger, "## Contexto actualizado recibido - PID: %d", pid);
        log_debug(logger, "   PC=%u  AX=%u   BX=%u   CX=%u   DX=%u", 
            registros_nuevos->PC, registros_nuevos->AX, registros_nuevos->BX, 
            registros_nuevos->CX, registros_nuevos->DX);
        log_debug(logger, "   EAX=%u     EBX=%u    ECX=%u   EDX=%u   SI=%u   DI=%u",
            registros_nuevos->EAX, registros_nuevos->EBX, registros_nuevos->ECX,
            registros_nuevos->EDX, registros_nuevos->SI, registros_nuevos->DI);
        log_debug(logger, "Contexto actualizado - PID: %d", pid);
    } else {
        // Si no existe, es un paquete tardío de una CPU 
        log_warning(logger, "Intento de actualizar PID %d, pero no existe. Paquete descartado.", pid);
    }
    pthread_mutex_unlock(&mutex_procesos);
}