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
 * FICHERO:       aliases.c
 * DESCRIPCIÓN:   Código fuente de los aliases.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aliases.h"
#include "config.h"
#include "infolinea.h"
#include "io.h"

// Estructura para almacenar un alias.
typedef struct
{
  char alias [ 256 ];
  char valor [ 256 ];
} Alias;

// Nodos de la tabla hash.
typedef struct _NodoHash
{
  Alias alias;
  struct _NodoHash* siguiente;
  struct _NodoHash* anterior;
} NodoHash;

// Tabla hash de aliases.
struct Aliases_
{
  NodoHash* nodos [ ALIASES_TABLA_HASH_TAMANYO ];
};

static inline unsigned int aliases_hash ( const char* str )
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

Aliases* aliases_obtener_instancia ()
{
  static Aliases* instancia = NULL;
  if ( instancia == NULL )
  {
    instancia = aliases_crear ();
  }
  return instancia;
}

Aliases* aliases_crear ()
{
  Aliases* aliases = (Aliases *)malloc(sizeof(Aliases));
  memset ( aliases, 0, sizeof(Aliases) );
  return aliases;
}

void aliases_eliminar ( Aliases* aliases )
{
  int i;
  for ( i = 0; i < ALIASES_TABLA_HASH_TAMANYO; ++i )
  {
    if ( aliases->nodos[i] != NULL )
    {
      NodoHash* actual;
      NodoHash* siguiente;
      for ( actual = aliases->nodos[i]; actual != NULL; actual = siguiente )
      {
        siguiente = actual->siguiente;
        free ( actual );
      }
    }
  }

  free ( aliases );
}

static NodoHash* aliases_buscar_nodo ( Aliases* aliases, const char* alias )
{
  NodoHash* nodo = NULL;
  NodoHash* actual;
  unsigned int hash = aliases_hash ( alias );
  unsigned int pos = ( hash % ALIASES_TABLA_HASH_TAMANYO );

  actual = aliases->nodos [ pos ];
  for (; ( nodo == NULL ) && ( actual != NULL ); actual = actual->siguiente )
  {
    if ( strcmp ( actual->alias.alias, alias ) == 0 )
    {
      nodo = actual;
    }
  }

  return nodo;
}

void aliases_establecer ( Aliases* aliases, const char* alias, const char* valor )
{
  // Buscamos el nodo, si existe, de este alias.
  NodoHash* nodo = aliases_buscar_nodo ( aliases, alias );

  // Si no se ha encontrado, creamos uno nuevo.
  if ( nodo == NULL )
  {
    unsigned int hash = aliases_hash ( alias );
    unsigned int pos = ( hash % ALIASES_TABLA_HASH_TAMANYO );

    nodo = (NodoHash *)malloc(sizeof(NodoHash));
    memset ( nodo, 0, sizeof(NodoHash) );
    strcpy ( nodo->alias.alias, alias );
    if ( aliases->nodos [ pos ] )
    {
      aliases->nodos [ pos ]->anterior = nodo;
    }
    nodo->siguiente = aliases->nodos [ pos ];
    nodo->anterior = NULL;
    aliases->nodos [ pos ] = nodo;
  }

  // Cambiamos el valor.
  strcpy ( nodo->alias.valor, valor );
}

void aliases_eliminar_alias ( Aliases* aliases, const char* alias )
{
  // Buscamos el nodo.
  NodoHash* nodo = aliases_buscar_nodo ( aliases, alias );

  if ( nodo != NULL )
  {
    unsigned int hash = aliases_hash ( alias );
    unsigned int pos = ( hash % ALIASES_TABLA_HASH_TAMANYO );

    if ( nodo == aliases->nodos [ pos ] )
    {
      aliases->nodos [ pos ] = aliases->nodos [ pos ]->siguiente;
    }
    else
    {
      if ( nodo->anterior != NULL )
      {
        nodo->anterior->siguiente = nodo->siguiente;
      }
      if ( nodo->siguiente != NULL )
      {
        nodo->siguiente->anterior = nodo->anterior;
      }
    }

    free ( nodo );
  }
}

char* aliases_procesar_linea ( Aliases* aliases, char* linea, char* nuevaLinea )
{
  // Procesamos la linea.
  InfoLinea info;
  infolinea_procesar ( &info, linea, NULL, -1 );

  // Nos aseguramos de que haya algún programa.
  if ( info.numProgramas == 0 )
  {
    return linea;
  }

  // Reconstruímos la linea reemplazando los aliases.
  int i;
  int j;
  NodoHash* nodo;
  nuevaLinea[0] = '\0';
  for ( i = 0; i < info.numProgramas; ++i )
  {
    for ( j = 0; j < info.programas[i].argc; ++j )
    {
      if ( j == 0 )
      {
        // ¿Es un alias?
        nodo = aliases_buscar_nodo ( aliases, info.programas[i].argv[0] );
        if ( nodo == NULL )
        {
          // No lo es.
          strcat ( nuevaLinea, info.programas[i].argv[0] );
          strcat ( nuevaLinea, " " );
        }
        else
        {
          // Sí lo es.
          strcat ( nuevaLinea, nodo->alias.valor );
          strcat ( nuevaLinea, " " );
        }
      }
      else
      {
        strcat ( nuevaLinea, info.programas[i].argv[j] );
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

  return nuevaLinea;
}

void aliases_mostrar ( Aliases* aliases, const char* alias )
{
  int continuar = 1;
  NodoHash* actual;
  int i;

  for ( i = 0; continuar && ( i < ALIASES_TABLA_HASH_TAMANYO ); ++i )
  {
    for ( actual = aliases->nodos[i]; continuar && ( actual != NULL ); actual = actual->siguiente )
    {
      if ( ( alias == NULL ) || ( strcmp ( alias, actual->alias.alias ) == 0 ) )
      {
        writef ( 1, "alias %s='%s'\n", actual->alias.alias, actual->alias.valor );
        continuar = ( alias == NULL );
      }
    }
  }
}

