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
 * FICHERO:       infolinea.c
 * DESCRIPCIÓN:   Procesado de la línea de comandos.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#include <string.h>
#include "infolinea.h"

void infolinea_procesar ( InfoLinea* info, char* linea, InfoLineaCursor* infoCursor, int posicionCursor )
{
  int i;
  char* p;
  char* p2;
  char* programas [ MAX_PROGRAMAS_POR_LINEA ];
  char* lineaFinal = &( linea [ strlen(linea) - 1 ] );
  int obtenerInformacionDeCursor = 0;
  char* posicionCursorEnLinea;

  // Comprobamos si debemos obtener información del cursor.
  if ( ( infoCursor != NULL ) && ( posicionCursor > 0 ) )
  {
    obtenerInformacionDeCursor = 1;
    posicionCursorEnLinea = &(linea[posicionCursor]);
    memset ( infoCursor, 0, sizeof(InfoLineaCursor) );
  }

  // Inicializamos la estructura.
  memset ( info, 0, sizeof(InfoLinea) );

  // Verificamos si se debe ejecutar en modo SPAWN buscando
  // un & al final de la linea.
  p = lineaFinal;

  // Evitamos espacios superfluos.
  while ( ( *p == ' ' ) && ( p >= linea ) )
  {
    *p = '\0';
    --p;
  }
  if ( *p == '&' )
  {
    info->ejecutarEnSpawn = 1;
    *p = '\0';
    --p;
  }
  lineaFinal = p + 1;

  // Buscamos los pipes para aislar cada programa.
  p = linea;
  do
  {
    // Nos saltamos los blancos al comienzo del comando.
    while ( ( p < lineaFinal ) && ( ( *p == ' ' ) || ( *p == '|' ) ) )
      ++p;

    p2 = strchr ( p, '|' );
    if ( p2 != NULL )
    {
      // Almacenamos la referencia a este programa.
      programas [ info->numProgramas ] = p;
      *p2 = '\0';

      // Incrementamos el contador y el apuntador a la linea para seguir buscando.
      info->numProgramas++;
      p = p2 + 1;

      // Nos aseguramos de no pasarnos de número de programas por línea.
      if ( info->numProgramas == MAX_PROGRAMAS_POR_LINEA )
        p2 = NULL;
    }
  } while ( p2 != NULL );

  // Añadimos el último programa de la lista.
  if ( ( strlen(p) > 0 ) && ( info->numProgramas < MAX_PROGRAMAS_POR_LINEA ) )
  {
    programas [ info->numProgramas ] = p;
    info->numProgramas++;
  }

  // Recorremos los programas leidos para extraer los argumentos de cada uno.
  if ( info->numProgramas > 0 )
  {
    for ( i = 0; i < info->numProgramas; ++i )
    {
      // Interpretamos los argumentos de este programa
      char* entreComillas = NULL;
      char* comienzoParam;
      char* finalPrograma;
      p = programas[i];
      comienzoParam = p;

      finalPrograma = &( programas[ i ][ strlen(programas[i]) - 1 ] );
      while ( p < finalPrograma )
      {
        // Primero comprobamos si es un argumento entrecomillado.
        if ( *p == '"' )
        {
          if ( entreComillas == NULL )
          {
            if ( p == comienzoParam )
              entreComillas = p + 1;
          }
          else
          {
            *p = '\0';
            if ( *entreComillas == '>' )
            {
              ++entreComillas;
              info->salidaAgregada = 0;
              if ( *entreComillas == '>' )
              {
                info->salidaAgregada = 1;
                ++entreComillas;
              }
              strcpy ( info->ficheroSalida, entreComillas );
            }
            else
            {
              if ( obtenerInformacionDeCursor )
              {
                if ( ( entreComillas <= posicionCursorEnLinea ) &&
                     ( p >= posicionCursorEnLinea ) )
                {
                  infoCursor->programa = i;
                  infoCursor->argumento = info->programas[i].argc;
                  obtenerInformacionDeCursor = 0;
                }
              }

              info->programas[i].argv[ info->programas[i].argc ] = entreComillas;
              info->programas[i].argc++;
            }
            entreComillas = NULL;
            comienzoParam = p + 1;
          }
        }

        // Procesamos los argumentos tal cual.
        if ( entreComillas == NULL && *p == ' ' )
        {
          // Saltamos los blancos al comienzo de un parámetro.
          if ( p == comienzoParam )
            ++comienzoParam;
          else
          {
            *p = '\0';
            if ( *comienzoParam == '>' )
            {
              ++comienzoParam;
              info->salidaAgregada = 0;
              if ( *comienzoParam == '>' )
              {
                info->salidaAgregada = 1;
                ++comienzoParam;
              }
              strcpy ( info->ficheroSalida, comienzoParam );
            }
            else
            {
              if ( obtenerInformacionDeCursor )
              {
                if ( ( comienzoParam <= posicionCursorEnLinea ) &&
                     ( p >= posicionCursorEnLinea ) )
                {
                  infoCursor->programa = i;
                  infoCursor->argumento = info->programas[i].argc;
                  obtenerInformacionDeCursor = 0;
                }
              }

              info->programas[i].argv[ info->programas[i].argc ] = comienzoParam;
              info->programas[i].argc++;
            }
            comienzoParam = p + 1;
          }
        }

        ++p;
      }

      // Añadimos el último parámetro, que no necesariamente estará seguido de un blanco.
      while ( *comienzoParam == ' ' )
        ++comienzoParam;
      if ( *comienzoParam != '\0' )
      {
        // Eliminamos los blancos superfluos.
        p = &( comienzoParam [ strlen ( comienzoParam ) - 1 ] );
        while ( ( p > comienzoParam ) && ( *p == ' ' ) )
        {
          *p = '\0';
          --p;
        }

        if ( *comienzoParam == '>' )
        {
          ++comienzoParam;
          info->salidaAgregada = 0;
          if ( *comienzoParam == '>' )
          {
            ++comienzoParam;
            info->salidaAgregada = 1;
          }
          strcpy ( info->ficheroSalida, comienzoParam );
        }
        else
        {
          if ( obtenerInformacionDeCursor )
          {
            infoCursor->programa = i;
            infoCursor->argumento = info->programas[i].argc;
          }

          info->programas[i].argv[ info->programas[i].argc ] = comienzoParam;
          info->programas[i].argc++;
        }
      }

      info->programas[i].argv [ info->programas[i].argc ] = NULL;
    }
  }
}

