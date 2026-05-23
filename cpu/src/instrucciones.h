#ifndef INSTRUCCIONES_H_
#define INSTRUCCIONES_H_

#include "cpu.h"
#include <stdint.h>


void ejecutar_NOOP(t_cpu* cpu, t_contexto* ctx);
void ejecutar_SET(t_registros* reg, char* registro_destino, uint32_t valor);
void ejecutar_SUM(t_registros* reg, char* registro_destino, char* registro_origen);
void ejecutar_SUB(t_registros* reg, char* registro_destino, char* registro_origen);
void ejecutar_JNZ(t_registros* reg, char* registro_evaluado, uint32_t nueva_instruccion);

// instrucciones de memoria(usaran la MMU)
int ejecutar_MOV_IN(t_cpu* cpu, t_contexto* ctx, char* registro_datos);
int ejecutar_MOV_OUT(t_cpu* cpu, t_contexto* ctx, char* registro_datos);
int ejecutar_COPY_MEM(t_cpu* cpu, t_contexto* ctx, char* registro_tamano);
int manejar_SYSCALL(t_cpu* cpu, t_contexto* ctx, char* operacion, char** parametros);

//syscalls
void ejecutar_SYSCALL(t_cpu* cpu, t_contexto* ctx, t_instruccion_decodificada instruccion);

#endif /* INSTRUCCIONES_H_ */