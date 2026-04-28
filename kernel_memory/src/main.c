#include "kernel_memory.h"

int main(int argc, char* argv[]) {

    if (argc != 2 ){
        printf("Uso: ./bin/kernel_memory [Archivo Config]\n");
        return EXIT_FAILURE;
    }

    //INICIA LOGGER TEMPORAL, CONFIG Y LOGGER
    t_kernel_memory* kernel_memory = iniciar_kernelMemory(argv[1]);

    //VERIFICA LOS DATOS CARGADOS
    verificarKernelMemory(kernel_memory);
    
    //INICIA SERVIDOR
    int kernel_memory_fd = iniciar_servidor(kernel_memory->puerto_escucha);
    log_debug(kernel_memory->logger, "Servidor listo para recibir una conexion - FD: %i / puerto: %s" , kernel_memory_fd, kernel_memory->puerto_escucha);

    lista_contextos = list_create();

    esperarConexiones(kernel_memory,kernel_memory_fd);

    void* atender_conexion(void* arg);

    // terminar_programa()
    return 0;
}
