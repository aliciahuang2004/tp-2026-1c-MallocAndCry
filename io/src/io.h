#ifndef IO_H
#define IO_H

#include "../../utils/include/utils.h"
#include "../../utils/include/protocolo.h"
#include <unistd.h>
#include <commons/config.h>
#include <pthread.h>

typedef struct {
    t_log* logger;
    t_config* config;
    char* log_level;
    char* tipo_IO;
    char* ip_kernel_scheduler;
    int puerto_kernel_scheduler;
    int socket_kernel_scheduler;
   
}t_io;

// Estructura para solicitud de IO (recibida desde KS)
typedef struct {
    int pid;
    t_io_operation tipo_operacion;
    uint32_t datos_size;
    void* datos;
} t_io_request;

t_io* inicializar_io(char* archivo_config, char* tipo_io);
void verificar_io(t_io* io);
void liberar_io(t_io* io);
int conectar_a_kernel_scheduler(t_io* io);
void enviar_handshake(t_io* io);

// función auxiliar para convertir string a enum
t_io_operation obtener_tipo_operacion(const char* tipo_io_str);

// funciones de IO
void recibir_y_ejecutar_tarea(t_io* io, t_io_operation tipo_modulo);
void ejecutar_stdin(int pid, uint32_t datos_size, int socket_ks, t_log* logger);
void ejecutar_stdout(int pid, uint32_t datos_size, void* datos, int socket_ks, t_log* logger);
void ejecutar_sleep(int pid, uint32_t tiempo_ms, int socket_ks, t_log* logger); 
void enviar_confirmacion_ks(int socket_ks, int pid, void* datos_respuesta, uint32_t datos_size, t_log* logger);

#endif /* IO_H */