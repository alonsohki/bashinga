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
 * FICHERO:       historial.c
 * DESCRIPCIÓN:   Control del historial.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "historial.h"
#include "io.h"

struct Historial_
{
  char** lineas;
  int maxTamanyo;
  int cursor;
  int lleno;
  const char* fichero;
};

Historial* historial_obtener_instancia ()
{
  static Historial* hist = NULL;
  if ( hist == NULL )
    hist = historial_crear ( 0 );
  return hist;
}

Historial* historial_crear ( int p_maxHistorial )
{
  Historial* hist = (Historial *)malloc(sizeof(Historial));
  memset ( hist, 0, sizeof(Historial) );

  const char* maxHistorial = getenv ( "HISTORY_LENGTH" );
  int iMaxHistorial;
  int i;
  char* datosLineas;

  // Intentamos coger el tamanyo del historial desde el parametro o una variable de entorno.
  // En caso de no existir esta, cogemos el valor por defecto.
  if ( p_maxHistorial > 0 )
    iMaxHistorial = p_maxHistorial;
  else if ( maxHistorial )
    iMaxHistorial = atoi ( maxHistorial );
  else
    iMaxHistorial = MAX_HISTORIAL;

  hist->maxTamanyo = iMaxHistorial;

  // Reservamos memoria para las lineas.
  hist->lineas = (char **)malloc( sizeof(char*) * iMaxHistorial );
  datosLineas = (char *)malloc( sizeof(char) * MAX_LINEA * iMaxHistorial );

  // Inicializamos los apuntadores.
  for ( i = 0; i < iMaxHistorial; i++ )
  {
    hist->lineas [ i ] = &(datosLineas[i * MAX_LINEA]);
  }

  return hist;
}

void historial_eliminar ( Historial* hist )
{
  free ( hist->lineas[0] );
  free ( hist->lineas );
  free ( hist );
}

void historial_anyadir ( Historial* hist, char* linea )
{
  strcpy ( hist->lineas[hist->cursor], linea );
  hist->cursor = ( hist->cursor + 1 ) % hist->maxTamanyo;
  if ( hist->cursor == 0 )
    hist->lleno = 1;

  // Verificamos si tenemos que guardar a un fichero la nueva linea.
  if ( hist->fichero )
  {
    int fd = open ( hist->fichero, O_WRONLY | O_CREAT | O_APPEND, S_IREAD | S_IWRITE );
    if ( fd != -1 )
    {
      writef ( fd, "%s\n", linea );
      close ( fd );
    }
  }
}

int historial_tamanyo ( Historial* hist )
{
  if ( hist->lleno )
    return hist->maxTamanyo;
  else
    return hist->cursor;
}

void historial_recorrer ( Historial* hist, int (*recorrerFn)(char*, void*), void* userData )
{
  if ( recorrerFn == NULL )
    return;

  if ( hist->lleno )
  {
    int i = hist->cursor;
    int continuar = 1;

    do
    {
      continuar = !recorrerFn ( hist->lineas[i], userData );
      i = ( i + 1 ) % hist->maxTamanyo;
    } while ( continuar && ( i != hist->cursor ) );
  }

  else if ( hist->cursor > 0 )
  {
    int i;
    int continuar = 1;
    for ( i = 0; continuar && ( i < hist->cursor ); ++i )
    {
      continuar = !recorrerFn ( hist->lineas[i], userData );
    }
  }
}

void historial_recorrer_inverso ( Historial* hist, int (*recorrerFn)(char*, void*), void* userData )
{
  if ( recorrerFn == NULL )
    return;

    if ( hist->lleno )
    {
      int i = ( ( hist->cursor - 1 ) + hist->maxTamanyo ) % hist->maxTamanyo;
      int continuar = 1;
      do
      {
        continuar = !recorrerFn ( hist->lineas[i], userData );
        i = ( ( i - 1 ) + hist->maxTamanyo ) % hist->maxTamanyo;
      } while ( continuar && ( i != hist->cursor ) );
    }
    else if ( hist->cursor > 0 )
    {
      int i;
      int continuar = 1;
      for ( i = ( hist->cursor - 1 ); i >= 0; --i )
      {
        continuar = !recorrerFn ( hist->lineas[i], userData );
      }
    }
}


struct MostrarData
{
  int fd;
  int n;
};
static int historial_mostrar_visitor ( char* linea, void* userData )
{
  struct MostrarData* data = (struct MostrarData *)userData;
  writef ( data->fd, " %d\t%s\n", data->n, linea );
  data->n++;
  return 0;
}

void historial_mostrar ( Historial* hist, int fd )
{
  struct MostrarData data;
  data.fd = fd;
  data.n = 1;
  historial_recorrer ( hist, historial_mostrar_visitor, &data );
}


struct EmpiezaPorData
{
  char* por;
  char* linea;
};
static int historial_empieza_por_visitor ( char* linea, void* userData )
{
  struct EmpiezaPorData* data = (struct EmpiezaPorData *)userData;

  if ( strstr ( linea, data->por ) == linea )
  {
    data->linea = linea;
    return 1;
  }

  return 0;
}

char* historial_empieza_por ( Historial* hist, char* por )
{
  struct EmpiezaPorData data;
  data.por = por;
  data.linea = NULL;

  historial_recorrer_inverso ( hist, historial_empieza_por_visitor, &data );
  return data.linea;
}


char* historial_obtener ( Historial* hist, int pos )
{
  // Ajustamos la posición requerida al rango.
  if ( !hist->lleno && ( hist->cursor == 0 ) )
  {
    // Está vacío.
    return NULL;
  }

  if ( pos >= hist->maxTamanyo || ( !hist->lleno && pos >= hist->cursor ) )
  {
    // Es mayor que el tamaño del historial.
    return NULL;
  }

  // No aceptamos posiciones negativas.
  if ( pos < 0 )
  {
    return NULL;
  }

  // Calculamos la posición real dentro del buffer circular.
  pos = (( hist->cursor - pos - 1 ) + hist->maxTamanyo) % hist->maxTamanyo;

  // El -1 podría generar problemas si se elimina la primera comprobación de este algoritmo.
  return hist->lineas [ pos ];
}

void historial_cargar_desde_fichero ( Historial* hist, const char* fichero )
{
  int fd = open ( fichero, O_RDONLY );
  char linea [ MAX_LINEA ];
  char c;
  int n;
  int nBytes = 0;

  // Inicializamos el historial.
  hist->cursor = 0;
  hist->lleno = 0;

  if ( fd != -1 )
  {
    int continuar = 1;

    while ( continuar )
    {
      do
      {
        n = read ( fd, &c, 1 );
        if ( n > 0 )
        {
          if ( c == '\n' )
          {
            if ( nBytes > 0 )
            {
              linea [ nBytes ] = '\0';
              nBytes = 0;
              historial_anyadir ( hist, linea );
            }
          }
          else
          {
            linea [ nBytes ] = c;
            ++nBytes;
          }
        }
        else
          continuar = 0;
      } while ( ( n > 0 ) && ( c != '\n' ) );
    }

    close ( fd );
  }
}

void historial_guardar_a_fichero ( Historial* hist, const char* fichero )
{
  hist->fichero = fichero;
}

