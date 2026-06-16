#include "kernel_memory.h"
#include "conexiones.h"

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
    log_info(kernel_memory->logger, "--------- Servidor KERNEL MEMORY listo para recibir una conexion - FD: %i / puerto: %s --------" , kernel_memory_fd, kernel_memory->puerto_escucha);

    lista_ms = list_create();
    lista_dir_global_ms = list_create();
    tabla_contextos = dictionary_create();
    lista_huecos_libres = list_create();
    cpus_conectadas = list_create();
    esperarConexiones(kernel_memory,kernel_memory_fd);

    void* atender_conexion(void* arg);

    // terminar_programa()
    return 0;
}
