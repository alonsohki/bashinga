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
 * FICHERO:       io.c
 * DESCRIPCIÓN:   Entrada/Salida y control de la linea de comandos.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>
#include "io.h"
#include "prompt.h"
#include "terminal.h"

// Escritura a ficheros con formato
int vwritef ( int fd, const char* format, va_list vl )
{
  char buffer [ 1024 ];
  int size;

  // Formateamos el string a enviar
  size = vsnprintf ( buffer, sizeof(buffer), format, vl );
  if ( size > 0 )
    return write ( fd, buffer, size );
  else
    return size;
}

int writef ( int fd, const char* format, ... )
{
  va_list vl;
  int res;

  va_start ( vl, format );
  res = vwritef ( fd, format, vl );
  va_end ( vl );

  return res;
}


// Lectura de un caracter
int getch(char* c)
{
    int ret;
    struct termio param_save, misparam;

    /* -- lectura de parametros dispositivo --- */
    ioctl(0, TCGETA,  &param_save);

    /* --- programacion del dispositivo --- */
    misparam = param_save;
    /* quitamos eco local y modo canonico */
    misparam.c_lflag &= ~(ICANON | ECHO);
    /* devolver entrada cuando haya 1 caracter en buffer */
    misparam.c_cc[4]=1;
    /* pone nuevos parametros */
    ioctl(0, TCSETA, &misparam );

    /* --- lectura de la entrada --- */
    ret = read(0, c, 1);

    /* --- reponemos parametros dispositivo --- */
    ioctl(0, TCSETA, &param_save );

    return ret;
}

// Eco
void hacer_eco ( char c )
{
  struct termio tm;

  // Comprobamos que el eco este activo
  ioctl(0, TCGETA, &tm);

  if ( tm.c_lflag & ECHO )
    write ( 1, &c, 1 );
}


// Lineas
void linea_inicializar ( Linea* linea )
{
  memset ( linea, 0, sizeof(Linea) );
}

void linea_anyadir ( Linea* linea, char c, void (*mostrarFn)(char) )
{
  if ( linea->escape.len > 0 )
  {
    linea_anyadir_secuencia_escape ( linea, c, mostrarFn );
  }
  else
  {
    if ( linea->cursor < linea->len )
    {
      // Nos encontramos en el caso en el que el cursor no esta al final de la linea y requiere desplazar.
      int i;
      for ( i = linea->len; i > linea->cursor; i-- )
      {
        linea->buffer [ i ] = linea->buffer [ i - 1 ];
      }
    }

    // Anyadimos el caracter leido en la posicion actual del cursor.
    linea->buffer [ linea->cursor ] = c;
    linea->cursor++;
    linea->len++;

    if ( mostrarFn )
    {
      ejecutar_secuencia_escape ( LINEA_LIMPIAR_DERECHA );
      mostrarFn ( c );
      linea_mostrar_desde_cursor ( linea );
    }
  }
}

void linea_anyadir_secuencia_escape ( Linea* linea, char c, void (*mostrarFn)(char) )
{
  linea->escape.bytes [ linea->escape.len ] = c;
  linea->escape.len++;
}

void linea_limpiar_secuencia_escape ( Linea* linea )
{
  linea->escape.len = 0;
}

void linea_mostrar_intervalo ( Linea* linea, int inicio, int n )
{
  write ( 1, &(linea->buffer[inicio]), n );
}

void linea_mostrar_desde_cursor ( Linea* linea )
{
  // Imprime todos los caracteres desde la posicion del cursor hasta el final.
  if ( linea->cursor < linea->len )
  {
    write ( 1, &(linea->buffer [ linea->cursor ]), linea->len - linea->cursor );
    // Restaura el cursor a la posicion anterior.
    ejecutar_secuencia_escape_repetir ( FLECHA_IZQUIERDA, linea->len - linea->cursor );
  }
}

void linea_mostrar_secuencia_escape ( Linea* linea, void (*mostrarFn)(char) )
{
  if ( linea->escape.len > 0 )
  {
    int i;
    for ( i = 0; i < linea->escape.len; ++i )
    {
      mostrarFn ( linea->escape.bytes [ i ] );
    }
  }
}

char* linea_hacer_string ( Linea* linea )
{
  // Ponemos el fin de cadena en el buffer para que pueda procesarse como string.
  linea->buffer[linea->len] = '\0';

  return linea->buffer;
}

void linea_backspace ( Linea* linea )
{
  if ( linea->cursor > 0 )
  {
    if ( linea->cursor < linea->len )
    {
      // Nos encontramos en el caso en el que el cursor no estaba al final de la linea.
      // Requiere desplazar.
      int i;
      for ( i = linea->cursor - 1; i < (linea->len - 1); i++ )
      {
        linea->buffer [ i ] = linea->buffer [ i + 1 ];
      }
    }
    linea->cursor--;
    linea->len--;

    // Mostramos el backspace
    write ( 1, "\177", 1 );
    ejecutar_secuencia_escape ( LINEA_LIMPIAR_DERECHA );
    linea_mostrar_desde_cursor ( linea );
  }
}

void linea_suprimir ( Linea* linea )
{
  if ( linea->cursor < linea->len )
  {
    // Nos encontramos en el caso en el que el cursor no estaba al final de la linea.
    // Requiere desplazar.
    int i;
    for ( i = linea->cursor; i < linea->len; i++ )
    {
      linea->buffer [ i ] = linea->buffer [ i + 1 ];
    }
    linea->len--;

    ejecutar_secuencia_escape ( LINEA_LIMPIAR_DERECHA );
    linea_mostrar_desde_cursor ( linea );
  }
}

void linea_cargar_contenido ( Linea* linea, char* contenido )
{
  linea_inicializar ( linea );
  strcpy ( linea->buffer, contenido );
  linea->len = strlen ( linea->buffer );
  linea->cursor = linea->len;
}

void linea_mostrar_reset ( Linea* linea )
{
  write ( 1, "\r", 1 );
  mostrar_prompt ();
  write ( 1, linea->buffer, linea->len );

  ejecutar_secuencia_escape ( LINEA_LIMPIAR_DERECHA );

  // Restauramos el cursor a su posición original.
  ejecutar_secuencia_escape_repetir ( FLECHA_IZQUIERDA, linea->len - linea->cursor );
}

int linea_procesar_secuencia_escape ( Linea* linea, CodigoSecuencia codigo )
{
  int mostrarSecuencia = 1;

  // Procesamos el codigo de secuencia en caso de exito.
  switch ( codigo )
  {
    // Secuencias conocidas
    case FLECHA_IZQUIERDA:
      if ( linea->cursor > 0 )
      {
        linea->cursor--;
      }
      else
      {
        mostrarSecuencia = 0;
      }
      break;

    case FLECHA_DERECHA:
      if ( linea->cursor < linea->len )
      {
        linea->cursor++;
      }
      else
      {
        mostrarSecuencia = 0;
      }
      break;

    case LINEA_SUPRIMIR:
      linea_suprimir ( linea );
      mostrarSecuencia = 0;
      break;

    case IR_INICIO:
    {
      ejecutar_secuencia_escape_repetir ( FLECHA_IZQUIERDA, linea->cursor );
      linea->cursor = 0;
      mostrarSecuencia = 0;
      break;
    }

    case IR_FINAL:
    {
      ejecutar_secuencia_escape_repetir ( FLECHA_DERECHA, linea->len - linea->cursor );
      linea->cursor = linea->len;
      mostrarSecuencia = 0;
      break;
    }

    default:
      break;
  }

  return mostrarSecuencia;
}

void linea_eliminar_desde_cursor ( Linea* linea )
{
  linea->len = linea->cursor;
}

