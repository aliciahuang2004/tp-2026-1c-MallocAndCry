#ifndef IO_H
#define IO_H

#include "../../utils/include/utils.h"
#include <commons/config.h>

typedef struct {
    t_log* logger;
    t_config* config;
    char* log_level;
    char* tipo_IO;
    char* ip_kernel_scheduler;
    int puerto_kernel_scheduler;
    int socket_kernel_scheduler;
   
}t_io;

t_io* inicializar_io(char* archivo_config, char* tipo_io);
void verificar_io(t_io* io);
void liberar_io(t_io* io);
int conectar_a_kernel_scheduler(t_io* io);
void enviar_handshake(t_io* io);


#endif /* IO_H */