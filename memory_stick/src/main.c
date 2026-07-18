#include "memory_stick.h"

int main(int argc, char* argv[]) {
    

    if (argc != 4){                  
        printf("Uso: ./bin/memory_stick [Archivo Config] [Tamaño] [ID]\n ");
        return EXIT_FAILURE;
    }

        char* ms_config = argv[1];                 //ms1.config ms2.config etc
        int tamano = atoi(argv[2]);                //1024 2048 etc
        int id = atoi(argv[3]); 
        t_memory_stick* ms = iniciar_memory_stick(ms_config,tamano,id); 
       
        
        verificar_memory_stick(ms);
    //-----------AHORA ES SERVIDOR PARA ATENDER CPUS--------------------------------
        int servidor_fd = iniciar_servidor(ms->puerto_escucha);
        if (servidor_fd == -1) {
            log_error(ms->logger, "No se pudo iniciar el servidor Memory Stick en puerto %s", ms->puerto_escucha);
            destruir_memory_stick(ms);
            return EXIT_FAILURE;
        }
        log_info(ms->logger,"Servidor Memory Stick ID:%d listo, esperando peticiones de CPUs..", ms->id);
        //conectarse como cliente a kernel_memory
        if(conectar_al_kernelmem(ms) == -1){
        destruir_memory_stick(ms);
        return EXIT_FAILURE;
        }
        //***MS ENVIA HANDSHAKE,ID,TAMAÑO A KM
        enviar_handshake(ms);

        pthread_t hilo_km;
        pthread_create(&hilo_km, NULL, escuchar_kernel_memory, ms);
        pthread_detach(hilo_km);   

        
       
        rutina_recepcion(ms, servidor_fd); //CREA HILOS PARA ATENDER A CADA CPU

        destruir_memory_stick(ms);
   
    return 0;
}
