#include "kernel_scheduler.h"

int main(int argc, char* argv[]) {
    
    if (argc != 3)
    {
        printf("Uso: ./bin/kernel_scheduler [Archivo Config] [Path Proceso Inicial]\n");
        return EXIT_FAILURE;
    }
     t_kernel_scheduler* kernel_scheduler = iniciar_kernel_scheduler(); 
    verificar_kernel_scheduler(kernel_scheduler);

    int storage_fd = iniciar_servidor(kernel_scheduler->puerto_escucha);  //socket, bind, listen    inicia el servidor 
    log_debug(kernel_scheduler->logger, "Servidor listo");

    rutina_recepcion(kernel_scheduler, storage_fd); // acepta conexiones y crea hilos para atender cada cliente
    
    destruir_kernel_scheduler(kernel_scheduler);

    return 0;
}
