#ifndef MEMORY_STICK_H
#define MEMORY_STICK_H
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdint.h>
#include <stdbool.h>

void manejar_desconexion_memory_stick(t_ms_info* ms, t_kernel_memory* km, t_log* logger);
int nuevo_memory_stick(int ms_id, int ms_tamano, int socket_cliente);
t_ms_info* buscar_ms_por_socket(int socket);
void agregar_posicion_ms(t_resultado_hueco r,int ms_id,t_log* logger);
t_list* calcular_dir_local_ms(uint32_t direccion_fisica_global,uint32_t tamano_contenido,t_log* logger);
void enviar_fragmentos_escritura(t_list* lista_fragmentos, char* contenido_a_escribir, t_log* logger);
int buscar_socket_ms_por_id(int ms_id);
void* enviar_fragmentos_lectura(t_list *lista_fragmentos, uint32_t tamano, t_log *logger);

#endif 