/*
 * Copyright (c) 2009-2010 Alberto Alonso Pinto
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * FICHERO:       io.h
 * DESCRIPCIÓN:   Entrada/Salida y control de la linea de comandos.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#pragma once

#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "config.h"
#include "codigos_secuencia.h"

// Errores
#define error(x) do { perror(x); exit(-1); } while(0)

// Escritura con formato a un fichero
int vwritef ( int fd, const char* format, va_list vl );
int writef ( int fd, const char* format, ... );

// Lectura de un caracter (http://www.canalc.hispla.com/foro/viewtopic.php?f=31&t=45)
int getch ( char* c );

// Eco
void hacer_eco ( char c );

// Declaramos los tipos de datos para las lineas.
typedef struct _Linea
{
  char buffer [ MAX_LINEA ];  // Contenido de la linea.
  int len;                    // Tamanyo actual del buffer.
  int cursor;                 // Posicion del cursor.
  struct
  {
    char bytes [ 32 ];        // Secuencia de escape en proceso.
    int len;                  // Tamanyo en bytes de la secuencia de escape.
  } escape;
} Linea;

void linea_inicializar ( Linea* linea );
void linea_anyadir ( Linea* linea, char c, void (*fnMostrar)(char) );
void linea_anyadir_secuencia_escape ( Linea* linea, char c, void (*fnMostrar)(char) );
void linea_mostrar_secuencia_escape ( Linea* linea, void (*fnMostrar)(char) );
void linea_limpiar_secuencia_escape ( Linea* linea );
int linea_procesar_secuencia_escape ( Linea* linea, CodigoSecuencia codigo );
void linea_mostrar_intervalo ( Linea* linea, int inicio, int n );
void linea_mostrar_desde_cursor ( Linea* linea );
void linea_cargar_contenido ( Linea* linea, char* contenido );
void linea_mostrar_reset ( Linea* linea );
void linea_backspace ( Linea* linea );
void linea_suprimir ( Linea* linea );
void linea_eliminar_desde_cursor ( Linea* linea );
char* linea_hacer_string ( Linea* linea );
