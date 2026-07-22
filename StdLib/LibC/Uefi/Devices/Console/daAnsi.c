/** @file
  Minimal handler of ANSI escape sequences for UEFI Console.

  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR> This program
  and the accompanying materials are licensed and made available under the
  terms and conditions of the BSD License that accompanies this distribution.
  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include  <Uefi.h>
#include  <Library/BaseLib.h>
#include  <Protocol/SimpleTextIn.h>
#include  <Protocol/SimpleTextOut.h>

#include  <errno.h>
#include  <wctype.h>
#include  <wchar.h>
#include  <stdarg.h>
#include  <sys/termios.h>

#include  <Containers/Fifo.h>
#include  <Device/Console.h>
#include  <Device/IIO.h>

#define MAX_NUMARGS 15

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

static
int
da_ConCtrlExec(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *proto,
               CtrlCharState *state,
               cIIO *iio) 
{
  UINTN cols = 0, rows = 0;
  proto->QueryMode(proto, proto->Mode->Mode, &cols, &rows);
  
  switch(state->cmd) {
    case L'H':
    case L'f':
      if(state->numargs == 0) {
        proto->SetCursorPosition(proto, 0, 0);
      } else if(state->numargs == 2) {
        proto->SetCursorPosition(proto, state->numarg[1], state->numarg[0]);
      } else {
        return 0;
      }
      break;
    case L'A': {
      UINTN delta;
      switch(state->numargs) {
        case 0:
          delta = 1;
          break;
        case 1:
          delta = state->numarg[0];
          break;
        default:
          return 0;
      }
      proto->SetCursorPosition(
          proto,
          proto->Mode->CursorColumn,
          MAX(0, proto->Mode->CursorRow - delta));
    } break;
    case L'B': {
      UINTN delta;
      switch(state->numargs) {
        case 0:
          delta = 1;
          break;
        case 1:
          delta = state->numarg[0];
          break;
        default:
          return 0;
      }

      proto->SetCursorPosition(
          proto,
          proto->Mode->CursorColumn,
          MIN(proto->Mode->CursorRow + delta, rows-1));
    } break;
    case L'C': {
      UINTN delta;
      switch(state->numargs) {
        case 0:
          delta = 1;
          break;
        case 1:
          delta = state->numarg[0];
          break;
        default:
          return 0;
      }

      proto->SetCursorPosition(
          proto,
          MIN(proto->Mode->CursorColumn + delta, cols-1),
          proto->Mode->CursorRow);
    } break;
    case L'D': {
      UINTN delta;
      switch(state->numargs) {
        case 0:
          delta = 1;
          break;
        case 1:
          delta = state->numarg[0];
          break;
        default:
          return 0;
      }
      
      proto->SetCursorPosition(
          proto,
          MAX(0, proto->Mode->CursorColumn - delta),
          proto->Mode->CursorRow);
    } break;
    case L'E': {
      UINTN delta;
      switch(state->numargs) {
        case 0:
          delta = 1;
          break;
        case 1:
          delta = state->numarg[0];
          break;
        default:
          return 0;
      }
      
      proto->SetCursorPosition(
          proto,
          0,
          MIN(proto->Mode->CursorRow + delta, rows-1));
    } break;
    case L'F': {
      UINTN delta;
      switch(state->numargs) {
        case 0:
          delta = 1;
          break;
        case 1:
          delta = state->numarg[0];
          break;
        default:
          return 0;
      }
      
      proto->SetCursorPosition(
          proto,
          0,
          MAX(0, proto->Mode->CursorRow - delta));
    } break;
    case L'G': {
      UINTN delta;
      switch(state->numargs) {
        case 0:
          delta = 1;
          break;
        case 1:
          delta = state->numarg[0];
          break;
        default:
          return 0;
      }
      
      proto->SetCursorPosition(
          proto,
          0,
          MAX(0, proto->Mode->CursorRow - delta));
    } break;
    case L'J': {
      INT32 row = proto->Mode->CursorRow;
      INT32 col = proto->Mode->CursorColumn;

      if(state->numargs == 0)
        state->numarg[0] = 0;
      
      switch(state->numarg[0])
      {
        case 0:          
          for(int i = 0; i < ((rows-row) * cols + cols-col); i++)
            proto->OutputString(proto, L" ");
          break;
        case 1:
          proto->SetCursorPosition(proto, 0, 0);
          for(int i = 0; i < (row * cols + col + 1); i++)
            proto->OutputString(proto, L" ");
          break;
        case 2:
          proto->ClearScreen(proto);
          break;
        default:
          return 0;
      }
      proto->SetCursorPosition(proto, col, row);
    } break;
    case L'K': {
      INT32 row = proto->Mode->CursorRow;
      INT32 col = proto->Mode->CursorColumn;

      if(state->numargs == 0)
        state->numarg[0] = 0;
      
      switch(state->numarg[0])
      {
        case 0:          
          for(int i = 0; i < (cols-col); i++)
            proto->OutputString(proto, L" ");
          break;
        case 1:
          proto->SetCursorPosition(proto, 0, row);
          for(int i = 0; i < (col + 1); i++)
            proto->OutputString(proto, L" ");
          break;
        case 2:
          proto->SetCursorPosition(proto, 0, row);
          for(int i = 0; i < cols; i++)
            proto->OutputString(proto, L" ");
          break;
        default:
          return 0;
      }
      proto->SetCursorPosition(proto, col, row);
    } break;
    case L'm': {
      if(state->numargs == 0) {
        return 0;
      }
      for(int i = 0; i < state->numargs; i++) {
        switch(state->numarg[i]) {
          case 0:
            proto->SetAttribute(proto, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
            break;
          case 1:
            proto->SetAttribute(proto, proto->Mode->Attribute | EFI_BRIGHT);
            break;
          case 2:
          case 22:
            proto->SetAttribute(proto, proto->Mode->Attribute & (~EFI_BRIGHT));
            break;
          case 7:
          case 27:
            proto->SetAttribute(proto, (~proto->Mode->Attribute) & 0x7f);
            break;
          case 30:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x78) | EFI_BLACK);
            break;
          case 100:
          case 40:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x0f) | (EFI_BLACK<<4));
            break;
          case 31:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x78) | EFI_RED);
            break;
          case 101:
          case 41:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x0f) | (EFI_RED<<4));
            break;
          case 32:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x78) | EFI_GREEN);
            break;
          case 102:            
          case 42:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x0f) | (EFI_GREEN<<4));
            break;
          case 33:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x78) | EFI_YELLOW);
            break;
          case 103:            
          case 43:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x0f) | (EFI_BROWN<<4));
            break;
          case 34:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x78) | EFI_BLUE);
            break;
          case 104:
          case 44:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x0f) | (EFI_BLUE<<4));
            break;
          case 35:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x78) | EFI_MAGENTA);
            break;
          case 105:            
          case 45:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x0f) | (EFI_MAGENTA<<4));
            break;
          case 36:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x78) | EFI_CYAN);
            break;
          case 106:            
          case 46:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x0f) | (EFI_CYAN<<4));
            break;
          case 37:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x78) | EFI_WHITE);
            break;
          case 107:            
          case 47:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x0f) | ((EFI_RED | EFI_BLUE| EFI_GREEN)<<4));
            break;
          case 39:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x70) | EFI_LIGHTGRAY);
            break;
          case 49:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x0f) | (EFI_BLACK<<4));
            break;            
          case 90:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x70) | EFI_BRIGHT | EFI_BLACK);
            break;            
          case 91:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x70) | EFI_BRIGHT | EFI_RED);
            break;            
          case 92:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x70) | EFI_BRIGHT | EFI_GREEN);
            break;            
          case 93:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x70) | EFI_BRIGHT | EFI_YELLOW);
            break;            
          case 94:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x70) | EFI_BRIGHT | EFI_BLUE);
            break;            
          case 95:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x70) | EFI_BRIGHT | EFI_MAGENTA);
            break;            
          case 96:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x70) | EFI_BRIGHT | EFI_CYAN);
            break;            
          case 97:
            proto->SetAttribute(
                proto,
                (proto->Mode->Attribute & 0x70) | EFI_BRIGHT | EFI_WHITE);
            break;            
        }
      }
    } break;
    case L'l':
    case L'h': {
      switch(state->cmd_prefix) {
        case L'=': /* no support for switching mode yet */
          break;
        case L'?':
          if(state->numargs == 1 && state->numarg[0] == 25) {
            proto->EnableCursor(proto, (state->cmd == L'h'));            
          }
          break;
      }
    } break;
    case L'n':
      if(state->numargs == 1 && state->numarg[0] == 6 && iio != NULL) {
        iio->cpr_row = proto->Mode->CursorRow;
        iio->cpr_column = proto->Mode->CursorColumn;
        iio->cpr_state = CPR_START; 
      } else {
        return 0;
      }
      break;
  }

  return 1;
}


size_t
da_ConCtrlChar(CtrlCharState *state,
               CHAR16 *buffer,
               EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *proto,
               cIIO *iio)
{

  CHAR16 *cur = buffer;
  int run_flag = 1;
  
  while(run_flag) {
    switch(state->state) {
      case CTRLCHAR_INIT: {
        state->state = CTRLCHAR_START;
        if(*cur == 0) {
          run_flag = 0;
        }
      } break;        
      case CTRLCHAR_START:        
        if(*cur == 0) {
          run_flag = 0;          
        } else if(*cur == L'[') {
          state->state = CTRLCHAR_PARSE;
          cur += 1;
        } else {
          state->state = CTRLCHAR_ERROR;
        }              
        break;
      case CTRLCHAR_PARSE:
        if(*cur == 0) {
          run_flag = 0;
        } else if(*cur >= L'0' && *cur <= L'9') {
          state->state = CTRLCHAR_NUMARG;
          state->digit_count = 0;
          state->numarg[state->numargs] = 0;
        } else {
          switch(*cur) {
            case L'?':
            case L'=':
              state->cmd_prefix = *cur;
              cur += 1;
              break;
            default:
              state->state = CTRLCHAR_CMD;
              break;
          }
        }
        break;
      case CTRLCHAR_NUMARG:
        if(*cur == 0) {
          run_flag = 0;
        }else if(*cur >= L'0' && *cur <= L'9') {
          if(state->digit_count < 4) {
            state->numarg[state->numargs] *= 10;
            state->numarg[state->numargs] += *cur - L'0';
            cur += 1;
            state->digit_count += 1;
          } else {
            state->state = CTRLCHAR_ERROR;
          }
        } else if(*cur == L';') {
          state->state = CTRLCHAR_NUMARG;
          cur += 1;
          state->digit_count = 0;
          state->numargs += 1;          
          if(state->numargs >= MAX_NUMARGS) {
            state->state = CTRLCHAR_ERROR;
          } else {
            state->numarg[state->numargs] = 0;
          }
        } else {
          state->state = CTRLCHAR_CMD;
          state->numargs += 1;
          if(state->numargs < MAX_NUMARGS) {
            state->numarg[state->numargs] = 0;
          }
        }
        break;
      case CTRLCHAR_CMD: {        
        CHAR16 *is_valid = wcschr( L"HABCDEFGsuJKmlhfn", *cur );
        if(is_valid) {
          state->cmd = *cur;
          state->state = CTRLCHAR_END;
          cur += 1;
        } else if(*cur == 0) {
          run_flag = 0;
        } else {
          state->state = CTRLCHAR_ERROR;
        }
      } break;
      case CTRLCHAR_ERROR:
        run_flag = 0;
        cur = buffer;
        break;        
      case CTRLCHAR_END:
        run_flag = 0;
        break;        
    }
  }

  if(state->state == CTRLCHAR_END) {
    if(!da_ConCtrlExec(proto, state, iio)) {
      cur = buffer;
    }
  }
  return cur - buffer;
}

#define TMP_USTR_SIZE 20
static CHAR16 tmp_str[TMP_USTR_SIZE];

size_t
da_ConKeyConvert(cFIFO *inbuf, CHAR16 *buffer, size_t buffer_size)
{
  CHAR16 *cur = buffer;
  CHAR16 *esc = NULL;

  while((size_t)(cur - buffer) < buffer_size) {
    CHAR16 c = 0;
    size_t chars_read = 0;

    if(esc != NULL) {
      if(*esc == 0) {
        esc = NULL;
      } else {
        *cur++ = *esc++;
        continue;
      }
    }
    
    chars_read = inbuf->Read(inbuf, &c, 1);
    if(chars_read == 0) {
      break;
    }

    if((c & 0xf000) == 0xe000) {
      INT16 row = (c >> 6) & 0x3f;
      INT16 column = c & 0x3f;
      swprintf(tmp_str, TMP_USTR_SIZE, L"\033[%d;%dR", column, row);
      esc = tmp_str;
      continue;
    }
    
    switch(c) {
      case TtyKeyEject:
        break;
      case TtyRecovery:
        break;
      case TtyToggleDisplay:
        break;
      case TtyHibernate:
        break;        
      case TtySuspend:
        break;
      case TtyBrightnessDown:
        break;
      case TtyBrightnessUp:
        break;        
      case TtyVolumeDown:
        break;
      case TtyVolumeUp:
        break;
      case TtyMute:
        break;        
      case TtyF24:
        esc = L"\x1b[24;2~";
        break;        
      case TtyF23:
        esc = L"\x1b[23;2~";
        break;
      case TtyF22:
        esc = L"\x1b[21;2~";
        break;
      case TtyF21:
        esc = L"\x1b[20;2~";
        break;
      case TtyF20:
        esc = L"\x1b[19;2~";
        break;        
      case TtyF19:
        esc = L"\x1b[18;2~";
        break;
      case TtyF18:
        esc = L"\x1b[17;2~";
        break;
      case TtyF17:
        esc = L"\x1b[15;2~";
        break;
      case TtyF16:
        esc = L"\x1b[1;2S";
        break;        
      case TtyF15:
        break;
      case TtyF14:
        esc = L"\x1b[1;2Q";
        break;
      case TtyF13:
        esc = L"\x1b[1;2P";
        break;        
      case TtyEscape:
        esc = L"\x1b";
        break;        
      case TtyF12:
        esc = L"\x1b[24~";
        break;
      case TtyF11:
        esc = L"\x1b[23~";
        break;
      case TtyF10:
        esc = L"\x1b[21~";
        break;
      case TtyF9:
        esc = L"\x1b[20~";
        break;
      case TtyF8:
        esc = L"\x1b[19~";
        break;
      case TtyF7:
        esc = L"\x1b[18~";
        break;
      case TtyF6:
        esc = L"\x1b[17~";
        break;
      case TtyF5:
        esc = L"\x1b[15~";
        break;        
      case TtyF4:
        esc = L"\x1b[14~";
        break;
      case TtyF3:
        esc = L"\x1b[13~";
        break;
      case TtyF2:
        esc = L"\x1b[12~";
        break;
      case TtyF1:
        esc = L"\x1b[11~";
        break;        
      case TtyPageDown:
        esc = L"\x1b[6~";
        break;
      case TtyPageUp:
        esc = L"\x1b[5~";
        break;
      case TtyDelete:
        esc = L"\x1b[3~";
        break;
      case TtyInsert:
        esc = L"\x1b[2~";
        break;        
      case TtyEnd:
        esc = L"\x1b[8~";
        break;
      case TtyHome:
        esc = L"\x1b[7~";
        break;
      case TtyLeftArrow:
        esc = L"\x1b[D";
        break;
      case TtyRightArrow:
        esc = L"\x1b[C";
        break;        
      case TtyDownArrow:
        esc = L"\x1b[B";
        break;
      case TtyUpArrow:
        esc = L"\x1b[A";
        break;
    }

    
    if(esc == NULL) {
      *cur++ = c;
    } 
  }

  return cur - buffer;
}
