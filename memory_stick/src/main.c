#include "memory_stick.h"

int main(int argc, char* argv[]) {
    

    if (argc != 3){                     //3 porque 2 son los argumentos:archivo config y tamaño y el 3ero el nombre del modulo
        printf("Uso: ./bin/memory_stick [Archivo Config] [Tamaño]\n ");
        return EXIT_FAILURE;
    }
        t_memory_stick* ms = iniciar_memory_stick(argc, argv); 
       
        
        verificar_memory_stick(ms);

        //conectarse como cliente a kernel_memory
                if(conectar_al_kernelmem(ms) == -1){
            destruir_memory_stick(ms);
            return EXIT_FAILURE;
        }

        enviar_handshake(ms);        //paquete(socket_kernel_mem);

       // destruir_memory_stick(ms); NO PORQUE DEBE CONTINUAR AHORA COMO SERVIDOR DE CPUS

        //-----------AHORA ES SERVIDOR PARA ATENDER CPUS--------------------------------
       
        int servidor_fd = iniciar_servidor(ms->puerto_escucha);
        
        log_info(ms->logger,"Servidor Memory Stick listo,esperando peticiones de cpu");

        rutina_recepcion(ms,servidor_fd); //CREA HILOS PARA ATENDER A CADA CPU


        destruir_memory_stick(ms);
   
    return 0;
}
