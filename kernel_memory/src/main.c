#include "kernel_memory.h"
#include "conexiones.h"
#include "estructuras.h"
#include <signal.h>
#include <unistd.h>

t_bitarray* bitmap_swap = NULL;
int swap_block_size = 0;
int km_socket_swap = -1;

volatile sig_atomic_t continuar = 1;
int socket_servidor_global = -1;

void capturar_sigint(int s) {
    printf("\n[SIGINT] Ctrl+C detectado. Rompiendo bucles de red...\n");
    continuar = 0;
    if (socket_servidor_global != -1) {
        close(socket_servidor_global);
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, capturar_sigint);

    if (argc != 2 ){
        printf("Uso: ./bin/kernel_memory [Archivo Config]\n");
        return EXIT_FAILURE;
    }

    t_kernel_memory* kernel_memory = iniciar_kernelMemory(argv[1]);

    verificarKernelMemory(kernel_memory);
    
    int kernel_memory_fd = iniciar_servidor(kernel_memory->puerto_escucha);
    log_info(kernel_memory->logger, "--------- Servidor KERNEL MEMORY listo para recibir una conexion - FD: %i / puerto: %s --------" , kernel_memory_fd, kernel_memory->puerto_escucha);

    socket_servidor_global = kernel_memory_fd;

    // Inicialización de estructuras globales
    lista_ms = list_create();
    lista_dir_global_ms = list_create();
    tabla_contextos = dictionary_create();
    lista_huecos_libres = list_create();
    cpus_conectadas = list_create();
    lista_ms_conexion = list_create();
    procesos = dictionary_create();

    esperarConexiones(kernel_memory, kernel_memory_fd);
    
    log_warning(kernel_memory->logger, "Apagando el módulo. Destruyendo estructuras...");
    
    close(kernel_memory_fd);          
    destruir_kernel_memory(kernel_memory); 

    return 0;
}


/*
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

    return 0;
}
*/