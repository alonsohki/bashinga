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
 * FICHERO:       variables.c
 * DESCRIPCIÓN:   Variables.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "infolinea.h"
#include "variables.h"

// Estructura para almacenar una variables.
typedef struct
{
  char clave [ 256 ];
  char valor [ 256 ];
} Variable;

// Nodos de la tabla hash.
typedef struct _NodoHash
{
  Variable variable;
  struct _NodoHash* siguiente;
} NodoHash;

// Tabla hash de variables.
struct Variables_
{
  NodoHash* nodos [ VARIABLES_TABLA_HASH_TAMANYO ];
};

static inline unsigned int variables_hash ( const char* str )
{
  // Intentaremos generar un hash con una buena dispersión
  // y evitando el solapamiento al máximo. Un buen desplazamiento
  // para esto puede ser 7, requiriendo 32 vueltas antes de
  // que los bits vuelvan a la posición original al ser primo.
  // Además, como en cada vuelta se añaden 8 nuevos bits, el
  // solapamiento es de un solo bit.
  unsigned int result = 0;
  const char* p = str;

  while ( *p != '\0' )
  {
    unsigned int bitsADesplazar = ( result >> 25 ) & 0x0000007F;
    result = ( result << 7 ) | bitsADesplazar;
    result = result ^ ((unsigned int )*p & 0x000000FF) ^ ( result >> 8 ) ^ ( result >> 16 ) ^ ( result >> 24 );
    ++p;
  }

  return result;
}

Variables* variables_crear ()
{
  Variables* variables = (Variables *)malloc(sizeof(Variables));
  memset ( variables, 0, sizeof(Variables) );
  return variables;
}

void variables_eliminar ( Variables* variables )
{
  int i;
  for ( i = 0; i < VARIABLES_TABLA_HASH_TAMANYO; ++i )
  {
    if ( variables->nodos[i] != NULL )
    {
      NodoHash* actual;
      NodoHash* siguiente;
      for ( actual = variables->nodos[i]; actual != NULL; actual = siguiente )
      {
        siguiente = actual->siguiente;
        free ( actual );
      }
    }
  }

  free ( variables );
}

static NodoHash* variables_buscar_nodo ( Variables* variables, const char* clave )
{
  NodoHash* nodo = NULL;
  NodoHash* actual;
  unsigned int hash = variables_hash ( clave );
  unsigned int pos = ( hash % VARIABLES_TABLA_HASH_TAMANYO );

  actual = variables->nodos [ pos ];
  for (; ( nodo == NULL ) && ( actual != NULL ); actual = actual->siguiente )
  {
    if ( strcmp ( actual->variable.clave, clave ) == 0 )
    {
      nodo = actual;
    }
  }

  return nodo;
}

void variables_establecer ( Variables* variables, const char* clave, const char* valor )
{
  // Buscamos el nodo, si existe, de esta variable.
  NodoHash* nodo = variables_buscar_nodo ( variables, clave );

  // Si no se ha encontrado, creamos uno nuevo.
  if ( nodo == NULL )
  {
    unsigned int hash = variables_hash ( clave );
    unsigned int pos = ( hash % VARIABLES_TABLA_HASH_TAMANYO );

    nodo = (NodoHash *)malloc(sizeof(NodoHash));
    memset ( nodo, 0, sizeof(NodoHash) );
    strcpy ( nodo->variable.clave, clave );
    nodo->siguiente = variables->nodos [ pos ];
    variables->nodos [ pos ] = nodo;
  }

  // Cambiamos el valor.
  strcpy ( nodo->variable.valor, valor );
}

const char* variables_obtener ( Variables* variables, const char* clave )
{
  // Buscamos el nodo, si existe, de esta variable.
  NodoHash* nodo = variables_buscar_nodo ( variables, clave );

  if ( nodo != NULL )
  {
    return nodo->variable.valor;
  }
  else
  {
    return NULL;
  }
}



char* variables_procesar_linea ( Variables* variables, char* linea, char* nuevaLinea )
{
  char* p;

  // Primero comprobamos si hay alguna variable.
  char* primerEspacio = strchr ( linea, ' ' );
  char* igualdad = strchr ( linea, '=' );

  int hayAsignacion = ( igualdad != NULL ) && ( ( primerEspacio == NULL ) || ( igualdad < primerEspacio ) );
  int hayVariables = hayAsignacion || ( strchr ( linea, '$' ) != NULL );
  if ( !hayVariables )
  {
    return linea;
  }

  // Comprobamos si están intentando asignar un valor a una variable.
  if ( igualdad != NULL )
  {
    // Buscamos el primer espacio. Si la igualdad está antes que este, o no
    // hay espacios, entonces es una asignación.
    p = strchr ( linea, ' ' );
    if ( ( p == NULL ) || ( p > igualdad ) )
    {
      *igualdad = '\0';
      ++igualdad;

      if ( p != NULL )
      {
        *p = '\0';
        --p;
      }
      else
      {
        p = igualdad + strlen ( igualdad ) - 1;
      }

      // Si el valor está entrecomillado, eliminamos las comillas.
      if ( ( ( igualdad[0] == '"' ) && ( *p == '"' ) ) ||
           ( ( igualdad[0] == '\'' ) && ( *p == '\'' ) ) 
         )
      {
        ++igualdad;
        *p = '\0';
      }

      // Establecemos el valor de la variable.
      variables_establecer ( variables, linea, igualdad );

      // Retornamos una linea vacía.
      nuevaLinea[0] = '\0';
    }
  }

  else
  {
    nuevaLinea[0] = '\0';

    // Procesamos la linea.
    InfoLinea info;
    infolinea_procesar ( &info, linea, NULL, -1 );

    if ( info.numProgramas > 0 )
    {
      int i;
      int j;

      for ( i = 0; i < info.numProgramas; ++i )
      {
        for ( j = 0; j < info.programas[i].argc; ++j )
        {
          char* arg = info.programas[i].argv[j];
          if ( arg[0] == '$' )
          {
            // Reemplazamos una variable por su valor.
            const char* valor = variables_obtener ( variables, &(arg[1]) );
            if ( valor != NULL )
            {
              strcat ( nuevaLinea, valor );
              strcat ( nuevaLinea, " " );
            }
            else
            {
              strcat ( nuevaLinea, arg );
              strcat ( nuevaLinea, " " );
            }
          }
          else
          {
            strcat ( nuevaLinea, arg );
            strcat ( nuevaLinea, " " );
          }
        }

        if ( i < ( info.numProgramas - 1 ) )
        {
          strcat ( nuevaLinea, "| " );
        }
      }

      if ( info.ficheroSalida[0] != '\0' )
      {
        if ( info.salidaAgregada )
        {
          strcat ( nuevaLinea, ">>" );
        }
        else
        {
          strcat ( nuevaLinea, ">" );
        }
        strcat ( nuevaLinea, info.ficheroSalida );
        strcat ( nuevaLinea, " ");
      }
      if ( info.ejecutarEnSpawn )
      {
        strcat ( nuevaLinea, "& " );
      }
    }
  }

  return nuevaLinea;
}

