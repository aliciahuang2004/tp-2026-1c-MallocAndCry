#ifndef INSTRUCCIONES_H_
#define INSTRUCCIONES_H_

#include "cpu.h"
#include <stdint.h>

//funciones auxiliares
bool es_registro_8bits(char *nombre);
void* obtener_registro(t_registros* reg, char* nombre_registro);
uint32_t leer_valor_registro(t_registros* registros, char* nombre_registro);

void ejecutar_NOOP(t_cpu* cpu, t_contexto* ctx);
void ejecutar_SET(t_cpu* cpu,t_registros* reg, char* registro_destino, uint32_t valor);
void ejecutar_SUM(t_cpu* cpu,t_registros* reg, char* registro_destino, char* registro_origen);
void ejecutar_SUB(t_cpu* cpu,t_registros* reg, char* registro_destino, char* registro_origen);
void ejecutar_JNZ(t_cpu* cpu,t_registros* reg, char* registro_evaluado, uint32_t nueva_instruccion);

// instrucciones de memoria(usaran la MMU)
int ejecutar_MOV_IN(t_cpu* cpu, t_contexto* ctx, char* registro_datos);
int ejecutar_MOV_OUT(t_cpu* cpu, t_contexto* ctx, char* registro_datos);
int ejecutar_COPY_MEM(t_cpu* cpu, t_contexto* ctx, char* registro_tamano);

//syscalls
int ejecutar_SYSCALL(t_cpu* cpu, t_contexto* ctx, t_instruccion_decodificada instruccion);
//mmu
bool mmu_traducir_direccion(t_cpu* cpu, t_contexto* ctx, uint32_t dir_logica, uint32_t tamano_a_operar, uint32_t* dir_fisica_out, int* ms_id_out);

t_list* fragmentar_acceso_memoria(t_cpu* cpu, uint32_t dir_fisica_global, int tamanio_total);
    

#endif /* INSTRUCCIONES_H_ */