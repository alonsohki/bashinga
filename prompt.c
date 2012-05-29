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
 * FICHERO:       prompt.c
 * DESCRIPCIÓN:   Generación y mostrado del prompt.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "io.h"
#include "prompt.h"

static char prompt_procesado [ 256 ];
static char prompt_procesado_base [ 256 ] = { 0 };
static char prompt_ultimo_usuario [ 256 ];
static char prompt_ultimo_host [ 256 ];
static char prompt_ultimo_dir [ 256 ];

static void procesar_prompt ( const char* prompt )
{
  int p = 0;
  int i;
  int enEscape = 0;
  // Valor acumulado para secuencias de escape como \033 (en octal).
  unsigned int valor_acumulado;

  // Almacenamos los datos actuales de usuario, host y directorio.
  const char* tmp = getenv ( "USER" );
  if ( tmp != NULL )
    strcpy ( prompt_ultimo_usuario, tmp );
  else
    prompt_ultimo_usuario[0] = '\0';

  tmp = getenv ( "HOSTNAME" );
  if ( tmp != NULL )
    strcpy ( prompt_ultimo_host, tmp );
  else
    prompt_ultimo_host[0] = '\0';

  tmp = getenv ( "PWD" );
  if ( tmp != NULL )
    strcpy ( prompt_ultimo_dir, tmp );
  else
    prompt_ultimo_dir[0] = '\0';


  // Interpretamos los caracteres del prompt.
  for ( i = 0; prompt[i] != '\0'; ++i )
  {
    switch ( prompt[i] )
    {
      case '\\':
        if ( enEscape )
        {
          prompt_procesado [ p ] = '\\';
          enEscape = 0;
        }
        else
        {
          enEscape = 1;
          valor_acumulado = 0;
        }
        break;

      case 'n':
        if ( enEscape )
          prompt_procesado [ p ] = '\n';
        else
          prompt_procesado [ p ] = 'n';
        p++;
        enEscape = 0;
        break;

      case 'r':
        if ( enEscape )
          prompt_procesado [ p ] = '\r';
        else
          prompt_procesado [ p ] = 'r';
        p++;
        enEscape = 0;
        break;

      case 't':
        if ( enEscape )
          prompt_procesado [ p ] = '\t';
        else
          prompt_procesado [ p ] = 't';
        p++;
        enEscape = 0;
        break;

      case 'u':
        if ( enEscape )
        {
          int len = strlen(prompt_ultimo_usuario);
          strcpy ( &(prompt_procesado[p]), prompt_ultimo_usuario );
          p += len;
          enEscape = 0;
        }
        else
          prompt_procesado [ p ] = prompt[i];
        break;

      case 'H':
      case 'h':
        if ( enEscape )
        {
          int len = strlen(prompt_ultimo_host);
          char* ptr;
          strcpy ( &(prompt_procesado[p]), prompt_ultimo_host );

          // Comprobamos si debemos utilizar el host reducido.
          if ( prompt[i] == 'h' )
          {
            // Si el nombre de host es compuesto, nos quedamos sólo con el primer nombre.
            ptr = strchr ( &(prompt_procesado[p]), '.' );
            if ( ptr != NULL )
            {
              len = ptr - &(prompt_procesado[p]);
              *ptr = '\0';
            }
          }

          p += len;
          enEscape = 0;
        }
        else
          prompt_procesado [ p ] = prompt[i];
        break;

      case 'W':
      case 'w':
        if ( enEscape )
        {
          const char* pwd = prompt_ultimo_dir;
          if ( pwd != NULL )
          {
            // Comprobamos si debemos utilizar el directorio reducido.
            if ( ( prompt[i] == 'W' ) && ( strcmp ( pwd, "/" ) != 0 ) )
            {
              char* temp = strrchr ( pwd, '/' );
              if ( temp )
                pwd = temp + 1;
            }

            int len = strlen(pwd);
            strcpy ( &(prompt_procesado[p]), pwd );
            p += len;
          }
          enEscape = 0;
        }
        else
          prompt_procesado [ p ] = prompt[i];
        break;

      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7':
        if ( enEscape )
        {
          valor_acumulado = ( valor_acumulado << 3 ) | ( prompt[i] - '0' );
          if ( valor_acumulado > 255 )
          {
            valor_acumulado >>= 3;
            prompt_procesado [ p ] = valor_acumulado;
            p++;
            enEscape = 0;
          }
        }
        else
        {
          prompt_procesado [ p ] = prompt[i];
          p++;
        }
        break;

      default:
        if ( enEscape && valor_acumulado > 0 )
        {
          prompt_procesado [ p ] = valor_acumulado;
          p++;
        }
        enEscape = 0;
        prompt_procesado [ p ] = prompt[i];
        p++;
        break;
    }
  }

  prompt_procesado [ p ] = '\0';
}

static inline int hay_que_actualizar_prompt ()
{
  const char* user = getenv ( "USER" );
  const char* host = getenv ( "HOSTNAME" );
  const char* dir = getenv ( "PWD" );

  if ( user && ( strcmp ( user, prompt_ultimo_usuario ) != 0 ) )
    return 1;
  if ( host && ( strcmp ( host, prompt_ultimo_host ) != 0 ) )
    return 1;
  if ( dir && ( strcmp ( dir, prompt_ultimo_dir ) != 0 ) )
    return 1;

  return 0;
}

void mostrar_prompt ()
{
  const char* PS1 = getenv ( "PROMPT" );
  if ( PS1 == NULL )
    PS1 = PROMPT_POR_DEFECTO;

  if ( hay_que_actualizar_prompt() || ( strcmp ( prompt_procesado_base, PS1 ) != 0 ) )
  {
    strcpy ( prompt_procesado_base, PS1 );
    procesar_prompt ( PS1 );
  }

  writef ( 1, prompt_procesado );
}
