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
 * FICHERO:       terminal.c
 * DESCRIPCIÓN:   Control de secuencias de escape de terminales concretos.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#include <string.h>
#include <unistd.h>
#include "io.h"
#include "terminal.h"

// Terminales
#include "vt100.h"

static CodigoSecuencia buscar_codigo_para_secuencia ( const TerminalSecuenciasEscape* terminal, Linea* linea, int* tamanyoSecuencia )
{
  CodigoSecuencia secuenciaEncontrada = SECUENCIA_INEXISTENTE;
  int secuenciaEncontradaIndice;
  const char* secuencia = linea->escape.bytes;
  int len = linea->escape.len;

  // Verificamos que tengamos algo que procesar.
  if ( len > 0 )
  {
    // Almacenamos la maxima longitud que debemos procesar.
    int maximaLongitud = len;
    if ( maximaLongitud > terminal->maxBytes )
    {
      maximaLongitud = terminal->maxBytes;
    }

    // Iteramos por todas las secuencias existentes y, para cada una de ellas, comprobamos
    // si coincide con los datos de entrada.
    int i;
    int curSeq;
    for ( curSeq = 0; ( curSeq < terminal->numSecuencias ) &&
                      ( secuenciaEncontrada == SECUENCIA_INEXISTENTE );
          curSeq++ )
    {
      const char* termSeq = terminal->secuencias [ curSeq ].secuencia;
      int coincidencia = 1;
      for ( i = 0; ( i < maximaLongitud ) &&
                     coincidencia &&
                   ( termSeq [ i ] != 0 );
            i++ )
      {
        if ( termSeq [ i ] != secuencia [ i ] )
        {
          coincidencia = 0;
        }
      }

      if ( coincidencia )
      {
        // Verificamos que la coincidencia es hasta el final.
        if ( termSeq [ i ] != 0 )
        {
          secuenciaEncontrada = SECUENCIA_NO_FINALIZADA;
        }
        else
        {
          secuenciaEncontrada = terminal->secuencias [ curSeq ].codigoSecuencia;
          secuenciaEncontradaIndice = curSeq;
        }

        if ( tamanyoSecuencia )
        {
          *tamanyoSecuencia = i - 1;
        }
      }
    }
  }

  return secuenciaEncontrada;
}

static const TerminalSecuenciasEscape* obtenerSecuenciasEscape ()
{
  // Solo vamos a soportar vt100 por el momento.
  return obtenerSecuenciasEscapeVT100 ();
}

CodigoSecuencia procesar_secuencia_escape ( Linea* linea, int* tamanyoSecuencia )
{
  const TerminalSecuenciasEscape* terminal = obtenerSecuenciasEscape ();

  // Si no estamos en una secuencia de escape, retornamos 0.
  if ( linea->escape.len == 0 )
    return SECUENCIA_NO_INICIADA;

  // Buscamos una secuencia de escape coincidente.
  CodigoSecuencia codigo = buscar_codigo_para_secuencia ( terminal, linea, tamanyoSecuencia );

  return codigo;
}

void ejecutar_secuencia_escape ( CodigoSecuencia codigo )
{
  const TerminalSecuenciasEscape* terminal = obtenerSecuenciasEscape ();
  int curSeq;
  int encontrada = 0;

  for ( curSeq = 0; ( encontrada == 0 ) &&
                    ( curSeq < terminal->numSecuencias );
        curSeq++ )
  {
    if ( terminal->secuencias [ curSeq ].codigoSecuencia == codigo )
    {
      const char* secuencia = terminal->secuencias [ curSeq ].secuencia;
      encontrada = 1;
      writef ( 1, secuencia );
    }
  }
}

void ejecutar_secuencia_escape_repetir ( CodigoSecuencia codigo, int nVeces )
{
  const TerminalSecuenciasEscape* terminal = obtenerSecuenciasEscape ();
  int curSeq;
  int encontrada = 0;

  for ( curSeq = 0; ( encontrada == 0 ) &&
                    ( curSeq < terminal->numSecuencias );
        curSeq++ )
  {
    if ( terminal->secuencias [ curSeq ].codigoSecuencia == codigo )
    {
      const char* secuencia = terminal->secuencias [ curSeq ].secuencia;
      char* repeticion = (char *)malloc( terminal->maxBytes * nVeces );
      int i;
      int len = strlen(secuencia);

      for ( i = 0; i < nVeces; ++i )
      {
        memcpy ( &(repeticion[i*len]), secuencia, len );
      }

      encontrada = 1;
      write ( 1, repeticion, nVeces*len );
      free ( repeticion );
    }
  }
}

