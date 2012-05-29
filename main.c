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
 * FICHERO:       main.c
 * DESCRIPCIÓN:   Punto de entrada y bucle de lectura.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "aliases.h"
#include "config.h"
#include "comandos.h"
#include "io.h"
#include "prompt.h"
#include "comodines.h"
#include "terminal.h"
#include "historial.h"
#include "variables.h"

static int continuar = 1;
static Linea* linea;
static Linea lineaActual;
static Linea lineaHistorial;
static int posicion_historial = -1;

static void sighandler ( int signum )
{
  switch ( signum )
  {
    case SIGTERM:
      continuar = 0;
      break;

    case SIGINT:
      writef ( 1, "\r\n" );
      mostrar_prompt ();
      linea_inicializar ( linea );
      linea = &lineaActual;
      posicion_historial = -1;
      break;
  }
}

int main ( int argc, const char* argv[], char* envp[] )
{
  linea = &lineaActual;

  // Definido en historial.h
  Historial* hist = historial_obtener_instancia ();

  // Definido en variables.h
  Variables* vars = variables_crear ();

  // Definido en aliases.h
  Aliases* aliases = aliases_obtener_instancia ();

  // Inicializaciones.
  linea_inicializar ( &lineaActual );
  linea_inicializar ( &lineaHistorial );
  historial_cargar_desde_fichero ( hist, FICHERO_HISTORIAL );
  historial_guardar_a_fichero ( hist, FICHERO_HISTORIAL );
  registrar_comandos_internos ();

  // Señales.
  signal ( SIGTERM, sighandler );
  signal ( SIGINT, sighandler );

  // Mostramos el prompt.
  mostrar_prompt ();

  // Bucle principal.
  char c;
  int n = 0;
  while ( continuar && ( ( n = getch ( &c ) ) == 1 ) )
  {
    switch ( c )
    {
      case 4:
        // CTRL+D
        if ( linea->len == 0 )
        {
          continuar = 0;
          writef ( 1, "exit\n" );
        }
        break;

      case 11:
        // CTRL+K
        ejecutar_secuencia_escape ( LINEA_LIMPIAR_DERECHA );
        linea_eliminar_desde_cursor ( linea );
        break;
      case 12:
        // CTRL+L (Limpiar pantalla)
        hacer_eco ( 12 );
        linea_mostrar_reset ( linea );
        break;
      case '\t':
        procesar_sugerencias ( linea );
        break;
      case 8:
        // CTRL+H
      case 127:
        // Backspace
        linea_backspace ( linea );
        break;
      case 27:
        // Secuencia de escape
        linea_anyadir_secuencia_escape ( linea, c, hacer_eco );
        break;
      default:
        // Almacenamos el caracter leido
        if ( c != '\n' )
        {
          linea_anyadir ( linea, c, hacer_eco );

          // Verificamos si tenemos que mostrar secuencias de escape y procesamos
          // algunos casos concretos.
          CodigoSecuencia codigoEscape = procesar_secuencia_escape ( linea, NULL );
          switch ( codigoEscape )
          {
            case SECUENCIA_MAXIMA:
            case SECUENCIA_NO_INICIADA:
            case SECUENCIA_NO_FINALIZADA:
              break;
            case SECUENCIA_INEXISTENTE:
              linea_mostrar_secuencia_escape ( linea, hacer_eco );
              linea_limpiar_secuencia_escape ( linea );
              break;

            default:
              if ( linea_procesar_secuencia_escape ( linea, codigoEscape ) == 1 )
              {
                ejecutar_secuencia_escape ( codigoEscape );
              }
              linea_limpiar_secuencia_escape ( linea );
              break;

            // Casos concretos.
            // Historial.
            case FLECHA_ARRIBA:
            {
              char* lineaContenido;

              linea_limpiar_secuencia_escape ( linea );

              ++posicion_historial;
              lineaContenido = historial_obtener ( hist, posicion_historial );
              if ( lineaContenido == NULL )
              {
                --posicion_historial;
              }
              else
              {
                if ( linea == &lineaActual )
                  linea = &lineaHistorial;
                linea_cargar_contenido ( linea, lineaContenido );
                linea_mostrar_reset ( linea );
              }

              break;
            }

            case FLECHA_ABAJO:
            {
              char* lineaContenido;

              linea_limpiar_secuencia_escape ( linea );

              if ( posicion_historial > -1 )
              {
                --posicion_historial;
              
                if ( posicion_historial > -1 )
                {
                  lineaContenido = historial_obtener ( hist, posicion_historial );
                  if ( lineaContenido != NULL )
                  {
                    linea = &lineaHistorial;
                    linea_cargar_contenido ( linea, lineaContenido );
                    linea_mostrar_reset ( linea );
                  }
                }
                else
                {
                  linea = &lineaActual;
                  linea->cursor = linea->len;
                  linea_mostrar_reset ( linea );
                }
              }

              break;
            }

            case PAGINA_ARRIBA:
            {
              char* lineaContenido;

              linea_limpiar_secuencia_escape ( linea );

              posicion_historial += 5;
              if ( posicion_historial >= historial_tamanyo ( hist ) )
                posicion_historial = historial_tamanyo ( hist ) - 1;

              lineaContenido = historial_obtener ( hist, posicion_historial );
              if ( lineaContenido != NULL )
              {
                if ( linea == &lineaActual )
                  linea = &lineaHistorial;
                linea_cargar_contenido ( linea, lineaContenido );
                linea_mostrar_reset ( linea );
              }

              break;
            }

            case PAGINA_ABAJO:
            {
              char* lineaContenido;

              linea_limpiar_secuencia_escape ( linea );
              if ( posicion_historial > -1 )
              {
                posicion_historial -= 5;
                if ( posicion_historial < 0 )
                  posicion_historial = -1;

                if ( posicion_historial > -1 )
                {
                  lineaContenido = historial_obtener ( hist, posicion_historial );
                  if ( lineaContenido != NULL )
                  {
                    linea = &lineaHistorial;
                    linea_cargar_contenido ( linea, lineaContenido );
                    linea_mostrar_reset ( linea );
                  }
                }
                else
                {
                  linea = &lineaActual;
                  linea_mostrar_reset ( linea );
                }
              }
            }
          }
        }

        // Si leemos un salto de linea, el comando ha sido introducido completamente.
        else
        {
          // Nos aseguramos de que no queden secuencias de escape iniciadas pero no finalizadas.
          linea_mostrar_secuencia_escape ( linea, hacer_eco );
          linea_limpiar_secuencia_escape ( linea );

          // Introducimos el salto de linea en pantalla.
          hacer_eco ( '\r' );
          hacer_eco ( '\n' );

          // Procesamos el comando introducido.
          if ( procesar_comando ( linea_hacer_string( linea ), envp, vars, aliases ) == COMANDO_SALIR )
            continuar = 0;

          // Reiniciamos el acceso al historial.
          linea = &lineaActual;
          posicion_historial = -1;
          linea_inicializar ( linea );

          if ( continuar )
            mostrar_prompt ();
        }
    }
  }

  // Finalizaciones
  historial_eliminar ( hist );
  variables_eliminar ( vars );
  aliases_eliminar ( aliases );

  // Verificamos si ha ocurrido algun error leyendo la linea.
  if ( n == -1 )
    error ( "getch" );

  return 0;
}
