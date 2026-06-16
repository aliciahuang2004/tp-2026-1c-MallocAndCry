#include "compactacion.h"
#include "kernel_memory.h"
#include "estructuras.h"
/*
compactar_memoria();
mover_segmento();
recalcular_huecos();
*/
/*
cuando detecto compactacion inmediatamente dejo de recibir peticiones y espero a ks?
ó sigo ejecutando hasta que ks envie "CPUS_DESALOJADAS" y recién allí dejo de recibir peticiones para compactar?
*/

void avisar_compactacion(t_kernel_memory* km, t_log* logger) {
    log_info(logger, "## Solicitando compactación al Kernel Scheduler");
    t_paquete* aviso = crear_paquete(INICIAR_COMPACTACION, crear_buffer());
    enviar_paquete(aviso, km->socket_kernel_scheduler, logger);
    eliminar_paquete(aviso);
}