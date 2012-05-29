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
 * FICHERO:       comandos.c
 * DESCRIPCIÓN:   Ejecución de los comandos introducidos y comandos internos.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "aliases.h"
#include "comandos.h"
#include "comodines.h"
#include "historial.h"
#include "infolinea.h"
#include "io.h"
#include "variables.h"

static struct
{
  const char* comando;
  CommandState (*fn)(int argc, char* argv[]);
} comandos_internos [ 128 ];
static int numComandosInternos = 0;

static CommandState ejecutar_comando_interno ( int argc, char* argv[] );
int es_comando_interno ( const char* comando )
{
  int i;
  int esInterno = 0;
  for ( i = 0; !esInterno && ( i < numComandosInternos ); ++i )
  {
    if ( strcmp ( comando, comandos_internos[i].comando ) == 0 )
    {
      esInterno = 1;
    }
  }

  return esInterno;
}

CommandState procesar_comando ( char* line, char* envp[], Variables* vars, Aliases* aliases )
{
  int i;
  InfoLinea info;
  CommandState state = COMANDO_OK;
  CommandState stateComandoInterno;
  Historial* hist = historial_obtener_instancia ();
  pid_t ultimoHijo;

  if ( line[0] == '\0' )
  {
    // No aceptamos lineas vacias.
    return COMANDO_OK;
  }

  else if ( line[0] == '!' )
  {
    // Busqueda en el historial.
    char* newLine = historial_empieza_por ( hist, &(line[1]) );
    if ( newLine )
    {
      writef ( 1, "%s\n", newLine );
      strcpy ( line, newLine );
    }
  }

  // Agregamos la linea leída al historial.
  historial_anyadir ( hist, line );

  // Reemplazamos los comodines.
  char nuevaLinea [ 1024 ];
  line = reemplazar_comodines ( line, nuevaLinea );

  // Reemplazamos las variables.
  char nuevaLinea2 [ 1024 ];
  line = variables_procesar_linea ( vars, line, nuevaLinea2 );
  if ( line[0] == '\0' )
  {
    // Era una asignación de variables, paramos.
    return COMANDO_OK;
  }

  // Reemplazamos los alises.
  char nuevaLinea3 [ 1024 ];
  line = aliases_procesar_linea ( aliases, line, nuevaLinea3 );

  // Extraemos la información de la línea.
  infolinea_procesar ( &info, line, NULL, -1 );

  // Si sólo tenemos un programa, es un comando interno, y no hay redirecciones,
  // lo ejecutamos diréctamente en el padre.
  if ( ( info.numProgramas == 1 ) &&
       ( es_comando_interno ( info.programas[0].argv[0] ) == 1 ) &&
       ( info.ficheroSalida[0] == '\0' )
     )
  {
    char str[8];
    state = ejecutar_comando_interno ( info.programas[0].argc, info.programas[0].argv );
    sprintf ( str, "%d", (int)state );
    variables_establecer ( vars, "?", str );
  }
  else
  {
    // Creamos un proceso hijo por cada comando a procesar.
    for ( i = 0; i < info.numProgramas; ++i )
    {
      // Generamos los pipes para la cadena.
      if ( ( i + 1 ) != info.numProgramas )
      {
        if ( pipe ( info.programas[i].pipe_io ) == -1 )
        {
          perror("pipe");
          return COMANDO_ERROR;
        }
      }

      ultimoHijo = fork ();
      switch ( ultimoHijo )
      {
        case -1:
          perror("fork");
          state = COMANDO_ERROR;
          break;
        case 0:
        {
          // Redireccionamos la entrada y salida estándar cuando sea apropiado.
          if ( ( i + 1 ) != info.numProgramas )
          {
            if ( close(1) == -1 )
            {
              perror("close");
              exit ( COMANDO_ERROR );
            }
            if ( dup ( info.programas[i].pipe_io[1] ) != 1 )
            {
              perror("dup");
              exit ( COMANDO_ERROR );
            }
            if ( close ( info.programas[i].pipe_io[1] ) == -1 || close ( info.programas[i].pipe_io[0] ) == -1 )
            {
              perror("close");
              exit ( COMANDO_ERROR );
            }
          }
          if ( i > 0 )
          {
            if ( close(0) == -1 )
            {
              perror("close");
              exit ( COMANDO_ERROR );
            }
            if ( dup ( info.programas[i - 1].pipe_io[0] ) != 0 )
            {
              perror("dup");
              exit ( COMANDO_ERROR );
            }
            if ( close ( info.programas[i - 1].pipe_io[0] ) == -1 )
            {
              perror("close");
              exit ( COMANDO_ERROR );
            }
          }
          if ( (i == ( info.numProgramas - 1 )) && (info.ficheroSalida[0] != '\0') )
          {
            int flags;

            if ( close(1) == -1 )
            {
              perror("close");
              exit ( COMANDO_ERROR );
            }

            flags = O_WRONLY | O_CREAT;
            if ( info.salidaAgregada )
              flags |= O_APPEND;
            else
              flags |= O_TRUNC;

            if ( open ( info.ficheroSalida, flags, S_IREAD|S_IWRITE ) != 1 )
            {
              perror("open");
              exit ( COMANDO_ERROR );
            }
          }

          // Comprobamos si es un comando interno.
          stateComandoInterno = ejecutar_comando_interno ( info.programas[i].argc, info.programas[i].argv );
          if ( stateComandoInterno != COMANDO_INTERNO_INEXISTENTE )
          {
            state = stateComandoInterno;
          }
          else
          {
            execvp ( info.programas[i].argv[0], info.programas[i].argv );
            perror ( "execvp" );
            state = COMANDO_ERROR;
          }

          exit ( state );
          break;
        }
      }

      // Cerramos el pipe en el proceso padre.
      if ( ( i + 1 ) != info.numProgramas )
      {
        if ( close ( info.programas[i].pipe_io[1] ) == -1 )
        {
          perror ( "close" );
        }
      }
      if ( i > 0 )
      {
        if ( close ( info.programas[i - 1].pipe_io[0] ) == -1 )
        {
          perror ( "close" );
        }
      }
    }

    // Si ejecutamos en modo RUN, esperamos.
    if ( !info.ejecutarEnSpawn )
    {
      pid_t hijoTerminado;
      int codigoRetorno;

      do
      {
        hijoTerminado = wait ( &codigoRetorno );

        // Si ha terminado el último hijo, guardamos su código de retorno
        // en la variable $?.
        if ( hijoTerminado == ultimoHijo )
        {
          char str [ 8 ];
          sprintf ( str, "%d", codigoRetorno );
          variables_establecer ( vars, "?", str );
        }
      } while ( hijoTerminado != -1 );
    }
  }

  return state;
}

// Comandos internos.
static void anyadirComandoInterno ( const char* comando, CommandState (*fn)(int argc, char* argv[]) )
{
  comandos_internos[numComandosInternos].comando = comando;
  comandos_internos[numComandosInternos].fn = fn;
  ++numComandosInternos;
}

static CommandState ejecutar_comando_interno ( int argc, char* argv[] )
{
  int i;

  for ( i = 0; i < numComandosInternos; ++i )
  {
    if ( !strcmp ( comandos_internos[i].comando, argv[0] ) )
      return comandos_internos[i].fn ( argc, argv );
  }

  return COMANDO_INTERNO_INEXISTENTE;
}

static CommandState cmdInterno_exit ( int argc, char* argv[] )
{
  return COMANDO_SALIR;
}

static CommandState cmdInterno_history ( int argc, char* argv[] )
{
  Historial* hist = historial_obtener_instancia ();
  historial_mostrar ( hist, 1 );
  return COMANDO_OK;
}

static CommandState cmdInterno_cd ( int argc, char* argv[] )
{
  char cwd [ 256 ];
  struct stat estado;

  // Si no especifican un parámetro, nos vamos al HOME.
  if ( argc < 2 )
  {
    const char* destino = getenv ( "HOME" );
    if ( destino == NULL )
    {
      write ( 2, "cd: Número de parámetros incorrecto.\n", 37 );
      return COMANDO_ERROR;
    }
    else
    {
      strcpy ( cwd, destino );
    }
  }
  else
  {
    // Comprobamos si es una ruta absoluta.
    if ( *argv[1] == '/' )
    {
      strcpy ( cwd, argv[1] );
    }
    else
    {
      if ( getcwd ( cwd, sizeof ( cwd ) / sizeof ( cwd[0] ) ) == NULL )
      {
        perror ( "cd: getcwd" );
        return COMANDO_ERROR;
      }

      strcat ( cwd, "/" );
      strcat ( cwd, argv[1] );
    }
  }

  if ( stat ( cwd, &estado ) == -1 )
  {
    perror ( "cd: stat" );
    return COMANDO_ERROR;
  }

  if ( !S_ISDIR ( estado.st_mode ) )
  {
    write ( 2, "cd: No es un directorio.\n", 25 );
    return COMANDO_ERROR;
  }

  if ( chdir ( cwd ) == -1 )
  {
    perror ( "cd: chdir" );
    return COMANDO_ERROR;
  }

  if ( getcwd ( cwd, sizeof ( cwd ) / sizeof ( cwd[0] ) ) == NULL )
  {
    perror ( "cd: getcwd" );
    return COMANDO_ERROR;
  }

  if ( setenv ( "PWD", cwd, 1 ) != 0 )
  {
    perror ( "cd: setenv" );
    return COMANDO_ERROR;
  }

  return COMANDO_OK;
}

static CommandState cmdInterno_alias ( int argc, char* argv[] )
{
  Aliases* aliases = aliases_obtener_instancia ();

  if ( argc == 1 )
  {
    // Mostramos todos los aliases
    aliases_mostrar ( aliases, NULL );
  }
  else
  {
    // Comprobamos si intentan crear o generar un nuevo alias.
    char* p = strchr ( argv[1], '=' );
    if ( p == NULL )
    {
      aliases_mostrar ( aliases, argv[1] );
    }
    else
    {
      // Volcamos todos los parámetros a una cadena.
      char linea [ 256 ];
      int i;

      linea[0] = '\0';
      for ( i = 1; i < argc; ++i )
      {
        strcat ( linea, argv[i] );
        if ( i < ( argc - 1 ) )
        {
          strcat ( linea, " ");
        }
      }
      p = strchr ( linea, '=' );

      // Generamos un nuevo alias.
      char* final;
      *p = '\0';
      ++p;
      final = &( p[ strlen(p) - 1 ] );

      // Eliminamos las comillas si las hubiere.
      if ( ( ( *p == '"' ) && ( *final == '"' ) ) ||
           ( ( *p == '\'' ) && ( *final == '\'' ) )
         )
      {
        p++;
        *final = '\0';
      }

      aliases_establecer ( aliases, linea, p );
    }
  }

  return COMANDO_OK;
}

static CommandState cmdInterno_unalias ( int argc, char* argv[] )
{
  Aliases* aliases = aliases_obtener_instancia ();

  if ( argc < 2 )
  {
    write ( 2, "unalias: Se requiere un parámetro.\n", 35 );
    return COMANDO_ERROR;
  }

  aliases_eliminar_alias ( aliases, argv[1] );
  return COMANDO_OK;
}

void registrar_comandos_internos ()
{
  anyadirComandoInterno ( "exit", cmdInterno_exit );
  anyadirComandoInterno ( "logout", cmdInterno_exit );
  anyadirComandoInterno ( "history", cmdInterno_history );
  anyadirComandoInterno ( "cd", cmdInterno_cd );
  anyadirComandoInterno ( "alias", cmdInterno_alias );
  anyadirComandoInterno ( "unalias", cmdInterno_unalias );
}

