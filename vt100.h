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
 * FICHERO:       vt100.h
 * DESCRIPCIÓN:   Declaración de las secuencias de escape concretas de VT100.
 * AUTORES:       Alberto Alonso Pinto <rydencillo@gmail.com>
 *
 * CAMBIOS:
 * - (2009-2010) Código fuente inicial.
 */

#pragma once

#include "terminal.h"

static const TerminalSecuenciasEscape* obtenerSecuenciasEscapeVT100 ()
{
  static TerminalSecuenciasEscape vt100 = {
    .maxBytes = 4,
    .numSecuencias = 13,
    .secuencias = {
       { FLECHA_ARRIBA,     "\033[A"  }
      ,{ FLECHA_ABAJO,      "\033[B"  }
      ,{ FLECHA_IZQUIERDA,  "\033[D"  }
      ,{ FLECHA_DERECHA,    "\033[C"  }
      ,{ IR_INICIO,         "\033[1~" }
      ,{ IR_FINAL,          "\033[4~" }
      ,{ PAGINA_ARRIBA,     "\033[5~" }
      ,{ PAGINA_ABAJO,      "\033[6~" }

      ,{ LINEA_LIMPIAR_DERECHA,   "\033[K"  }
      ,{ LINEA_LIMPIAR_DERECHA,   "\033[0K" }
      ,{ LINEA_LIMPIAR_IZQUIERDA, "\033[1K" }
      ,{ LINEA_LIMPIAR_ENTERA,    "\033[2K" }
      ,{ LINEA_SUPRIMIR,          "\033[3~" }
    }
  };

  return &vt100;
}
