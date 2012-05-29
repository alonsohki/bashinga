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
 * FICHERO:       comodines.c
 * DESCRIPCIÓN:   Comodines y sugerencias.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "comodines.h"
#include "infolinea.h"
#include "io.h"
#include "match.h"
#include "terminal.h"

typedef struct _ListaSugerencias
{
  char sugerencia [ 256 ];
  int esDirectorio;
  struct _ListaSugerencias* siguiente;
  struct _ListaSugerencias* anterior;
} ListaSugerencias;

static void rellenar_en_cursor_con_sugerencia ( Linea* linea, const char* sugerencia, int esDefinitiva );

static void eliminar_lista_sugerencias ( ListaSugerencias* lista )
{
  ListaSugerencias* siguiente;
  for ( ; lista != NULL; lista = siguiente )
  {
    siguiente = lista->siguiente;
    free ( lista );
  }
}

static ListaSugerencias* crear_nodo_sugerencias ( char* sugerencia, int esDirectorio )
{
  ListaSugerencias* nodo = (ListaSugerencias *)malloc(sizeof(ListaSugerencias));
  strcpy ( nodo->sugerencia, sugerencia );
  nodo->esDirectorio = esDirectorio;
  nodo->siguiente = NULL;
  nodo->anterior = NULL;

  return nodo;
}

static void ordenar_lista_sugerencias ( ListaSugerencias** primero, int tamanyoLista )
{
  if ( tamanyoLista > 1 )
  {
    int ordenada;

    do
    {
      ListaSugerencias* actual;
      ListaSugerencias* siguiente;
      ListaSugerencias* anterior;
      ordenada = 1;

      for ( actual = *primero; actual->siguiente != NULL; )
      {
        siguiente = actual->siguiente;

        // Si el nodo actual es mayor que el siguiente, muévelo una posición hacia delante.
        if ( strcmp ( actual->sugerencia, siguiente->sugerencia ) > 0 )
        {
          // Marca la lista como no ordenada.
          ordenada = 0;

          anterior = actual->anterior;

          // Si el actual era el primero, actualiza como primero a su siguiente.
          if ( actual == *primero )
            *primero = siguiente;

          // Desliga el nodo actual de la lista.
          siguiente->anterior = anterior;
          if ( anterior != NULL )
            anterior->siguiente = siguiente;

          // Reenlázalo después del siguiente.
          if ( siguiente->siguiente != NULL )
            siguiente->siguiente->anterior = actual;
          actual->siguiente = siguiente->siguiente;
          siguiente->siguiente = actual;
          actual->anterior = siguiente;
        }
        else
        {
          actual = siguiente;
        }
      }

    } while ( !ordenada );
  }
}

static int sugerencias_comienzan_igual ( ListaSugerencias* lista, char* comienzo )
{
  // Primero comprobamos cuánto coinciden la primera y la segunda sugerencia.
  int coincidencia = 0;
  char* primera = lista->sugerencia;
  char* segunda = lista->siguiente->sugerencia;

  while ( ( *primera == *segunda ) && ( *primera != '\0' ) )
  {
    coincidencia++;
    primera++;
    segunda++;
  }

  if ( coincidencia == 0 )
  {
    // Si no coinciden en nada, paramos.
    return 0;
  }

  // Copiamos el comienzo.
  strncpy ( comienzo, lista->sugerencia, coincidencia );
  comienzo [ coincidencia ] = '\0';

  // Verificamos si pasa lo mismo con las demás.
  for ( lista = lista->siguiente->siguiente; lista != NULL; lista = lista->siguiente )
  {
    if ( strncmp ( lista->sugerencia, comienzo, coincidencia ) != 0 )
    {
      // No coinciden, paramos.
      return 0;
    }
  }

  return 1;
}




static int buscar_entradas_sugeridas ( char* argumento, int esEjecutable, ListaSugerencias** sugerencias )
{
  char* p;
  char* comodin;
  char* comodin2;
  int hayComodines;
  int numSugerencias = 0;
  int rutasCompletas = 1;
  char nuevaRuta [ 256 ];

  // Si no nos dan un lugar en el que guardar las sugerencias, simplemente finalizamos.
  if ( sugerencias == NULL )
    return 0;
  *sugerencias = NULL;

  // Creamos el primer nodo, y le asignamos el patrón tal cual, que se irá expandiendo
  // con el siguiente bucle. En caso de ser un ejecutable, comprobamos primero si nos
  // están intentando dar una ruta (buscando el caracter '/'). De no ser así, agregamos
  // una entrada por cada entrada del PATH y exigimos rutas no completas.
  ListaSugerencias* lista;

  if ( !esEjecutable || ( strchr ( argumento, '/' ) != NULL ) )
  {
    lista = crear_nodo_sugerencias ( argumento, 0 );
    *sugerencias = lista;
    numSugerencias++;
  }
  else
  {
    // Añadimos una entrada por cada entrada del PATH.
    const char* PATH = getenv("PATH");
    char path [ 256 ];
    int continuar = 1;
    char* pathActual;
    char* pathSiguiente;
    ListaSugerencias* ultimo;

    // Se requiere un PATH.
    if ( PATH == NULL )
      return 0;

    // Hacemos una copia para poder modificarla.
    strcpy ( path, PATH );
    pathSiguiente = path;

    // Iteramos por los directorios del PATH.
    while ( continuar )
    {
      pathActual = pathSiguiente;
      p = strchr ( pathActual, ':' );
      if ( p == NULL )
      {
        p = pathActual + strlen( pathActual );
        continuar = 0;
      }
      else
      {
        pathSiguiente = p + 1;
      }
      *p = '\0';

      // Eliminamos las barras superfluas al final.
      p--;
      while ( ( p > pathActual ) && ( *p == '/' ) )
      {
        *p = '\0';
        p--;
      }

      // Agregamos el nodo a la lista con este PATH.
      sprintf ( nuevaRuta, "%s/%s", pathActual, argumento );
      lista = crear_nodo_sugerencias ( nuevaRuta, 0 );
      if ( *sugerencias == NULL )
      {
        *sugerencias = lista;
      }
      else
      {
        ultimo->siguiente = lista;
        lista->anterior = ultimo;
      }
      ultimo = lista;
      ++numSugerencias;
    }

    rutasCompletas = 0;
  }





  // Buscamos las rutas coincidentes.
  ListaSugerencias* actual;
  do
  {
    char ruta [ 256 ];
    char rutaDespues [ 256 ];
    char sugerencia [ 256 ];
    ListaSugerencias* nuevaSugerencia;

    // Buscamos comodines en el primer elemento de la lista.
    actual = *sugerencias;
    comodin = strchr ( actual->sugerencia, '*' );
    comodin2 = strchr ( actual->sugerencia, '?' );
    hayComodines = ( comodin != NULL ) || ( comodin2 != NULL );

    if ( hayComodines )
    {
      // Si aún quedan comodines por procesar, los procesamos para cada elemento de la lista.
      do
      {
        ListaSugerencias* siguiente = actual->siguiente;
        ListaSugerencias* ultimaSugerenciaCreada = NULL;

        // Cogemos el comodín que antes esté.
        if ( comodin == NULL )
          comodin = comodin2;
        else if ( ( comodin2 != NULL ) && ( comodin2 < comodin ) )
          comodin = comodin2;

        // En este punto debería haber siempre un comodín en el patrón.
        assert ( comodin != NULL );





        // Buscamos la ruta que haya antes del comodín.
        p = comodin - 1;
        while ( ( p > actual->sugerencia ) && ( *p != '/' ) )
          --p;
        if ( p <= actual->sugerencia )
        {
          // Caso especial: la sugerencia es /*
          if ( ( p == actual->sugerencia ) && ( *p == '/' ) )
          {
            strcpy ( ruta, "/" );
          }
          else
          {
            // Es el principio de la ruta.
            ruta[0] = '\0';
          }
        }
        else
        {
          strncpy ( ruta, actual->sugerencia, p - actual->sugerencia + 1 );
          ruta [ p - actual->sugerencia + 1 ] = '\0';
        }

        // Buscamos la ruta que hay después del comodín.
        p = strchr ( comodin, '/' );
        if ( p == NULL )
        {
          // Es el final de la ruta.
          rutaDespues[0] = '\0';
        }
        else
        {
          strcpy ( rutaDespues, p );
        }

        // Cogemos el patrón a procesar.
        int len = strlen ( actual->sugerencia ) - strlen ( ruta ) - strlen ( rutaDespues );
        strncpy ( sugerencia, &( actual->sugerencia [ strlen ( ruta ) ] ), len );
        sugerencia [ len ] = '\0';





        // En este punto tenemos las siguientes variables:
        // - ruta: La ruta antes del patrón a buscar.
        // - sugerencia: El patrón a buscar.
        // - rutaDespues: La ruta después del patrón a buscar.
        // Abrimos el directorio para buscar las entradas con la ruta dada.
        DIR* dir;
        struct dirent* entry;
        if ( *ruta == '\0' )
        {
          dir = opendir ( "." );
        }
        else
        {
          dir = opendir ( ruta );
        }

        // No nos interesa abortar la ejecución si no se encuentra el directorio,
        // ya que se espera que todos los directorios que aparezcan aquí hayan sido
        // procesados antes, que el usuario haya dado un directorio inexistente o
        // que uno de los directorios del PATH no exista.
        if ( dir != NULL )
        {
          for ( entry = readdir ( dir ); entry != NULL; entry = readdir ( dir ) )
          {
            char rutaParcial [ 256 ];
            struct stat estado;
            int esValido = 1;

            // Ignoramos ficheros oculto si no están pidiendo un fichero oculto.
            if ( ( sugerencia[0] != '.' ) && ( entry->d_name[0] == '.' ) )
            {
              esValido = 0;
            }

            // Extraemos el estado de la entrada.
            if ( esValido )
            {
              sprintf ( rutaParcial, "%s%s", ruta, entry->d_name );
              if ( stat ( rutaParcial, &estado ) == -1 )
                esValido = 0;
            }

            // Si hay una ruta después del patrón, sólo buscamos directorios.
            if ( esValido && ( rutaDespues[0] != '\0' ) )
            {
              if ( ! S_ISDIR ( estado.st_mode ) )
              {
                esValido = 0;
              }
            }

            // Comprobamos que coincida con el patrón.
            if ( esValido && ( match ( sugerencia, entry->d_name ) != 0 ) )
            {
              esValido = 0;
            }

            // Comprobamos que sea un fichero regular y ejecutable.
            if ( esValido && esEjecutable && ( rutaDespues[0] == '\0' ) )
            {
              // Comprobamos que sea un fichero regular.
              if ( ! S_ISREG ( estado.st_mode ) )
              {
                esValido = 0;
              }

              // Si no es un fichero regular ejecutable, lo ignoramos.
              if ( esValido && ( ( estado.st_mode & ( S_IXUSR | S_IXGRP | S_IXOTH ) ) == 0 ) )
              {
                esValido = 0;
              }
            }

            if ( esValido )
            {
              // Como hay coincidencia, creamos un nuevo nodo en la lista con esta entrada.
              if ( rutasCompletas )
              {
                sprintf ( nuevaRuta, "%s%s%s", ruta, entry->d_name, rutaDespues );
                nuevaSugerencia = crear_nodo_sugerencias ( nuevaRuta, S_ISDIR ( estado.st_mode ) );
              }
              else
              {
                nuevaSugerencia = crear_nodo_sugerencias ( entry->d_name, S_ISDIR ( estado.st_mode ) );
              }

              if ( ultimaSugerenciaCreada == NULL )
              {
                nuevaSugerencia->siguiente = siguiente;
                if ( siguiente )
                  siguiente->anterior = nuevaSugerencia;
                nuevaSugerencia->anterior = actual;
                actual->siguiente = nuevaSugerencia;
              }
              else
              {
                nuevaSugerencia->siguiente = siguiente;
                if ( siguiente )
                  siguiente->anterior = nuevaSugerencia;
                nuevaSugerencia->anterior = ultimaSugerenciaCreada;
                ultimaSugerenciaCreada->siguiente = nuevaSugerencia;
              }
              ultimaSugerenciaCreada = nuevaSugerencia;
              ++numSugerencias;
            }
          }

          closedir ( dir );
        }




        // Pasamos al siguiente elemento de la lista y volvemos a procesar, desligando este
        // nodo de la lista. Se asume que el nodo actual ya ha sido reemplazado con las
        // entradas que cumplían el patrón.
        if ( actual == *sugerencias )
          *sugerencias = actual->siguiente;
        if ( actual->anterior )
          actual->anterior->siguiente = actual->siguiente;
        if ( actual->siguiente )
          actual->siguiente->anterior = actual->anterior;
        free ( actual );
        actual = siguiente;
        --numSugerencias;

        // Buscamos la posición de los comodines del siguiente.
        if ( actual != NULL )
        {
          comodin = strchr ( actual->sugerencia, '*' );
          comodin2 = strchr ( actual->sugerencia, '?' );
        }
      } while ( actual != NULL );
    }
  } while ( ( hayComodines ) && ( *sugerencias != NULL ) );

  return numSugerencias;
}




void procesar_sugerencias ( Linea* linea_ )
{
  int argumentoEsEjecutable = 0;
  int argumentoContieneComodines = 0;
  char argumentoOriginal [ 256 ];
  char argumento [ 256 ];
  int esNuevoArgumentoSugerido = 0;
  ListaSugerencias* sugerencias;

  // Copiamos la linea para no alterarla en el procesado.
  char linea [ MAX_LINEA ];
  strcpy ( linea, linea_hacer_string ( linea_ ) );

  // Casos a evitar:
  // - El cursor está al principio, la línea no está vacía y en el cursor hay un espacio.
  if ( ( linea_->cursor == 0 ) && ( linea_->len > 0 ) && ( linea_->buffer[0] == ' ' ) )
  {
    return;
  }

  // - El cursor está en un espacio, y a su izquierda hay un espacio.
  if ( ( linea_->cursor > 0 ) && ( linea_->buffer[linea_->cursor] == ' ' ) &&
       ( linea_->buffer[linea_->cursor - 1] == ' ' ) )
  {
    return;
  }

  // Comprobamos si están intentando generar un nuevo argumento mediante sugerencias.
  // El cursor está al final, y a su izquierda hay un espacio.
  if ( ( linea_->cursor == linea_->len ) && ( linea_->len > 0 ) &&
       ( linea_->buffer[linea_->cursor - 1] == ' ' ) )
  {
    esNuevoArgumentoSugerido = 1;
  }


  // Procesamos la linea.
  InfoLinea info;
  InfoLineaCursor infoCursor;
  infolinea_procesar ( &info, linea, &infoCursor, linea_->cursor );

  // Comprobamos si el argumento es un ejecutable.
  argumentoEsEjecutable = ( ( infoCursor.argumento == 0 ) || ( info.numProgramas == 0 ) ) && !esNuevoArgumentoSugerido;

  // Obtenemos el argumento.
  if ( ( info.numProgramas == 0 ) || esNuevoArgumentoSugerido )
  {
    strcpy ( argumento, "*" );
  }
  else
  {
    int len;
    strcpy ( argumento, info.programas [ infoCursor.programa ].argv [ infoCursor.argumento ] );
    strcpy ( argumentoOriginal, argumento );

    if ( ( strchr ( argumento, '*' ) != NULL ) || ( strchr ( argumento, '?' ) != NULL ) )
    {
      argumentoContieneComodines = 1;
    }

    // Si el último caracter no es un comodín, añadimos un '*'.
    len = strlen ( argumento );
    if ( len > 0 )
    {
      switch ( argumento [ len - 1 ] )
      {
        case '*':
        case '?':
          break;
        default:
          argumento [ len ] = '*';
          argumento [ len + 1 ] = '\0';
          break;
      }
    }
    else
      strcpy ( argumento, "*" );
  }

  // Eliminamos comodines superfluos.
  collapse ( argumento );

  // Buscamos las sugerencias
  int numSugerencias = buscar_entradas_sugeridas ( argumento, argumentoEsEjecutable, &sugerencias );
  if ( numSugerencias == 1 )
  {
    // Si sólo tenemos una sugerencia, lo sustituímos diréctamente en la linea.
    // En caso de ser un directorio, no se considera definitiva y se añade la / final.
    if ( sugerencias->esDirectorio == 1 )
    {
      if ( sugerencias->sugerencia [ strlen ( sugerencias->sugerencia ) - 1 ] != '/' )
      {
        strcat ( sugerencias->sugerencia, "/" );
      }
      rellenar_en_cursor_con_sugerencia ( linea_, sugerencias->sugerencia, 0 );
    }
    else
    {
      rellenar_en_cursor_con_sugerencia ( linea_, sugerencias->sugerencia, 1 );
    }
  }
  else if ( numSugerencias > 1 )
  {
    // En caso de tener más de una sugerencia, las mostramos por pantalla.
    int i;
    int continuar = 1;

    // Si todas las sugerencias empiezan igual, el argumento no tenía comodines,
    // y el comienzo de las sugerencias es distinto al argumento, autocompletamos
    // la parte coincidente.
    char comienzo_comun [ 256 ];
    if ( !argumentoContieneComodines &&
         sugerencias_comienzan_igual ( sugerencias, comienzo_comun ) &&
         ( strcmp ( comienzo_comun, argumentoOriginal ) != 0 ) )
    {
      rellenar_en_cursor_con_sugerencia ( linea_, comienzo_comun, 0 );
    }
    else
    {
      ordenar_lista_sugerencias ( &sugerencias, numSugerencias );
      writef ( 1, "\n" );

      // Evitamos que cancelen el listado con un CTRL+C
      void (*prevHandler)(int) = signal ( SIGINT, SIG_IGN );

      for ( i = 0; continuar && ( sugerencias != NULL ); sugerencias = sugerencias->siguiente, ++i )
      {
        writef ( 1, "%s\n", sugerencias->sugerencia );
        if ( ( i != 0 ) && ( ( i % 25 ) == 0 ) && ( sugerencias->siguiente != NULL ) )
        {
          // Cada N sugerencias, interrumpimos a la espera de que pida continuar.
          writef ( 1, "--- Pulsa q para parar el listado, cualquier otra tecla para continuar ---" );
          char c;
          if ( getch ( &c ) != 1 )
            continuar = 0;
          else if ( c == 'q' )
            continuar = 0;
          writef ( 1, "\r" );
          ejecutar_secuencia_escape ( LINEA_LIMPIAR_DERECHA );
        }
      }

      linea_mostrar_reset ( linea_ );

      // Restauramos el manejador del SIGINT.
      signal ( SIGINT, prevHandler );
    }
  }

  // Eliminamos la lista de memoria.
  if ( sugerencias )
    eliminar_lista_sugerencias ( sugerencias );
}




static void rellenar_en_cursor_con_sugerencia ( Linea* linea, const char* sugerencia, int esDefinitiva )
{
  char* nuevaLinea = (char *) malloc ( sizeof(char) * ( linea->len + strlen ( sugerencia ) + 2 ) );
  int nuevoCursor;

  char* p;

  // Si han hecho solicitado sugerencias desde el espacio que procede
  // al argumento, decrementamos el cursor.
  if ( ( linea->cursor > 0 ) && ( linea->cursor < linea->len ) && ( linea->buffer [ linea->cursor ] == ' ' ) )
  {
    linea->cursor--;
  }

  // Buscamos el comienzo del argumento.
  char* comienzoArgumento;
  for ( p = &(linea->buffer [ linea->cursor ] );
        ( p > linea->buffer ) && ( *p != ' ' );
        --p );
  comienzoArgumento = p;
  if ( *comienzoArgumento == ' ' )
    ++comienzoArgumento;

  // Buscamos el final del argumento
  char* finalArgumento;
  for ( p = &(linea->buffer [ linea->cursor ] );
        ( *p != '\0' ) && ( *p != ' ' );
        ++p );
  finalArgumento = p;

  // Copiamos la linea hasta el argumento.
  int len = comienzoArgumento - linea->buffer;
  strncpy ( nuevaLinea, linea->buffer, len );

  // Copiamos la sugerencia.
  int sugerenciaLen = strlen(sugerencia);
  strncpy ( &(nuevaLinea[len]), sugerencia, sugerenciaLen );
  len += sugerenciaLen;
  if ( esDefinitiva && ( finalArgumento[0] != ' ' ) )
  {
    nuevaLinea [ len ] = ' ';
    ++len;
  }
  nuevoCursor = len;

  // Copiamos el resto.
  strcpy ( &(nuevaLinea[len]), finalArgumento );

  // Modificamos la linea.
  linea_cargar_contenido ( linea, nuevaLinea );
  linea->cursor = nuevoCursor;
  linea_mostrar_reset ( linea );

  free ( nuevaLinea );
}





char* reemplazar_comodines ( char* linea, char* nuevaLinea )
{
  // Primero comprobamos si hay algún comodín.
  int hayComodines = ( strchr ( linea, '*' ) != NULL ) || ( strchr ( linea, '?' ) != NULL );
  if ( !hayComodines )
  {
    return linea;
  }

  // Procesamos la linea primero.
  InfoLinea info;
  infolinea_procesar ( &info, linea, NULL, -1 );

  if ( info.numProgramas > 0 )
  {
    int i;
    int j;

    // Inicializamos la nueva linea.
    nuevaLinea[0] = '\0';

    for ( i = 0; i < info.numProgramas; ++i )
    {
      for ( j = 0; j < info.programas[i].argc; ++j )
      {
        char* arg = info.programas[i].argv[j];
        hayComodines = ( strchr ( arg, '*' ) != NULL ) || ( strchr ( arg, '?' ) != NULL );

        if ( hayComodines )
        {
          ListaSugerencias* sugerencias;
          int numSugerencias = buscar_entradas_sugeridas ( arg, (j == 0), &sugerencias );
          if ( numSugerencias == 0 )
          {
            strcat ( nuevaLinea, arg );
            strcat ( nuevaLinea, " " );
          }
          else
          {
            ListaSugerencias* actual;
            for ( actual = sugerencias; actual != NULL; actual = actual->siguiente )
            {
              strcat ( nuevaLinea, actual->sugerencia );
              strcat ( nuevaLinea, " " );
            }
          }

          if ( sugerencias )
            eliminar_lista_sugerencias ( sugerencias );
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

    return nuevaLinea;
  }
  else
  {
    return linea;
  }
}

