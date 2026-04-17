#ifndef SWAP_H
#define SWAP_H

#include "../../utils/include/utils.h"
#include <commons/config.h>

typedef struct {
    t_log* logger;
    t_config* config;
    char* log_level;
    char* swap_file_path;
    char* ip_kernel_memory;
    int puerto_kernel_memory;
    int socket_kernel_memory;
}t_swap;

t_swap* inicializar_swap(int argc, char* argv[] );

void verificar_swap(t_swap* sp);

void liberar_swap(t_swap* sp);

int conectar_a_kernel_memory(t_swap* sp);

void enviar_handshake(t_swap* sp);

#endif /* SWAP_H*/
