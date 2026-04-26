#include "kernel_scheduler.h"

int main(int argc, char* argv[]) {
    
    if (argc != 3)
    {
        printf("Uso: ./bin/kernel_scheduler [Archivo Config] [Path Proceso Inicial]\n");
        return EXIT_FAILURE;
    }
    //Se inicializa la estructura y configuracion
    t_kernel_scheduler* kernel_scheduler = iniciar_kernel_scheduler(argv[1]);
    verificar_kernel_scheduler(kernel_scheduler);
    // Conexión con Kernel Memory 
    conectar_con_kernel_memory(kernel_scheduler);


    iniciarPlanificadorLargoPlazo();
    crearProceso(kernel_scheduler, argv[2],0);

    esperar_conexiones(kernel_scheduler);
    destruir_kernel_scheduler(kernel_scheduler);
    return 0;
}
