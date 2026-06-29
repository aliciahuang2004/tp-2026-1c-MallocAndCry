#include "kernel_scheduler.h"

// char* pathInicial;
t_kernel_scheduler* kernel = NULL;

int main(int argc, char* argv[]) {
    
    if (argc != 3){
        printf("Uso: ./bin/kernel_scheduler [Archivo Config] [Path Proceso Inicial]\n");
        return EXIT_FAILURE;
    }

    //Se inicializa la estructura y configuracion
    t_kernel_scheduler* kernel_scheduler = iniciar_kernel_scheduler(argv[1]);
    //*** verificar_kernel_scheduler(kernel_scheduler);

    // Conexión con Kernel Memory 
    kernel = kernel_scheduler; // Asignar el kernel_scheduler a la variable global para su uso en otros módulos
    conectar_con_kernel_memory();

    inicializarColas();
    inicializarSemaforos();
    inicializar_interfaces();
    inicializarHilos();
    
    // Crear el proceso inicial (PID 0) antes de esperar CPUs

    crearProceso(argv[2], 0);

    esperar_conexiones(); // DE CPU'S E IO'S
    // destruir_kernel_scheduler(kernel_scheduler);
    // liberarks(kernel_scheduler);
    //LIBERAR HILOS
    return 0;
}
