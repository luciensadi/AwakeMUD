/******************************************************************************
 Protocol snippet by KaVir.  Released into the public domain in February 2011.

 In case this is not legally possible:

 The copyright holder grants any entity the right to use this work for any
 purpose, without any conditions, unless such conditions are required by law.
 ******************************************************************************/

/******************************************************************************
 This snippet was originally designed to be codebase independent, but has been
 modified slightly so that it runs out-of-the-box on Merc derivatives.  To use
 it for other codebases, just change the code in the "Diku/Merc" section below.
 ******************************************************************************/

/******************************************************************************
 Header files.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/telnet.h>
#include <time.h>
#include <ctype.h>

#include "utils.hpp"
#include "awake.hpp"
#include "structs.hpp"
#include "constants.hpp"
#include "comm.hpp"
#include "protocol.hpp"
#include "config.hpp"
#include "nlohmann/json.hpp"
#include "db.hpp"

using nlohmann::json;

/******************************************************************************
 The following section is for Diku/Merc derivatives.  Replace as needed.
 ******************************************************************************/

#define PROTO_DEBUG_MSG(...) log_vfprintf(__VA_ARGS__);
// #define PROTO_DEBUG_MSG(...)

static void Write( descriptor_t *apDescriptor, const char *apData )
{
  if ( apDescriptor != NULL ) {
   if ( apDescriptor->pProtocol->WriteOOB > 0 ) {
    apDescriptor->pProtocol->WriteOOB = 2;
   }
   // Append our output to the player's output buffer.
   write_to_output( apData, apDescriptor );

   // Writing directly to the descriptor can break control sequences mid-stream, causing display problems.
   // write_to_descriptor( apDescriptor->descriptor, apData );
  }
}

static void ReportBug( const char *apText )
{
  static char bugbuf[1000];
  snprintf(bugbuf, sizeof(bugbuf), "SYSERR: protocol.c bug: %s", apText);
  mudlog(bugbuf, NULL, LOG_SYSLOG, TRUE);
}

static void InfoMessage( descriptor_t *apDescriptor, const char *apData )
{
  Write( apDescriptor, "\t[F210][\toINFO\t[F210]]\tn " );
  Write( apDescriptor, apData );
}

static void CompressStart( descriptor_t *apDescriptor )
{
  /* If your mud uses MCCP (Mud Client Compression Protocol), you need to
   * call whatever function normally starts compression from here - the
   * ReportBug() call should then be deleted.
   *
   * Otherwise you can just ignore this function.
   */
  ReportBug( "CompressStart() in protocol.c is being called, but it doesn't do anything!\n" );
}

static void CompressEnd( descriptor_t *apDescriptor )
{
  /* If your mud uses MCCP (Mud Client Compression Protocol), you need to
   * call whatever function normally starts compression from here - the
   * ReportBug() call should then be deleted.
   *
   * Otherwise you can just ignore this function.
   */
  ReportBug( "CompressEnd() in protocol.c is being called, but it doesn't do anything!\n" );
}

/******************************************************************************
 MSDP file-scope variables.
 ******************************************************************************/

/* These are for the GUI_VARIABLES, my unofficial extension of MSDP.  They're
 * intended for clients that wish to offer a generic GUI - not as nice as a
 * custom GUI, admittedly, but still better than a plain terminal window.
 *
 * These are per-player so that they can be customised for different characters
 * (eg changing 'mana' to 'blood' for vampires).  You could even allow players
 * to customise the buttons and gauges themselves if you wish.
 */
static const char s_Button1[] = "\005\002Help\002help\006";
static const char s_Button2[] = "\005\002Look\002look\006";
static const char s_Button3[] = "\005\002Score\002help\006";
static const char s_Button4[] = "\005\002Equipment\002equipment\006";
static const char s_Button5[] = "\005\002Inventory\002inventory\006";

static const char s_Gauge1[]  = "\005\002Health\002red\002HEALTH\002HEALTH_MAX\006";
static const char s_Gauge2[]  = "\005\002Mana\002blue\002MANA\002MANA_MAX\006";
static const char s_Gauge3[]  = "\005\002Movement\002green\002MOVEMENT\002MOVEMENT_MAX\006";
static const char s_Gauge4[]  = "\005\002Exp TNL\002yellow\002EXPERIENCE\002EXPERIENCE_MAX\006";
static const char s_Gauge5[]  = "\005\002Opponent\002darkred\002OPPONENT_HEALTH\002OPPONENT_HEALTH_MAX\006";

/******************************************************************************
 MSDP variable table.
 ******************************************************************************/

/* Macros for readability, but you can remove them if you don't like them */
#define NUMBER_READ_ONLY        FALSE, FALSE, FALSE, FALSE, -1, -1,  0, NULL
#define NUMBER_READ_ONLY_SET_TO(x) FALSE, FALSE, FALSE, FALSE, -1, -1,  x, NULL
#define STRING_READ_ONLY        TRUE,  FALSE, FALSE, FALSE, -1, -1,  0, NULL
#define NUMBER_IN_THE_RANGE(x,y)  FALSE, TRUE,  FALSE, FALSE,  x,  y,  0, NULL
#define BOOLEAN_SET_TO(x)       FALSE, TRUE,  FALSE, FALSE,  0,  1,  x, NULL
#define STRING_WITH_LENGTH_OF(x,y) TRUE,  TRUE,  FALSE, FALSE,  x,  y,  0, NULL
#define STRING_WRITE_ONCE(x,y)    TRUE,  TRUE,  TRUE,  FALSE, -1, -1,  0, NULL
#define STRING_GUI(x)          TRUE,  FALSE, FALSE, TRUE,  -1, -1,  0, x

static variable_name_t VariableNameTable[eMSDP_MAX+1] =
{
  /* General */
  { eMSDP_CHARACTER_NAME,  "CHARACTER_NAME",  STRING_READ_ONLY },
  { eMSDP_SERVER_ID,      "SERVER_ID",      STRING_READ_ONLY },
  { eMSDP_SERVER_TIME,    "SERVER_TIME",    NUMBER_READ_ONLY },
  { eMSDP_SNIPPET_VERSION,  "SNIPPET_VERSION",  NUMBER_READ_ONLY_SET_TO(SNIPPET_VERSION) },

  /* Character */
  { eMSDP_AFFECTS,       "AFFECTS",       STRING_READ_ONLY },
  { eMSDP_ALIGNMENT,      "ALIGNMENT",      NUMBER_READ_ONLY },
  { eMSDP_EXPERIENCE,     "EXPERIENCE",     NUMBER_READ_ONLY },
  { eMSDP_EXPERIENCE_MAX,  "EXPERIENCE_MAX",  NUMBER_READ_ONLY },
  { eMSDP_EXPERIENCE_TNL,  "EXPERIENCE_TNL",  NUMBER_READ_ONLY },
  { eMSDP_HEALTH,        "HEALTH",        NUMBER_READ_ONLY },
  { eMSDP_HEALTH_MAX,     "HEALTH_MAX",     NUMBER_READ_ONLY },
  { eMSDP_LEVEL,        "LEVEL",        NUMBER_READ_ONLY },
  { eMSDP_RACE,         "RACE",         STRING_READ_ONLY },
  { eMSDP_CLASS,        "CLASS",        STRING_READ_ONLY },
  { eMSDP_MANA,         "MANA",         NUMBER_READ_ONLY },
  { eMSDP_MANA_MAX,      "MANA_MAX",      NUMBER_READ_ONLY },
  { eMSDP_WIMPY,        "WIMPY",        NUMBER_READ_ONLY },
  { eMSDP_PRACTICE,      "PRACTICE",      NUMBER_READ_ONLY },
  { eMSDP_MONEY,        "MONEY",        NUMBER_READ_ONLY },
  { eMSDP_MOVEMENT,      "MOVEMENT",      NUMBER_READ_ONLY },
  { eMSDP_MOVEMENT_MAX,    "MOVEMENT_MAX",    NUMBER_READ_ONLY },
  { eMSDP_HITROLL,       "HITROLL",       NUMBER_READ_ONLY },
  { eMSDP_DAMROLL,       "DAMROLL",       NUMBER_READ_ONLY },
  { eMSDP_AC,          "AC",          NUMBER_READ_ONLY },
  { eMSDP_STR,          "STR",          NUMBER_READ_ONLY },
  { eMSDP_INT,          "INT",          NUMBER_READ_ONLY },
  { eMSDP_WIS,          "WIS",          NUMBER_READ_ONLY },
  { eMSDP_DEX,          "DEX",          NUMBER_READ_ONLY },
  { eMSDP_CON,          "CON",          NUMBER_READ_ONLY },
  { eMSDP_STR_PERM,      "STR_PERM",      NUMBER_READ_ONLY },
  { eMSDP_INT_PERM,      "INT_PERM",      NUMBER_READ_ONLY },
  { eMSDP_WIS_PERM,      "WIS_PERM",      NUMBER_READ_ONLY },
  { eMSDP_DEX_PERM,      "DEX_PERM",      NUMBER_READ_ONLY },
  { eMSDP_CON_PERM,      "CON_PERM",      NUMBER_READ_ONLY },

  /* Combat */
  { eMSDP_OPPONENT_HEALTH,  "OPPONENT_HEALTH",  NUMBER_READ_ONLY },
  { eMSDP_OPPONENT_HEALTH_MAX,"OPPONENT_HEALTH_MAX",NUMBER_READ_ONLY },
  { eMSDP_OPPONENT_LEVEL,  "OPPONENT_LEVEL",  NUMBER_READ_ONLY },
  { eMSDP_OPPONENT_NAME,   "OPPONENT_NAME",   STRING_READ_ONLY },

  /* World */
  { eMSDP_AREA_NAME,      "AREA_NAME",      STRING_READ_ONLY },
  { eMSDP_ROOM_EXITS,     "ROOM_EXITS",     STRING_READ_ONLY },
  { eMSDP_ROOM_NAME,      "ROOM_NAME",      STRING_READ_ONLY },
  { eMSDP_ROOM_VNUM,      "ROOM_VNUM",      NUMBER_READ_ONLY },
  { eMSDP_WORLD_TIME,     "WORLD_TIME",     NUMBER_READ_ONLY },

  /* Configurable variables */
  { eMSDP_CLIENT_ID,      "CLIENT_ID",      STRING_WRITE_ONCE(1,40) },
  { eMSDP_CLIENT_VERSION,  "CLIENT_VERSION",  STRING_WRITE_ONCE(1,40) },
  { eMSDP_PLUGIN_ID,      "PLUGIN_ID",      STRING_WITH_LENGTH_OF(1,40) },
  { eMSDP_ANSI_COLORS,    "ANSI_COLORS",    BOOLEAN_SET_TO(1) },
  { eMSDP_XTERM_256_COLORS, "XTERM_256_COLORS", BOOLEAN_SET_TO(0) },
  { eMSDP_UTF_8,        "UTF_8",        BOOLEAN_SET_TO(0) },
  { eMSDP_SOUND,        "SOUND",        BOOLEAN_SET_TO(0) },
  { eMSDP_MXP,          "MXP",          BOOLEAN_SET_TO(0) },

  /* GUI variables */
  { eMSDP_BUTTON_1,      "BUTTON_1",      STRING_GUI(s_Button1) },
  { eMSDP_BUTTON_2,      "BUTTON_2",      STRING_GUI(s_Button2) },
  { eMSDP_BUTTON_3,      "BUTTON_3",      STRING_GUI(s_Button3) },
  { eMSDP_BUTTON_4,      "BUTTON_4",      STRING_GUI(s_Button4) },
  { eMSDP_BUTTON_5,      "BUTTON_5",      STRING_GUI(s_Button5) },
  { eMSDP_GAUGE_1,       "GAUGE_1",       STRING_GUI(s_Gauge1) },
  { eMSDP_GAUGE_2,       "GAUGE_2",       STRING_GUI(s_Gauge2) },
  { eMSDP_GAUGE_3,       "GAUGE_3",       STRING_GUI(s_Gauge3) },
  { eMSDP_GAUGE_4,       "GAUGE_4",       STRING_GUI(s_Gauge4) },
  { eMSDP_GAUGE_5,       "GAUGE_5",       STRING_GUI(s_Gauge5) },

  { eMSDP_MAX,          "",            0,0,0,0,0,0,0,0} /* This must always be last. */
};

/******************************************************************************
 MSSP file-scope variables.
 ******************************************************************************/

static int   s_Players = 0;
static time_t s_Uptime  = 0;

/******************************************************************************
 Local function prototypes.
 ******************************************************************************/

static void Negotiate          ( descriptor_t *apDescriptor );
static void PerformHandshake      ( descriptor_t *apDescriptor, char aCmd, char aProtocol );
static void PerformSubnegotiation  ( descriptor_t *apDescriptor, char aCmd, char *apData, size_t aSize );
static void SendNegotiationSequence ( descriptor_t *apDescriptor, char aCmd, char aProtocol );
static bool ConfirmNegotiation   ( descriptor_t *apDescriptor, negotiated_t aProtocol, bool abWillDo, bool abSendReply );

static void ParseMSDP          ( descriptor_t *apDescriptor, const char *apData );
static void ExecuteMSDPPair      ( descriptor_t *apDescriptor, const char *apVariable, const char *apValue );

static void ParseATCP          ( descriptor_t *apDescriptor, const char *apData );
#ifdef MUDLET_PACKAGE
static void SendATCP           ( descriptor_t *apDescriptor, const char *apVariable, const char *apValue );
#endif /* MUDLET_PACKAGE */

static void SendMSSP           ( descriptor_t *apDescriptor );

static char *GetMxpTag          ( const char *apTag, const char *apText );

static const char *GetAnsiColour   ( bool abBackground, int aRed, int aGreen, int aBlue );
static const char *GetRGBColour    ( bool abBackground, int aRed, int aGreen, int aBlue );
static bool IsValidColour      ( const char *apArgument );

static bool MatchString        ( const char *apFirst, const char *apSecond );
static bool PrefixString       ( const char *apPart, const char *apWhole );
static bool IsNumber          ( const char *apString );
static char  *AllocString        ( const char *apString );

/******************************************************************************
 ANSI colour codes.
 ******************************************************************************/

static const char s_Clean     [] = "\033[0;00m"; /* Remove colour */

static const char s_DarkBlack  [] = "\033[0;30m"; /* Black foreground */
static const char s_DarkRed    [] = "\033[0;31m"; /* Red foreground */
static const char s_DarkGreen  [] = "\033[0;32m"; /* Green foreground */
static const char s_DarkYellow  [] = "\033[0;33m"; /* Yellow foreground */
static const char s_DarkBlue   [] = "\033[0;34m"; /* Blue foreground */
static const char s_DarkMagenta [] = "\033[0;35m"; /* Magenta foreground */
static const char s_DarkCyan   [] = "\033[0;36m"; /* Cyan foreground */
static const char s_DarkWhite  [] = "\033[0;37m"; /* White foreground */

static const char s_BoldBlack  [] = "\033[1;30m"; /* Grey foreground */
static const char s_BoldRed    [] = "\033[1;31m"; /* Bright red foreground */
static const char s_BoldGreen  [] = "\033[1;32m"; /* Bright green foreground */
static const char s_BoldYellow  [] = "\033[1;33m"; /* Bright yellow foreground */
static const char s_BoldBlue   [] = "\033[1;34m"; /* Bright blue foreground */
static const char s_BoldMagenta [] = "\033[1;35m"; /* Bright magenta foreground */
static const char s_BoldCyan   [] = "\033[1;36m"; /* Bright cyan foreground */
static const char s_BoldWhite  [] = "\033[1;37m"; /* Bright white foreground */

static const char s_BackBlack  [] = "\033[1;40m"; /* Black background */
static const char s_BackRed    [] = "\033[1;41m"; /* Red background */
static const char s_BackGreen  [] = "\033[1;42m"; /* Green background */
static const char s_BackYellow  [] = "\033[1;43m"; /* Yellow background */
static const char s_BackBlue   [] = "\033[1;44m"; /* Blue background */
static const char s_BackMagenta [] = "\033[1;45m"; /* Magenta background */
static const char s_BackCyan   [] = "\033[1;46m"; /* Cyan background */
static const char s_BackWhite  [] = "\033[1;47m"; /* White background */

/******************************************************************************
 Protocol global functions.
 ******************************************************************************/

protocol_t *ProtocolCreate( void )
{
  int i; /* Loop counter */
  protocol_t *pProtocol;

  /* Called the first time we enter - make sure the table is correct */
  static bool bInit = FALSE;
  if ( !bInit )
  {
    bInit = TRUE;
    for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
    {
      if ( VariableNameTable[i].Variable != i )
      {
        ReportBug( "MSDP: Variable table does not match the enums in the header.\n" );
        break;
      }
    }
  }

  pProtocol = new protocol_t;
  memset(pProtocol, 0, sizeof(protocol_t));
  pProtocol->WriteOOB = 0;
  for ( i = eNEGOTIATED_TTYPE; i < eNEGOTIATED_MAX; ++i )
    pProtocol->Negotiated[i] = FALSE;
  pProtocol->bIACMode = FALSE;
  pProtocol->bNegotiated = FALSE;
  pProtocol->bRenegotiate = FALSE;
  pProtocol->bNeedMXPVersion = FALSE;
  pProtocol->bBlockMXP = FALSE;
  pProtocol->bTTYPE = FALSE;
  pProtocol->bECHO = FALSE;
  pProtocol->bNAWS = FALSE;
  pProtocol->bCHARSET = FALSE;
  pProtocol->bMSDP = FALSE;
  pProtocol->bMSSP = FALSE;
  pProtocol->bATCP = FALSE;
  pProtocol->bMSP = FALSE;
  pProtocol->bMXP = FALSE;
  pProtocol->bMCCP = FALSE;
  pProtocol->b256Support = eUNKNOWN;
  pProtocol->ScreenWidth = 0;
  pProtocol->ScreenHeight = 0;
  pProtocol->pMXPVersion = AllocString("Unknown");
  pProtocol->pLastTTYPE = NULL;
#ifdef USE_DEBUG_CANARIES
  pProtocol->canary = 31337;
#endif
  pProtocol->do_coerce_ansi_capable_colors_to_ansi = FALSE;
  pProtocol->pVariables = new MSDP_t*[eMSDP_MAX];

  for ( i = 0; i < eMSDP_MAX; ++i )
  {
    pProtocol->pVariables[i] = new MSDP_t;
    pProtocol->pVariables[i]->bReport = FALSE;
    pProtocol->pVariables[i]->bDirty = FALSE;
    pProtocol->pVariables[i]->ValueInt = 0;
    pProtocol->pVariables[i]->pValueString = NULL;

    if ( VariableNameTable[i].bString )
    {
      if ( VariableNameTable[i].pDefault != NULL )
        pProtocol->pVariables[i]->pValueString = AllocString(VariableNameTable[i].pDefault);
      else if ( VariableNameTable[i].bConfigurable )
        pProtocol->pVariables[i]->pValueString = AllocString("Unknown");
      else /* Use an empty string */
        pProtocol->pVariables[i]->pValueString = AllocString("");
    }
    else if ( VariableNameTable[i].Default != 0 )
    {
      pProtocol->pVariables[i]->ValueInt = VariableNameTable[i].Default;
    }
  }

  return pProtocol;
}

void ProtocolDestroy( protocol_t *apProtocol )
{
  int i; /* Loop counter */

  for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
  {
    delete [] apProtocol->pVariables[i]->pValueString;
    delete apProtocol->pVariables[i];
  }

  delete [] apProtocol->pVariables;
  delete [] apProtocol->pLastTTYPE;
  delete [] apProtocol->pMXPVersion;
  delete apProtocol;
}

void ProtocolInput( descriptor_t *apDescriptor, char *apData, int aSize, char *apOut, size_t apOutSize )
{
  static char CmdBuf[MAX_PROTOCOL_BUFFER+1];
  static char IacBuf[MAX_PROTOCOL_BUFFER+1];
  int CmdIndex = 0;
  int IacIndex = 0;
  int Index;

  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  for ( Index = 0; Index < aSize; ++Index )
  {
    /* If we'd overflow the buffer, we just ignore the input */
    if ( CmdIndex >= MAX_PROTOCOL_BUFFER || IacIndex >= MAX_PROTOCOL_BUFFER )
    {
      ReportBug("ProtocolInput: Too much incoming data to store in the buffer.\n");
      return;
    }

    /* IAC IAC is treated as a single value of 255 */
    if ( apData[Index] == (char)IAC && apData[Index+1] == (char)IAC )
    {
      if ( pProtocol->bIACMode )
        IacBuf[IacIndex++] = (char)IAC;
      else /* In-band command */
        CmdBuf[CmdIndex++] = (char)IAC;
      Index++;
    }
    else if ( pProtocol->bIACMode )
    {
      /* End subnegotiation. */
      if ( apData[Index] == (char)IAC && apData[Index+1] == (char)SE )
      {
        Index++;
        pProtocol->bIACMode = FALSE;
        IacBuf[IacIndex] = '\0';
        if ( IacIndex >= 2 )
          PerformSubnegotiation( apDescriptor, IacBuf[0], &IacBuf[1], IacIndex-1 );
        IacIndex = 0;
      }
      else
        IacBuf[IacIndex++] = apData[Index];
    }
    else if ( apData[Index] == (char)27 && apData[Index+1] == '[' &&
      isdigit(apData[Index+2]) && apData[Index+3] == 'z' )
    {
      char MXPBuffer [1024];
      char *pMXPTag = NULL;
      int i = 0; /* Loop counter */

      Index += 4; /* Skip to the start of the MXP sequence. */

      while ( Index < aSize && apData[Index] != '>' && i < 1000 )
      {
        MXPBuffer[i++] = apData[Index++];
      }
      MXPBuffer[i++] = '>';
      MXPBuffer[i] = '\0';

      if ( ( pMXPTag = GetMxpTag( "CLIENT=", MXPBuffer ) ) != NULL )
      {
        /* Overwrite the previous client name - this is harder to fake */
        delete [] pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString;
        pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pMXPTag);
      }

      if ( ( pMXPTag = GetMxpTag( "VERSION=", MXPBuffer ) ) != NULL )
      {
        const char *pClientName = pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString;

        delete [] pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString;
        pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString = AllocString(pMXPTag);

        if ( MatchString( "MUSHCLIENT", pClientName ) )
        {
          /* MUSHclient 4.02 and later supports 256 colours. */
          if ( strcmp(pMXPTag, "4.02") >= 0 )
          {
            pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
            pProtocol->b256Support = eYES;
          }
          else /* We know for sure that 256 colours are not supported */
            pProtocol->b256Support = eNO;
        }
        else if ( MatchString( "CMUD", pClientName ) )
        {
          /* CMUD 3.04 and later supports 256 colours. */
          if ( strcmp(pMXPTag, "3.04") >= 0 )
          {
            pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
            pProtocol->b256Support = eYES;
          }
          else /* We know for sure that 256 colours are not supported */
            pProtocol->b256Support = eNO;
        }
        else if ( MatchString( "ATLANTIS", pClientName ) )
        {
          /* Atlantis 0.9.9.0 supports XTerm 256 colours, but it doesn't
           * yet have MXP.  However MXP is planned, so once it responds
           * to a <VERSION> tag we'll know we can use 256 colours.
           */
          pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
          pProtocol->b256Support = eYES;
        }
      }

      if ( ( pMXPTag = GetMxpTag( "MXP=", MXPBuffer ) ) != NULL )
      {
        delete [] pProtocol->pMXPVersion;
        pProtocol->pMXPVersion = AllocString(pMXPTag);
      }

      if ( strcmp(pProtocol->pMXPVersion, "Unknown") )
      {
        Write( apDescriptor, "\n" );
        snprintf( MXPBuffer, sizeof(MXPBuffer), "MXP version %s detected and enabled.\r\n",
          pProtocol->pMXPVersion );
        InfoMessage( apDescriptor, MXPBuffer );
      }
    }
    else /* In-band command */
    {
      if ( apData[Index] == (char)IAC )
      {
        switch ( apData[Index+1] )
        {
          case (char)SB: /* Begin subnegotiation. */
            Index++;
            pProtocol->bIACMode = TRUE;
            break;

          case (char)DO: /* Handshake. */
          case (char)DONT:
          case (char)WILL:
          case (char)WONT:
            PerformHandshake( apDescriptor, apData[Index+1], apData[Index+2] );
            Index += 2;
            break;

          case (char)IAC: /* Two IACs count as one. */
            CmdBuf[CmdIndex++] = (char)IAC;
            Index++;
            break;

          default: /* Skip it. */
            Index++;
            break;
        }
      }
      else
        CmdBuf[CmdIndex++] = apData[Index];
    }
  }

  /* Terminate the two buffers */
  IacBuf[IacIndex] = '\0';
  CmdBuf[CmdIndex] = '\0';

  /* Copy the input buffer back to the player. */
  strlcat( apOut, CmdBuf, apOutSize );
}

const char *ProtocolOutput( descriptor_t *apDescriptor, const char *apData, int *apLength, bool appendGA )
{
  static char Result[MAX_OUTPUT_BUFFER+1];
  const char Tab[] = "\t";
  const char MSP[] = "!!";
  const char MXPStart[] = "\033[1z<";
  const char MXPStop[] = ">\033[7z";
  const char LinkStart[] = "\033[1z<send>\033[7z";
  const char LinkStop[] = "\033[1z</send>\033[7z";
  char Buffer[8] = {'\0'};
  bool bTerminate = FALSE, bUseMXP = FALSE, bUseMSP = FALSE;
#ifdef COLOUR_CHAR
  bool bColourOn = COLOUR_ON_BY_DEFAULT;
#endif /* COLOUR_CHAR */
  int i = 0, j = 0; /* Index values */

#ifdef USE_DEBUG_CANARIES
  if (apDescriptor && apDescriptor->canary != 31337) {
    mudlog("SYSERR: apDescriptor canary was invalid! Handing back unaltered data.", NULL, LOG_SYSLOG, TRUE);
    return apData;
  }
#endif

  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;
  if ( pProtocol == NULL || apData == NULL)
    return apData;

#ifdef USE_DEBUG_CANARIES
  if (pProtocol->canary != 31337) {
    mudlog("SYSERR: pprotocol canary was invalid! Handing back unaltered data.", NULL, LOG_SYSLOG, TRUE);
    return apData;
  }
#endif

  if (!pProtocol->pVariables)
    return apData;

  /* Strip !!SOUND() triggers if they support MSP or are using sound */
  if ( pProtocol->bMSP || (pProtocol->pVariables[eMSDP_SOUND] && pProtocol->pVariables[eMSDP_SOUND]->ValueInt) )
    bUseMSP = TRUE;

  for ( ; i < MAX_OUTPUT_BUFFER && apData[j] != '\0' && !bTerminate &&
    (*apLength <= 0 || j < *apLength); ++j )
  {
    if ( apData[j] == '\t' )
    {
      const char *pCopyFrom = NULL;

      bool p_is_color = TRUE;
      switch ( apData[++j] )
      {
        case '\t': /* Two tabs in a row will display an actual tab */
          pCopyFrom = Tab;
          p_is_color = FALSE;
          break;
        case 'n':
          pCopyFrom = s_Clean;
          break;
        case 'N':
          pCopyFrom = s_Clean;
          break;
        case 'r': /* dark red */
          pCopyFrom = ColourRGB(apDescriptor, "F300", COERCE_ANSI);
          break;
        case 'R': /* light red */
          pCopyFrom = ColourRGB(apDescriptor, "F500", COERCE_ANSI);
          break;
        case 'g': /* dark green */
          pCopyFrom = ColourRGB(apDescriptor, "F030", COERCE_ANSI);
          break;
        case 'G': /* light green */
          pCopyFrom = ColourRGB(apDescriptor, "F050", COERCE_ANSI);
          break;
        case 'y': /* dark yellow */
          pCopyFrom = ColourRGB(apDescriptor, "F330", COERCE_ANSI);
          break;
        case 'Y': /* light yellow */
          pCopyFrom = ColourRGB(apDescriptor, "F550", COERCE_ANSI);
          break;
        case 'b': /* dark blue */
          pCopyFrom = ColourRGB(apDescriptor, "F025", COERCE_ANSI);
          break;
        case 'B': /* light blue */
          pCopyFrom = ColourRGB(apDescriptor, "F035", COERCE_ANSI);
          break;
        case 'm': /* dark magenta */
          pCopyFrom = ColourRGB(apDescriptor, "F303", COERCE_ANSI);
          break;
        case 'M': /* light magenta */
          pCopyFrom = ColourRGB(apDescriptor, "F505", COERCE_ANSI);
          break;
        case 'c': /* dark cyan */
          pCopyFrom = ColourRGB(apDescriptor, "F033", COERCE_ANSI);
          break;
        case 'C': /* light cyan */
          pCopyFrom = ColourRGB(apDescriptor, "F055", COERCE_ANSI);
          break;
        case 'w': /* dark white */
          pCopyFrom = ColourRGB(apDescriptor, "F333", COERCE_ANSI);
          break;
        case 'W': /* light white */
          pCopyFrom = ColourRGB(apDescriptor, "F555", COERCE_ANSI);
          break;
        case 'a': /* dark azure */
          pCopyFrom = ColourRGB(apDescriptor, "F014", NO_COERCION);
          break;
        case 'A': /* light azure */
          pCopyFrom = ColourRGB(apDescriptor, "F025", NO_COERCION);
          break;
        case 'j': /* dark jade */
          pCopyFrom = ColourRGB(apDescriptor, "F031", NO_COERCION);
          break;
        case 'J': /* light jade */
          pCopyFrom = ColourRGB(apDescriptor, "F052", NO_COERCION);
          break;
        case 'e': /* dark electric lime */
          pCopyFrom = ColourRGB(apDescriptor, "F140", NO_COERCION);
          break;
        case 'E': /* light electric lime */
          pCopyFrom = ColourRGB(apDescriptor, "F250", NO_COERCION);
          break;
        case 'l': /* dark black, AKA you're a dick for using this color when most clients have black backgrounds */
          pCopyFrom = ColourRGB(apDescriptor, "F111", COERCE_ANSI);
          break;
        case 'L': /* grey */
          pCopyFrom = ColourRGB(apDescriptor, "F222", COERCE_ANSI);
          break;
        case 'o': /* dark orange */
          pCopyFrom = ColourRGB(apDescriptor, "F520", NO_COERCION);
          break;
        case 'O': /* light orange */
          pCopyFrom = ColourRGB(apDescriptor, "F530", NO_COERCION);
          break;
        case 'p': /* dark pink */
          pCopyFrom = ColourRGB(apDescriptor, "F301", NO_COERCION);
          break;
        case 'P': /* light pink */
          pCopyFrom = ColourRGB(apDescriptor, "F502", NO_COERCION);
          break;
        case 't': /* dark tan */
          pCopyFrom = ColourRGB(apDescriptor, "F210", NO_COERCION);
          break;
        case 'T': /* light tan */
          pCopyFrom = ColourRGB(apDescriptor, "F321", NO_COERCION);
          break;
        case 'v': /* dark violet */
          pCopyFrom = ColourRGB(apDescriptor, "F104", NO_COERCION);
          break;
        case 'V': /* light violet */
          pCopyFrom = ColourRGB(apDescriptor, "F205", NO_COERCION);
          break;
        case '(': /* MXP link */
          if ( !pProtocol->bBlockMXP && (pProtocol->pVariables[eMSDP_MXP] && pProtocol->pVariables[eMSDP_MXP]->ValueInt) ) {
              pCopyFrom = LinkStart;
              p_is_color = FALSE;
          }
          break;
        case ')': /* MXP link */
          if ( !pProtocol->bBlockMXP && (pProtocol->pVariables[eMSDP_MXP] && pProtocol->pVariables[eMSDP_MXP]->ValueInt) ) {
              pCopyFrom = LinkStop;
              p_is_color = FALSE;
          }
          pProtocol->bBlockMXP = FALSE;
          break;
        case '<':
          if ( !pProtocol->bBlockMXP && (pProtocol->pVariables[eMSDP_MXP] && pProtocol->pVariables[eMSDP_MXP]->ValueInt) )
          {
            pCopyFrom = MXPStart;
            bUseMXP = TRUE;
            p_is_color = FALSE;
          }
          else /* No MXP support, so just strip it out */
          {
            while ( apData[j] != '\0' && apData[j] != '>' )
              ++j;
          }
          pProtocol->bBlockMXP = FALSE;
          break;
        case '[':
          if ( tolower(apData[++j]) == 'u' )
          {
            memset(Buffer, '\0', sizeof(Buffer));
            char BugString[256];
            int Index = 0;
            int Number = 0;
            bool bDone = FALSE, bValid = TRUE;

            while ( isdigit(apData[++j]) )
            {
              Number *= 10;
              Number += (apData[j])-'0';
            }

            if ( apData[j] == '/' )
              ++j;

            while ( apData[j] != '\0' && !bDone )
            {
              if ( apData[j] == ']' )
                bDone = TRUE;
              else if ( Index < 7 )
                Buffer[Index++] = apData[j++];
              else /* It's too long, so ignore the rest and note the problem */
              {
                j++;
                bValid = FALSE;
              }
            }

            if ( !bDone )
            {
              snprintf( BugString, sizeof(BugString), "BUG: Unicode substitute '%s' wasn't terminated with ']'.\n", Buffer );
              ReportBug( BugString );
            }
            else if ( !bValid )
            {
              snprintf( BugString, sizeof(BugString), "BUG: Unicode substitute '%s' truncated.  Missing ']'?\n", Buffer );
              ReportBug( BugString );
            }
            else if ( pProtocol->pVariables[eMSDP_UTF_8] && pProtocol->pVariables[eMSDP_UTF_8]->ValueInt )
            {
              pCopyFrom = UnicodeGet(Number);
              p_is_color = FALSE;
            }
            else /* Display the substitute string */
            {
              pCopyFrom = Buffer;
              p_is_color = FALSE;
            }

            /* Terminate if we've reached the end of the string */
            bTerminate = !bDone;
          }
          else if ( tolower(apData[j]) == 'f' || tolower(apData[j]) == 'b' )
          {
            char Buffer[8] = {'\0'}, BugString[256];
            int Index = 0;
            bool bDone = FALSE, bValid = TRUE;

            /* Copy the 'f' (foreground) or 'b' (background) */
            Buffer[Index++] = apData[j++];

            while ( apData[j] != '\0' && !bDone && bValid )
            {
              if ( apData[j] == ']' )
                bDone = TRUE;
              else if ( Index < 4 )
                Buffer[Index++] = apData[j++];
              else /* It's too long, so drop out - the colour code may still be valid */
                bValid = FALSE;
            }

#ifndef USE_BACKGROUND_COLORS
            if ( tolower(Buffer[0]) == 'b' ) {
              // Background colors have been disabled.
              pCopyFrom = NULL;
              break;
            }
#endif

            if ( !bDone || !bValid )
            {
              snprintf( BugString, sizeof(BugString), "BUG: RGB %sground colour '%s' wasn't terminated with ']'.\n",
                (tolower(Buffer[0]) == 'f') ? "fore" : "back", &Buffer[1] );
              ReportBug( BugString );
            }
            else if ( !IsValidColour(Buffer) )
            {
              snprintf( BugString, sizeof(BugString), "BUG: RGB %sground colour '%s' invalid (each digit must be in the range 0-5).\n",
                (tolower(Buffer[0]) == 'f') ? "fore" : "back", &Buffer[1] );
              ReportBug( BugString );
            }
            else /* Success */
            {
              pCopyFrom = ColourRGB(apDescriptor, Buffer, NO_COERCION);
            }
          }
          else if ( tolower(apData[j]) == 'x' )
          {
            char Buffer[8] = {'\0'}, BugString[256];
            int Index = 0;
            bool bDone = FALSE, bValid = TRUE;

            ++j; /* Skip the 'x' */

            while ( apData[j] != '\0' && !bDone )
            {
              if ( apData[j] == ']' )
                bDone = TRUE;
              else if ( Index < 7 )
                Buffer[Index++] = apData[j++];
              else /* It's too long, so ignore the rest and note the problem */
              {
                j++;
                bValid = FALSE;
              }
            }

            if ( !bDone )
            {
              snprintf( BugString, sizeof(BugString), "BUG: Required MXP version '%s' wasn't terminated with ']'.\n", Buffer );
              ReportBug( BugString );
            }
            else if ( !bValid )
            {
              snprintf( BugString, sizeof(BugString), "BUG: Required MXP version '%s' too long.  Missing ']'?\n", Buffer );
              ReportBug( BugString );
            }
            else if ( !strcmp(pProtocol->pMXPVersion, "Unknown") ||
              strcmp(pProtocol->pMXPVersion, Buffer) < 0 )
            {
              /* Their version of MXP isn't high enough */
              pProtocol->bBlockMXP = TRUE;
            }
            else /* MXP is sufficient for this tag */
            {
              pProtocol->bBlockMXP = FALSE;
            }

            /* Terminate if we've reached the end of the string */
            bTerminate = !bDone;
          }
          break;
        case '!': /* Used for in-band MSP sound triggers */
          pCopyFrom = MSP;
          p_is_color = FALSE;
          break;
#ifdef COLOUR_CHAR
        case '+':
          bColourOn = TRUE;
          break;
        case '-':
          bColourOn = FALSE;
          break;
#endif /* COLOUR_CHAR */
        case '\0':
          bTerminate = TRUE;
          break;
        default:
          break;
      }

      /* Copy the colour code, if any. */
      if ( pCopyFrom != NULL && (!p_is_color || !D_PRF_FLAGGED(apDescriptor, PRF_NOCOLOR)))
      {
        while ( *pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER )
          Result[i++] = *pCopyFrom++;
      }
    }

#ifdef COLOUR_CHAR
    else if ( bColourOn && apData[j] == COLOUR_CHAR )
    {
      const char ColourChar[] = { COLOUR_CHAR, '\0' };
      const char *pCopyFrom = NULL;

      bool p_is_color = TRUE;
      switch ( apData[++j] )
      {
        case COLOUR_CHAR: /* Two in a row display the actual character */
          pCopyFrom = ColourChar;
          p_is_color = FALSE;
          break;
        case 'n':
          pCopyFrom = s_Clean;
          break;
        case 'N':
          pCopyFrom = s_Clean;
          break;
        case 'r': /* dark red */
          pCopyFrom = ColourRGB(apDescriptor, "F300", COERCE_ANSI);
          break;
        case 'R': /* light red */
          pCopyFrom = ColourRGB(apDescriptor, "F500", COERCE_ANSI);
          break;
        case 'g': /* dark green */
          pCopyFrom = ColourRGB(apDescriptor, "F030", COERCE_ANSI);
          break;
        case 'G': /* light green */
          pCopyFrom = ColourRGB(apDescriptor, "F050", COERCE_ANSI);
          break;
        case 'y': /* dark yellow */
          pCopyFrom = ColourRGB(apDescriptor, "F330", COERCE_ANSI);
          break;
        case 'Y': /* light yellow */
          pCopyFrom = ColourRGB(apDescriptor, "F550", COERCE_ANSI);
          break;
        case 'b': /* dark blue */
          pCopyFrom = ColourRGB(apDescriptor, "F025", COERCE_ANSI);
          break;
        case 'B': /* light blue */
          pCopyFrom = ColourRGB(apDescriptor, "F035", COERCE_ANSI);
          break;
        case 'm': /* dark magenta */
          pCopyFrom = ColourRGB(apDescriptor, "F303", COERCE_ANSI);
          break;
        case 'M': /* light magenta */
          pCopyFrom = ColourRGB(apDescriptor, "F505", COERCE_ANSI);
          break;
        case 'c': /* dark cyan */
          pCopyFrom = ColourRGB(apDescriptor, "F033", COERCE_ANSI);
          break;
        case 'C': /* light cyan */
          pCopyFrom = ColourRGB(apDescriptor, "F055", COERCE_ANSI);
          break;
        case 'w': /* dark white */
          pCopyFrom = ColourRGB(apDescriptor, "F333", COERCE_ANSI);
          break;
        case 'W': /* light white */
          pCopyFrom = ColourRGB(apDescriptor, "F555", COERCE_ANSI);
          break;
        case 'a': /* dark azure */
          pCopyFrom = ColourRGB(apDescriptor, "F014", NO_COERCION);
          break;
        case 'A': /* light azure */
          pCopyFrom = ColourRGB(apDescriptor, "F025", NO_COERCION);
          break;
        case 'j': /* dark jade */
          pCopyFrom = ColourRGB(apDescriptor, "F031", NO_COERCION);
          break;
        case 'J': /* light jade */
          pCopyFrom = ColourRGB(apDescriptor, "F052", NO_COERCION);
          break;
        case 'e': /* dark electric lime */
          pCopyFrom = ColourRGB(apDescriptor, "F140", NO_COERCION);
          break;
        case 'E': /* light electric lime */
          pCopyFrom = ColourRGB(apDescriptor, "F250", NO_COERCION);
          break;
        case 'l': /* black, AKA you're a dick for using this color when most clients have black backgrounds */
          pCopyFrom = ColourRGB(apDescriptor, "F111", COERCE_ANSI);
          break;
        case 'L': /* grey */
          pCopyFrom = ColourRGB(apDescriptor, "F222", COERCE_ANSI);
          break;
        case 'o': /* dark orange */
          pCopyFrom = ColourRGB(apDescriptor, "F520", NO_COERCION);
          break;
        case 'O': /* light orange */
          pCopyFrom = ColourRGB(apDescriptor, "F530", NO_COERCION);
          break;
        case 'p': /* dark pink */
          pCopyFrom = ColourRGB(apDescriptor, "F301", NO_COERCION);
          break;
        case 'P': /* light pink */
          pCopyFrom = ColourRGB(apDescriptor, "F502", NO_COERCION);
          break;
        case 't': /* dark tan */
          pCopyFrom = ColourRGB(apDescriptor, "F210", NO_COERCION);
          break;
        case 'T': /* light tan */
          pCopyFrom = ColourRGB(apDescriptor, "F321", NO_COERCION);
          break;
        case 'v': /* dark violet */
          pCopyFrom = ColourRGB(apDescriptor, "F104", NO_COERCION);
          break;
        case 'V': /* light violet */
          pCopyFrom = ColourRGB(apDescriptor, "F205", NO_COERCION);
          break;
        case '\0':
          bTerminate = TRUE;
          break;
#ifdef EXTENDED_COLOUR
        case '[':
          if ( tolower(apData[++j]) == 'f' || tolower(apData[j]) == 'b' )
          {
            char Buffer[8] = {'\0'};
            int Index = 0;
            bool bDone = FALSE, bValid = TRUE;

            /* Copy the 'f' (foreground) or 'b' (background) */
            Buffer[Index++] = apData[j++];

            while ( apData[j] != '\0' && !bDone && bValid )
            {
              if ( apData[j] == ']' )
                bDone = TRUE;
              else if ( Index < 4 )
                Buffer[Index++] = apData[j++];
              else /* It's too long, so drop out - the colour code may still be valid */
                bValid = FALSE;
            }

#ifndef USE_BACKGROUND_COLORS
            if ( tolower(Buffer[0]) == 'b' ) {
              // Background colors have been disabled.
              pCopyFrom = NULL;
              break;
            }
#endif

            if ( bDone && bValid && IsValidColour(Buffer) )
              pCopyFrom = ColourRGB(apDescriptor, Buffer, NO_COERCION);
          }
          break;
#endif /* EXTENDED_COLOUR */
        default:
#ifdef DISPLAY_INVALID_COLOUR_CODES
          Result[i++] = COLOUR_CHAR;
          Result[i++] = apData[j];
#endif /* DISPLAY_INVALID_COLOUR_CODES */
          break;
      }

      /* Copy the colour code, if any. */
      if ( pCopyFrom != NULL && (!p_is_color || !D_PRF_FLAGGED(apDescriptor, PRF_NOCOLOR)))
      {
        while ( *pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER )
          Result[i++] = *pCopyFrom++;
      }
    }
#endif /* COLOUR_CHAR */
    else if ( bUseMXP && apData[j] == '>' )
    {
      const char *pCopyFrom = MXPStop;
      while ( *pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER)
        Result[i++] = *pCopyFrom++;
      bUseMXP = FALSE;
    }
    else if ( bUseMSP && j > 0 && apData[j-1] == '!' && apData[j] == '!' &&
      PrefixString("SOUND(", &apData[j+1]) )
    {
      /* Avoid accidental triggering of old-style MSP triggers */
      Result[i++] = '?';
    }
    else if (apData[j] == '#' && apData[j + 1] == '#')
    {
      if (D_PRF_FLAGGED(apDescriptor, PRF_SCREENREADER))
       Result[i++] = '#';
      j += 1;
    }
    else /* Just copy the character normally */
    {
      Result[i++] = apData[j];
    }
  }

  // Terminate the color. Yes, all the colors.
  if (!D_PRF_FLAGGED(apDescriptor, PRF_NOCOLOR)) {
    const char *pCopyFrom = s_Clean;
    while ( *pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER )
     Result[i++] = *pCopyFrom++;
  }

  /* If we'd overflow the buffer, we don't send any output */
  if ( i >= MAX_OUTPUT_BUFFER )
  {
    i = 0;
    ReportBug("ProtocolOutput: Too much outgoing data to store in the buffer.\n");
  }

#ifdef SEND_TELNET_GA
  /* Append GA to prompts */
  if (appendGA && i + 3 < MAX_OUTPUT_BUFFER) {
    Result[i++] = (char) IAC;
    Result[i++] = (char) TELOPT_GA;
  }
#endif

  /* Terminate the string */
  Result[i] = '\0';

  /* Store the length */
  if ( apLength )
    *apLength = i;

  /* Return the string */
  return Result;
}

/* Some clients (such as GMud) don't properly handle negotiation, and simply
 * display every printable character to the screen.  However TTYPE isn't a
 * printable character, so we negotiate for it first, and only negotiate for
 * other protocols if the client responds with IAC WILL TTYPE or IAC WONT
 * TTYPE.  Thanks go to Donky on MudBytes for the suggestion.
 */
void ProtocolNegotiate( descriptor_t *apDescriptor )
{
  ConfirmNegotiation(apDescriptor, eNEGOTIATED_TTYPE, TRUE, TRUE);
}

/* Tells the client to switch echo on or off. */
void ProtocolNoEcho( descriptor_t *apDescriptor, bool abOn )
{
  ConfirmNegotiation(apDescriptor, eNEGOTIATED_ECHO, abOn, TRUE);
}

/******************************************************************************
 Copyover save/load functions.
 ******************************************************************************/

const char *CopyoverGet( descriptor_t *apDescriptor )
{
  static char Buffer[64];
  char *pBuffer = Buffer;
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol != NULL )
  {
    snprintf(Buffer, sizeof(Buffer), "%d/%d", pProtocol->ScreenWidth, pProtocol->ScreenHeight);

    /* Skip to the end */
    while ( *pBuffer != '\0' )
      ++pBuffer;

    if ( pProtocol->bTTYPE )
      *pBuffer++ = 'T';
    if ( pProtocol->bNAWS )
      *pBuffer++ = 'N';
    if ( pProtocol->bMSDP )
      *pBuffer++ = 'M';
    if ( pProtocol->bATCP )
      *pBuffer++ = 'A';
    if ( pProtocol->bMSP )
      *pBuffer++ = 'S';
    if ( pProtocol->pVariables[eMSDP_MXP] && pProtocol->pVariables[eMSDP_MXP]->ValueInt )
      *pBuffer++ = 'X';
    if ( pProtocol->bMCCP )
    {
      *pBuffer++ = 'c';
      CompressEnd(apDescriptor);
    }
    if ( pProtocol->pVariables[eMSDP_XTERM_256_COLORS] && pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt )
      *pBuffer++ = 'C';
    if ( pProtocol->bCHARSET )
      *pBuffer++ = 'H';
    if ( pProtocol->pVariables[eMSDP_UTF_8] && pProtocol->pVariables[eMSDP_UTF_8]->ValueInt )
      *pBuffer++ = 'U';
  }

  /* Terminate the string */
  *pBuffer = '\0';

  return Buffer;
}

void CopyoverSet( descriptor_t *apDescriptor, const char *apData )
{
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol != NULL && apData != NULL )
  {
    int Width = 0, Height = 0;
    bool bDoneWidth = FALSE;
    int i; /* Loop counter */

    for ( i = 0; apData[i] != '\0'; ++i )
    {
      switch ( apData[i] )
      {
        case 'T':
          pProtocol->bTTYPE = TRUE;
          break;
        case 'N':
          pProtocol->bNAWS = TRUE;
          break;
        case 'M':
          pProtocol->bMSDP = TRUE;
          break;
        case 'A':
          pProtocol->bATCP = TRUE;
          break;
        case 'S':
          pProtocol->bMSP = TRUE;
          break;
        case 'X':
          pProtocol->bMXP = TRUE;
          pProtocol->pVariables[eMSDP_MXP]->ValueInt = 1;
          break;
        case 'c':
          pProtocol->bMCCP = TRUE;
          CompressStart(apDescriptor);
          break;
        case 'C':
          pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
          break;
        case 'H':
          pProtocol->bCHARSET = TRUE;
          break;
        case 'U':
          pProtocol->pVariables[eMSDP_UTF_8]->ValueInt = 1;
          break;
        default:
          if ( apData[i] == '/' )
            bDoneWidth = TRUE;
          else if ( isdigit(apData[i]) )
          {
            if ( bDoneWidth )
            {
              Height *= 10;
              Height += (apData[i] - '0');
            }
            else /* We're still calculating height */
            {
              Width *= 10;
              Width += (apData[i] - '0');
            }
          }
          break;
      }
    }

    /* Restore the width and height */
    pProtocol->ScreenWidth = Width;
    pProtocol->ScreenHeight = Height;

    /* If we're using MSDP or ATCP, we need to renegotiate it so that the
     * client can resend the list of variables it wants us to REPORT.
     *
     * Note that we only use ATCP if MSDP is not supported.
     */
    if ( pProtocol->bMSDP )
    {
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, TRUE, TRUE);
    }
    else if ( pProtocol->bATCP )
    {
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, TRUE, TRUE);
    }

    /* Ask the client to send its MXP version again */
    if ( pProtocol->bMXP )
      MXPSendTag( apDescriptor, "<VERSION>" );
  }
}

/******************************************************************************
 MSDP global functions.
 ******************************************************************************/

void MSDPUpdate( descriptor_t *apDescriptor )
{
  if (!apDescriptor) {
    ReportBug("apDescriptor was NULL on entering MSDPUpdate()!");
    return;
  }

  protocol_t *pProtocol = apDescriptor->pProtocol;

  for ( int i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
  {
    if ( !pProtocol->pVariables[i] )
      continue;

    if ( pProtocol->pVariables[i]->bReport && pProtocol->pVariables[i]->bDirty )
    {
      MSDPSend( apDescriptor, (variable_t)i );
      pProtocol->pVariables[i]->bDirty = FALSE;
    }
  }
}

void MSDPFlush( descriptor_t *apDescriptor, variable_t aMSDP )
{
  if ( aMSDP > eMSDP_NONE && aMSDP < eMSDP_MAX )
  {
    protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

    if (pProtocol && pProtocol->pVariables[aMSDP]) {
      if (pProtocol->pVariables[aMSDP]->bReport && pProtocol->pVariables[aMSDP]->bDirty) {
        MSDPSend( apDescriptor, aMSDP );
        pProtocol->pVariables[aMSDP]->bDirty = FALSE;
      }
    }
  }
}

void MSDPSend( descriptor_t *apDescriptor, variable_t aMSDP )
{
  char MSDPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

  if ( aMSDP > eMSDP_NONE && aMSDP < eMSDP_MAX )
  {
    protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

    if ( VariableNameTable[aMSDP].bString )
    {
      /* Should really be replaced with a dynamic buffer */
      int RequiredBuffer = strlen(VariableNameTable[aMSDP].pName) +
        strlen(pProtocol->pVariables[aMSDP]->pValueString) + 12;

      if ( RequiredBuffer >= MAX_VARIABLE_LENGTH )
      {
        snprintf( MSDPBuffer, sizeof(MSDPBuffer),
          "MSDPSend: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n",
          VariableNameTable[aMSDP].pName, RequiredBuffer,
          MAX_VARIABLE_LENGTH );
        ReportBug( MSDPBuffer );
        MSDPBuffer[0] = '\0';
      }
      else if ( pProtocol->bMSDP )
      {
        snprintf( MSDPBuffer, sizeof(MSDPBuffer), "%c%c%c%c%s%c%s%c%c",
          IAC, SB, TELOPT_MSDP, MSDP_VAR,
          VariableNameTable[aMSDP].pName, MSDP_VAL,
          pProtocol->pVariables[aMSDP]->pValueString, IAC, SE );
      }
      else if ( pProtocol->bATCP )
      {
        snprintf( MSDPBuffer, sizeof(MSDPBuffer), "%c%c%cMSDP.%s %s%c%c",
          IAC, SB, TELOPT_ATCP,
          VariableNameTable[aMSDP].pName,
          pProtocol->pVariables[aMSDP]->pValueString, IAC, SE );
      }
    }
    else /* It's an integer, not a string */
    {
      if ( pProtocol->bMSDP )
      {
        snprintf( MSDPBuffer, sizeof(MSDPBuffer), "%c%c%c%c%s%c%d%c%c",
          IAC, SB, TELOPT_MSDP, MSDP_VAR,
          VariableNameTable[aMSDP].pName, MSDP_VAL,
          pProtocol->pVariables[aMSDP]->ValueInt, IAC, SE );
      }
      else if ( pProtocol->bATCP )
      {
        snprintf( MSDPBuffer, sizeof(MSDPBuffer), "%c%c%cMSDP.%s %d%c%c",
          IAC, SB, TELOPT_ATCP,
          VariableNameTable[aMSDP].pName,
          pProtocol->pVariables[aMSDP]->ValueInt, IAC, SE );
      }
    }

    /* Just in case someone calls this function without checking MSDP/ATCP */
    if ( MSDPBuffer[0] != '\0' )
      Write( apDescriptor, MSDPBuffer );
  }
}

void MSDPSendPair( descriptor_t *apDescriptor, const char *apVariable, const char *apValue )
{
  char MSDPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

  if ( apVariable != NULL && apValue != NULL )
  {
    protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

    /* Should really be replaced with a dynamic buffer */
    int RequiredBuffer = strlen(apVariable) + strlen(apValue) + 12;

    if ( RequiredBuffer >= MAX_VARIABLE_LENGTH )
    {
      if ( RequiredBuffer - strlen(apValue) < MAX_VARIABLE_LENGTH )
      {
        snprintf( MSDPBuffer, sizeof(MSDPBuffer),
          "MSDPSendPair: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n",
          apVariable, RequiredBuffer, MAX_VARIABLE_LENGTH );
      }
      else /* The variable name itself is too long */
      {
        snprintf( MSDPBuffer, sizeof(MSDPBuffer),
          "MSDPSendPair: Variable name has a length of %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n",
          RequiredBuffer, MAX_VARIABLE_LENGTH );
      }

      ReportBug( MSDPBuffer );
      MSDPBuffer[0] = '\0';
    }
    else if ( pProtocol->bMSDP )
    {
      snprintf( MSDPBuffer, sizeof(MSDPBuffer), "%c%c%c%c%s%c%s%c%c",
        IAC, SB, TELOPT_MSDP, MSDP_VAR, apVariable, MSDP_VAL,
        apValue, IAC, SE );
    }
    else if ( pProtocol->bATCP )
    {
      snprintf( MSDPBuffer, sizeof(MSDPBuffer), "%c%c%cMSDP.%s %s%c%c",
        IAC, SB, TELOPT_ATCP, apVariable, apValue, IAC, SE );
    }

    /* Just in case someone calls this function without checking MSDP/ATCP */
    if ( MSDPBuffer[0] != '\0' )
      Write( apDescriptor, MSDPBuffer );
  }
}

void MSDPSendList( descriptor_t *apDescriptor, const char *apVariable, const char *apValue )
{
  char MSDPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

  if ( apVariable != NULL && apValue != NULL )
  {
    protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

    /* Should really be replaced with a dynamic buffer */
    int RequiredBuffer = strlen(apVariable) + strlen(apValue) + 12;

    if ( RequiredBuffer >= MAX_VARIABLE_LENGTH )
    {
      if ( RequiredBuffer - strlen(apValue) < MAX_VARIABLE_LENGTH )
      {
        snprintf( MSDPBuffer, sizeof(MSDPBuffer),
          "MSDPSendList: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n",
          apVariable, RequiredBuffer, MAX_VARIABLE_LENGTH );
      }
      else /* The variable name itself is too long */
      {
        snprintf( MSDPBuffer, sizeof(MSDPBuffer),
          "MSDPSendList: Variable name has a length of %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n",
          RequiredBuffer, MAX_VARIABLE_LENGTH );
      }

      ReportBug( MSDPBuffer );
      MSDPBuffer[0] = '\0';
    }
    else if ( pProtocol->bMSDP )
    {
      int i; /* Loop counter */
      snprintf( MSDPBuffer, sizeof(MSDPBuffer), "%c%c%c%c%s%c%c%c%s%c%c%c",
        IAC, SB, TELOPT_MSDP, MSDP_VAR, apVariable, MSDP_VAL,
        MSDP_ARRAY_OPEN, MSDP_VAL, apValue, MSDP_ARRAY_CLOSE, IAC, SE );

      /* Convert the spaces to MSDP_VAL */
      for ( i = 0; MSDPBuffer[i] != '\0'; ++i )
      {
        if ( MSDPBuffer[i] == ' ' )
          MSDPBuffer[i] = MSDP_VAL;
      }
    }
    else if ( pProtocol->bATCP )
    {
      snprintf( MSDPBuffer, sizeof(MSDPBuffer), "%c%c%cMSDP.%s %s%c%c",
        IAC, SB, TELOPT_ATCP, apVariable, apValue, IAC, SE );
    }

    /* Just in case someone calls this function without checking MSDP/ATCP */
    if ( MSDPBuffer[0] != '\0' )
      Write( apDescriptor, MSDPBuffer );
  }
}

void MSDPSetNumber( descriptor_t *apDescriptor, variable_t aMSDP, int aValue )
{
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol != NULL && aMSDP > eMSDP_NONE && aMSDP < eMSDP_MAX )
  {
    if ( !VariableNameTable[aMSDP].bString )
    {
      if ( pProtocol->pVariables[aMSDP] && pProtocol->pVariables[aMSDP]->ValueInt != aValue )
      {
        pProtocol->pVariables[aMSDP]->ValueInt = aValue;
        pProtocol->pVariables[aMSDP]->bDirty = TRUE;
      }
    }
  }
}

void MSDPSetString( descriptor_t *apDescriptor, variable_t aMSDP, const char *apValue )
{
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol != NULL && apValue != NULL )
  {
    if ( VariableNameTable[aMSDP].bString )
    {
      if ( strcmp(pProtocol->pVariables[aMSDP]->pValueString, apValue) )
      {
        delete [] pProtocol->pVariables[aMSDP]->pValueString;
        pProtocol->pVariables[aMSDP]->pValueString = AllocString(apValue);
        pProtocol->pVariables[aMSDP]->bDirty = TRUE;
      }
    }
  }
}

void MSDPSetTable( descriptor_t *apDescriptor, variable_t aMSDP, const char *apValue )
{
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol != NULL && apValue != NULL )
  {
    if ( *apValue == '\0' )
    {
      /* It's easier to call MSDPSetString if the value is empty */
      MSDPSetString(apDescriptor, aMSDP, apValue);
    }
    else if ( VariableNameTable[aMSDP].bString )
    {
      const char MsdpTableStart[] = { (char)MSDP_TABLE_OPEN, '\0' };
      const char MsdpTableStop[]  = { (char)MSDP_TABLE_CLOSE, '\0' };

      const size_t pTableSize = strlen(apValue) + 3;
      char *pTable = new char[pTableSize]; /* 3: START, STOP, NUL */

      strlcpy(pTable, MsdpTableStart, pTableSize);
      strlcat(pTable, apValue, pTableSize);
      strlcat(pTable, MsdpTableStop, pTableSize);

      if ( strcmp(pProtocol->pVariables[aMSDP]->pValueString, pTable) )
      {
        delete [] pProtocol->pVariables[aMSDP]->pValueString;
        pProtocol->pVariables[aMSDP]->pValueString = pTable;
        pProtocol->pVariables[aMSDP]->bDirty = TRUE;
      }
      else /* Just discard the table, we've already got one */
      {
        delete [] pTable;
      }
    }
  }
}

void MSDPSetArray( descriptor_t *apDescriptor, variable_t aMSDP, const char *apValue )
{
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol != NULL && apValue != NULL )
  {
    if ( *apValue == '\0' )
    {
      /* It's easier to call MSDPSetString if the value is empty */
      MSDPSetString(apDescriptor, aMSDP, apValue);
    }
    else if ( VariableNameTable[aMSDP].bString )
    {
      const char MsdpArrayStart[] = { (char)MSDP_ARRAY_OPEN, '\0' };
      const char MsdpArrayStop[]  = { (char)MSDP_ARRAY_CLOSE, '\0' };

      const size_t pArrayLen = strlen(apValue) + 3;
      char *pArray = new char[pArrayLen]; /* 3: START, STOP, NUL */

      strlcpy(pArray, MsdpArrayStart, pArrayLen);
      strlcat(pArray, apValue, pArrayLen);
      strlcat(pArray, MsdpArrayStop, pArrayLen);

      if ( strcmp(pProtocol->pVariables[aMSDP]->pValueString, pArray) )
      {
        delete [] pProtocol->pVariables[aMSDP]->pValueString;
        pProtocol->pVariables[aMSDP]->pValueString = pArray;
        pProtocol->pVariables[aMSDP]->bDirty = TRUE;
      }
      else /* Just discard the array, we've already got one */
      {
        delete [] pArray;
      }
    }
  }
}

/******************************************************************************
 MSSP global functions.
 ******************************************************************************/

void MSSPSetPlayers( int aPlayers )
{
  s_Players = aPlayers;

  if ( s_Uptime == 0 )
    s_Uptime = time(0);
}

/******************************************************************************
 MXP global functions.
 ******************************************************************************/

const char *MXPCreateTag( descriptor_t *apDescriptor, const char *apTag )
{
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol != NULL && pProtocol->pVariables[eMSDP_MXP] && pProtocol->pVariables[eMSDP_MXP]->ValueInt &&
    strlen(apTag) < 1000 )
  {
    static char MXPBuffer [1024];
    snprintf( MXPBuffer, sizeof(MXPBuffer), "\033[1z%s\033[7z", apTag );
    return MXPBuffer;
  }
  else /* Leave the tag as-is, don't try to MXPify it */
  {
    return apTag;
  }
}

void MXPSendTag( descriptor_t *apDescriptor, const char *apTag )
{
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol != NULL && apTag != NULL && strlen(apTag) < 1000 )
  {
    if ( pProtocol->pVariables[eMSDP_MXP] && pProtocol->pVariables[eMSDP_MXP]->ValueInt )
    {
      char MXPBuffer [1024];
      snprintf(MXPBuffer, sizeof(MXPBuffer), "\033[1z%s\033[7z\r\n", apTag );
      Write(apDescriptor, MXPBuffer);
    }
    else if ( pProtocol->bRenegotiate )
    {
      /* Tijer pointed out that when MUSHclient autoconnects, it fails
       * to complete the negotiation.  This workaround will attempt to
       * renegotiate after the character has connected.
       */

      int i; /* Renegotiate everything except TTYPE */
      for ( i = eNEGOTIATED_TTYPE+1; i < eNEGOTIATED_MAX; ++i )
      {
        // Don't renegotiate echo, we already handled everything related to this.
        if (i == TELOPT_ECHO)
          continue;

        pProtocol->Negotiated[i] = FALSE;
        ConfirmNegotiation(apDescriptor, (negotiated_t)i, TRUE, TRUE);
      }

      pProtocol->bRenegotiate = FALSE;
      pProtocol->bNeedMXPVersion = TRUE;
      Negotiate(apDescriptor);
    }
  }
}

/******************************************************************************
 Sound global functions.
 ******************************************************************************/

void SoundSend( descriptor_t *apDescriptor, const char *apTrigger )
{
  const int MaxTriggerLength = 128; /* Used for the buffer size */

  if ( apDescriptor != NULL && apTrigger != NULL )
  {
    protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

    if ( pProtocol != NULL && pProtocol->pVariables[eMSDP_SOUND] && pProtocol->pVariables[eMSDP_SOUND]->ValueInt )
    {
      if ( pProtocol->bMSDP || pProtocol->bATCP )
      {
        /* Send the sound trigger through MSDP or ATCP */
        MSDPSendPair( apDescriptor, "PLAY_SOUND", apTrigger );
      }
      else if ( strlen(apTrigger) <= MaxTriggerLength )
      {
        /* Use an old MSP-style trigger */
        size_t length = MaxTriggerLength+10;
        char *pBuffer = new char[length];
        snprintf( pBuffer, length, "\t!SOUND(%s)", apTrigger );
        Write(apDescriptor, pBuffer);
        delete [] pBuffer;
      }
    }
  }
}

/******************************************************************************
 Colour global functions.
 ******************************************************************************/

const char *ColourRGB( descriptor_t *apDescriptor, const char *apRGB, bool coerce_to_ansi )
{
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol && pProtocol->pVariables[eMSDP_ANSI_COLORS]->ValueInt )
  {
    if ( IsValidColour(apRGB) )
    {
      bool bBackground = (tolower(apRGB[0]) == 'b');
      int Red = apRGB[1] - '0';
      int Green = apRGB[2] - '0';
      int Blue = apRGB[3] - '0';
      int total = Red + Blue + Green;

      if (total == 0) {
       Red = 1;
       Green = 1;
       Blue = 1;
      } else if (total < 2) {
      // Super dark? Don't be a douche.
      if (Red > 0)
        Red += 1;
      if (Green > 0)
        Green += 1;
      if (Blue > 0)
        Blue += 1;
      }

      // We only coerce ANSI colors if the player has indicated they want this.
      coerce_to_ansi &= pProtocol->do_coerce_ansi_capable_colors_to_ansi;

      if ( !coerce_to_ansi && pProtocol->pVariables[eMSDP_XTERM_256_COLORS] && pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt ) {
        return GetRGBColour( bBackground, Red, Green, Blue );
      } else { /* Use regular ANSI colour */
        return GetAnsiColour( bBackground, Red, Green, Blue );
      }
    }
    else /* Invalid colour - use this to clear any existing colour. */
    {
      return s_Clean;
    }
  }
  else /* Don't send any colour, not even clear */
  {
    return "";
  }
}

void disable_xterm_256(descriptor_t *apDescriptor) {
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol && pProtocol->pVariables[eMSDP_XTERM_256_COLORS]) {
   if ( pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt ) {
    pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 0;
   }
  } else {
   mudlog("SYSERR: Unable to disable xterm256 colors for descriptor.", NULL, LOG_SYSLOG, TRUE);
  }
}

void enable_xterm_256(descriptor_t *apDescriptor) {
  protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

  if ( pProtocol && pProtocol->pVariables[eMSDP_XTERM_256_COLORS]) {
   pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
  } else {
   mudlog("SYSERR: Unable to enable xterm256 colors for descriptor.", NULL, LOG_SYSLOG, TRUE);
  }
}

/******************************************************************************
 UTF-8 global functions.
 ******************************************************************************/

char *UnicodeGet( int aValue )
{
  static char Buffer[8];
  char *pString = Buffer;

  UnicodeAdd( &pString, aValue );
  *pString = '\0';

  return Buffer;
}

void UnicodeAdd( char **apString, int aValue )
{
  if ( aValue < 0x80 )
  {
    *(*apString)++ = (char)aValue;
  }
  else if ( aValue < 0x800 )
  {
    *(*apString)++ = (char)(0xC0 | (aValue>>6));
    *(*apString)++ = (char)(0x80 | (aValue & 0x3F));
  }
  else if ( aValue < 0x10000 )
  {
    *(*apString)++ = (char)(0xE0 | (aValue>>12));
    *(*apString)++ = (char)(0x80 | (aValue>>6 & 0x3F));
    *(*apString)++ = (char)(0x80 | (aValue & 0x3F));
  }
  else if ( aValue < 0x200000 )
  {
    *(*apString)++ = (char)(0xF0 | (aValue>>18));
    *(*apString)++ = (char)(0x80 | (aValue>>12 & 0x3F));
    *(*apString)++ = (char)(0x80 | (aValue>>6 & 0x3F));
    *(*apString)++ = (char)(0x80 | (aValue & 0x3F));
  }
}

/******************************************************************************
 Local negotiation functions.
 ******************************************************************************/

static void Negotiate( descriptor_t *apDescriptor )
{
  PROTO_DEBUG_MSG("Entering Negotiate(*)");
  protocol_t *pProtocol = apDescriptor->pProtocol;

  if ( pProtocol->bNegotiated )
  {
    const char RequestTTYPE  [] = { (char)IAC, (char)SB,  TELOPT_TTYPE, SEND, (char)IAC, (char)SE, '\0' };

    /* Request the client type if TTYPE is supported. */
    if ( pProtocol->bTTYPE )
      Write(apDescriptor, RequestTTYPE);

    /* Check for other protocols. */
    ConfirmNegotiation(apDescriptor, eNEGOTIATED_NAWS, TRUE, TRUE);
    ConfirmNegotiation(apDescriptor, eNEGOTIATED_CHARSET, TRUE, TRUE);
    ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, TRUE, TRUE);
    ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSSP, TRUE, TRUE);
    ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, TRUE, TRUE);
    ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSP, TRUE, TRUE);
    ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP, TRUE, TRUE);
    ConfirmNegotiation(apDescriptor, eNEGOTIATED_MCCP, TRUE, TRUE);
    
    Write(apDescriptor, (char[]) { (char)IAC, (char)DO, TELOPT_NEW_ENVIRON, '\0' });
  }
}

static void PerformHandshake( descriptor_t *apDescriptor, char aCmd, char aProtocol )
{
  PROTO_DEBUG_MSG("Entering PerformHandshake(*, %d, %d)", (unsigned char)aCmd, (unsigned char)aProtocol);
  protocol_t *pProtocol = apDescriptor->pProtocol;

  switch ( aProtocol )
  {
    case (char)TELOPT_TTYPE:
      if ( aCmd == (char)WILL )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_TTYPE, TRUE, TRUE);
        pProtocol->bTTYPE = TRUE;

        if ( !pProtocol->bNegotiated )
        {
          /* Negotiate for the remaining protocols. */
          pProtocol->bNegotiated = TRUE;
          Negotiate(apDescriptor);

          /* We may need to renegotiate if they don't reply */
          pProtocol->bRenegotiate = TRUE;
        }
      }
      else if ( aCmd == (char)WONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_TTYPE, FALSE, pProtocol->bTTYPE);
        pProtocol->bTTYPE = FALSE;

        if ( !pProtocol->bNegotiated )
        {
          /* Still negotiate, as this client obviously knows how to
           * correctly respond to negotiation attempts - but we don't
           * ask for TTYPE, as it doesn't support it.
           */
          pProtocol->bNegotiated = TRUE;
          Negotiate(apDescriptor);

          /* We may need to renegotiate if they don't reply */
          pProtocol->bRenegotiate = TRUE;
        }
      }
      else if ( aCmd == (char)DO )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC DO TTYPE from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
      }
      break;

    case (char)TELOPT_ECHO:
      /* Disabled the ability for the protocol snippet to respond to this, because:
        - We decide when we're echoing anyways, so this has no mechanical effect, AND
        - This can cause a race condition where the client and server endlessly spam DO DONT WILL WONT at each other.
      */

#ifdef LOG_TELOPT_ECHO
      log_vfprintf("Received IAC %s ECHO from %ld.", aCmd == (char)DO ? "DO" : "DONT", apDescriptor->descriptor);
#endif

#ifdef RESPOND_TO_TELOPT_ECHO
      if ( aCmd == (char)DO )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_ECHO, TRUE, TRUE);
        pProtocol->bECHO = TRUE;
      }
      else if ( aCmd == (char)DONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_ECHO, FALSE, pProtocol->bECHO);
        pProtocol->bECHO = FALSE;
      }
      else if ( aCmd == (char)WILL )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC WILL ECHO from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
      }
 #endif
      break;

    case (char)TELOPT_NAWS:
      if ( aCmd == (char)WILL )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_NAWS, TRUE, TRUE);
        pProtocol->bNAWS = TRUE;

        /* Renegotiation workaround won't be necessary. */
        pProtocol->bRenegotiate = FALSE;
      }
      else if ( aCmd == (char)WONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_NAWS, FALSE, pProtocol->bNAWS);
        pProtocol->bNAWS = FALSE;

        /* Renegotiation workaround won't be necessary. */
        pProtocol->bRenegotiate = FALSE;
      }
      else if ( aCmd == (char)DO )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC DO NAWS from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
      }
      break;

    case (char)TELOPT_CHARSET:
      if ( aCmd == (char)WILL )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_CHARSET, TRUE, TRUE);
        if ( !pProtocol->bCHARSET )
        {
          const char charset_utf8 [] = { (char)IAC, (char)SB, TELOPT_CHARSET, 1, ' ', 'U', 'T', 'F', '-', '8', (char)IAC, (char)SE, '\0' };
          Write(apDescriptor, charset_utf8);
          pProtocol->bCHARSET = TRUE;
        }
      }
      else if ( aCmd == (char)WONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_CHARSET, FALSE, pProtocol->bCHARSET);
        pProtocol->bCHARSET = FALSE;
      }
      else if ( aCmd == (char)DO )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC DO CHARSET from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
      }
      break;

    case (char)TELOPT_MSDP:
      if ( aCmd == (char)DO )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, TRUE, TRUE);

        if ( !pProtocol->bMSDP )
        {
          pProtocol->bMSDP = TRUE;

          /* Identify the mud to the client. */
          MSDPSendPair( apDescriptor, "SERVER_ID", MUD_NAME );
        }
      }
      else if ( aCmd == (char)DONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, FALSE, pProtocol->bMSDP);
        pProtocol->bMSDP = FALSE;
      }
      else if ( aCmd == (char)WILL )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC WILL MSDP from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
      }
      break;

    case (char)TELOPT_MSSP:
      if ( aCmd == (char)DO )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSSP, TRUE, TRUE);

        if ( !pProtocol->bMSSP )
        {
          SendMSSP( apDescriptor );
          pProtocol->bMSSP = TRUE;
        }
      }
      else if ( aCmd == (char)DONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSSP, FALSE, pProtocol->bMSSP);
        pProtocol->bMSSP = FALSE;
      }
      else if ( aCmd == (char)WILL )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC WILL MSSP from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
      }
      break;

    case (char)TELOPT_MCCP:
      if ( aCmd == (char)DO )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MCCP, TRUE, TRUE);

        if ( !pProtocol->bMCCP )
        {
          pProtocol->bMCCP = TRUE;
          CompressStart( apDescriptor );
        }
      }
      else if ( aCmd == (char)DONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MCCP, FALSE, pProtocol->bMCCP);

        if ( pProtocol->bMCCP )
        {
          pProtocol->bMCCP = FALSE;
          CompressEnd( apDescriptor );
        }
      }
      else if ( aCmd == (char)WILL )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC WILL MCCP from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
      }
      break;

    case (char)TELOPT_GMCP:
      if ( aCmd == (char)DO )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_GMCP, TRUE, TRUE);
        pProtocol->bGMCP = TRUE;
      }
      else if ( aCmd == (char)DONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_GMCP, FALSE, pProtocol->bGMCP);
        pProtocol->bGMCP = FALSE;
      }
      else if ( aCmd == (char)WILL )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC WILL MSP from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
      }
      break;

    case (char)TELOPT_MSP:
      if ( aCmd == (char)DO )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSP, TRUE, TRUE);
        pProtocol->bMSP = TRUE;
      }
      else if ( aCmd == (char)DONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSP, FALSE, pProtocol->bMSP);
        pProtocol->bMSP = FALSE;
      }
      else if ( aCmd == (char)WILL )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC WILL MSP from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
      }
      break;

    case (char)TELOPT_NEW_ENVIRON:
      if ( aCmd == (char)WILL )
      {
        PROTO_DEBUG_MSG("Received IAC WILL NEW-ENVIRON from client, requesting IPADDRESS.");
        // IAC SB NEW-ENVIRON SEND VAR "IPADDRESS" IAC SE
        Write(apDescriptor, (char[]) { (char)IAC, (char)SB, TELOPT_NEW_ENVIRON, NEW_ENV_SEND, NEW_ENV_USERVAR, 'I', 'P', 'A', 'D', 'D', 'R', 'E', 'S', 'S', (char)IAC, (char)SE, 0 });
      }
      else if ( aCmd == (char)WONT )
      {
        // Not supported. Ignore.
        PROTO_DEBUG_MSG("Received IAC WONT NEW-ENVIRON from client, leaving it be.");
      }
      else {
        // Invalid mode. Ignore.
        log_vfprintf("Received invalid IAC %s NEW-ENVIRON from client, ignoring.", aCmd == (char)DO ? "DO" : "DONT");
      }
      break;

    case (char)TELOPT_MXP:
      if ( aCmd == (char)WILL || aCmd == (char)DO )
      {
        if ( aCmd == (char)WILL )
          ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP, TRUE, TRUE);
        else /* aCmd == (char)DO */
          ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP2, TRUE, TRUE);

        if ( !pProtocol->bMXP )
        {
          /* Enable MXP. */
          const char EnableMXP[] = { (char)IAC, (char)SB, TELOPT_MXP, (char)IAC, (char)SE, '\0' };
          Write(apDescriptor, EnableMXP);

          /* Create a secure channel, and note that MXP is active. */
          Write(apDescriptor, "\033[7z");
          pProtocol->bMXP = TRUE;
          pProtocol->pVariables[eMSDP_MXP]->ValueInt = 1;

          if ( pProtocol->bNeedMXPVersion )
            MXPSendTag( apDescriptor, "<VERSION>" );
        }
      }
      else if ( aCmd == (char)WONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP, FALSE, pProtocol->bMXP);

        if ( !pProtocol->bMXP )
        {
          /* The MXP standard doesn't actually specify whether you should
           * negotiate with IAC DO MXP or IAC WILL MXP.  As a result, some
           * clients support one, some the other, and some support both.
           *
           * Therefore we first try IAC DO MXP, and if the client replies
           * with WONT, we try again (here) with IAC WILL MXP.
           */
          ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP2, TRUE, TRUE);
        }
        else /* The client is actually asking us to switch MXP off. */
        {
          pProtocol->bMXP = FALSE;
        }
      }
      else if ( aCmd == (char)DONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP2, FALSE, pProtocol->bMXP);
        pProtocol->bMXP = FALSE;
      }
      break;

    case (char)TELOPT_ATCP:
      if ( aCmd == (char)WILL )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, TRUE, TRUE);

        /* If we don't support MSDP, fake it with ATCP */
        if ( !pProtocol->bMSDP && !pProtocol->bATCP )
        {
          pProtocol->bATCP = TRUE;

#ifdef MUDLET_PACKAGE
          /* Send the Mudlet GUI package to the user. */
          if ( MatchString( "Mudlet",
            pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString ) )
          {
            SendATCP( apDescriptor, "Client.GUI", MUDLET_PACKAGE );
          }
#endif /* MUDLET_PACKAGE */

          /* Identify the mud to the client. */
          MSDPSendPair( apDescriptor, "SERVER_ID", MUD_NAME );
        }
      }
      else if ( aCmd == (char)WONT )
      {
        ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, FALSE, pProtocol->bATCP);
        pProtocol->bATCP = FALSE;
      }
      else if ( aCmd == (char)DO )
      {
        /* Invalid negotiation, send a rejection */
        log("Received invalid IAC DO ATCP from client, denying.");
        SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
      }
      break;

    default:
      log_vfprintf("Received invalid IAC %s %d from client, denying.", aCmd == (char)WILL ? "WILL" : "DO", aProtocol);
      if ( aCmd == (char)WILL ) {
        /* Invalid negotiation, send a rejection */
        SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
      }
      else if ( aCmd == (char)DO ) {
        /* Invalid negotiation, send a rejection */
        SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
      }
      break;
  }
}

static void PerformSubnegotiation( descriptor_t *apDescriptor, char aCmd, char *apData, size_t aSize )
{
  protocol_t *pProtocol = apDescriptor->pProtocol;

  switch ( aCmd )
  {
    case (char)TELOPT_NEW_ENVIRON:
      {
        PROTO_DEBUG_MSG("Entering PerformSubnegotiation's TELOPT_NEW_ENVIRON case.");
        // We care about the very specific response message of (IAC SB NEW-ENVIRON IS VAR "IPADDRESS" VAL "the ip" IAC SE).
        // We receive it to this switch case as (IS VAR "IPADDRESS" VAL "the ip").
        size_t required_min_len = 3 /* telnet flags */ + 9 /* strlen("IPADDRESS") */ + 7 /* strlen of absolute smallest IP, 1.1.1.1 */;
        if (aSize >= required_min_len) {
          if (apData[0] == (char)NEW_ENV_IS && (apData[1] == (char)NEW_ENV_VAR || apData[1] == (char)NEW_ENV_USERVAR)) {
            // Require that they've sent (IS VAR "IPADDRESS" VAL)
            char expected_input[] = { 'I', 'P', 'A', 'D', 'D', 'R', 'E', 'S', 'S', (char)NEW_ENV_VALUE, '\0' };
            if (!strncmp(apData, expected_input, strlen(expected_input))) {
              // Capture the value.
              char ipAddr[1000];
              memset(ipAddr, 0, sizeof(ipAddr));
              for (size_t idx = 12; idx < sizeof(ipAddr) - 1 && apData[idx] && apData[idx] != (char)IAC; idx++)
                ipAddr[idx - 12] = apData[idx];
              // Print it to logs.
              PROTO_DEBUG_MSG("Received IP of '%s' from TELOPT_NEW_ENVIRON.", ipAddr);
            } else {
              // Copy it out and ensure it's zero-terminated.
              char writable[1000];
              memset(writable, 0, sizeof(writable));
              for (size_t idx = 2; idx < sizeof(writable) - 1 && apData[idx]; idx++)
                writable[idx - 2] = apData[idx];
              PROTO_DEBUG_MSG("- Bailing out: Does not continue with 'IPADDRESS' VAL, instead is '%s'", writable);
            }
          } else {
            PROTO_DEBUG_MSG("- Bailing out: Does not start with IS (VAR|USERVAR), instead is %d %d", (int) apData[0], (int) apData[1]);
          }
        } else {
          PROTO_DEBUG_MSG("- Bailing out: Size %d is less than required.", aSize);
        }
      }
      break;
    case (char)TELOPT_TTYPE:
      if ( pProtocol->bTTYPE )
      {
        /* Store the client name. */
        const int MaxClientLength = 64;
        char *pClientName = new char[MaxClientLength+1];
        int i = 0, j = 1;
        bool bStopCyclicTTYPE = FALSE;

        for ( ; apData[j] != '\0' && i < MaxClientLength; ++j )
        {
          if ( isprint(apData[j]) )
            pClientName[i++] = apData[j];
        }
        pClientName[i] = '\0';

        /* Store the first TTYPE as the client name */
        if ( !strcmp(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString, "Unknown") )
        {
          delete [] pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString;
          pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pClientName);

          /* This is a bit nasty, but using cyclic TTYPE on windows telnet
           * causes it to lock up.  None of the clients we need to cycle
           * with send ANSI to start with anyway, so we shouldn't have any
           * conflicts.
           *
           * An alternative solution is to use escape sequences to check
           * for windows telnet prior to negotiation, and this also avoids
           * the player losing echo, but it has other issues.  Because the
           * escape codes are technically in-band, even though they'll be
           * stripped from the display, the newlines will still cause some
           * scrolling.  Therefore you need to either pause the session
           * for a few seconds before displaying the login screen, or wait
           * until the player has entered their name before negotiating.
           */
          if ( !strcmp(pClientName,"ANSI") )
            bStopCyclicTTYPE = TRUE;
        }

        /* Cycle through the TTYPEs until we get the same result twice, or
         * find ourselves back at the start.
         *
         * If the client follows RFC1091 properly then it will indicate the
         * end of the list by repeating the last response, and then return
         * to the top of the list.  If you're the trusting type, then feel
         * free to remove the second strcmp ;)
         */
        if ( pProtocol->pLastTTYPE == NULL ||
          (strcmp(pProtocol->pLastTTYPE, pClientName) &&
          strcmp(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString, pClientName)) )
        {
          char RequestTTYPE [] = { (char)IAC, (char)SB, TELOPT_TTYPE, SEND, (char)IAC, (char)SE, '\0' };
          const char *pStartPos = strstr( pClientName, "-" );

          /* Store the TTYPE */
          delete [] pProtocol->pLastTTYPE;
          pProtocol->pLastTTYPE = AllocString(pClientName);

          /* Look for 256 colour support */
          if ( pStartPos != NULL && MatchString(pStartPos, "-256color") )
          {
            /* This is currently the only way to detect support for 256
             * colours in TinTin++, WinTin++ and BlowTorch.
             */
            pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
            pProtocol->b256Support = eYES;
          }

          /* Request another TTYPE */
          if ( !bStopCyclicTTYPE )
            Write(apDescriptor, RequestTTYPE);
        }

        if ( PrefixString("Mudlet", pClientName) )
        {
          /* Mudlet beta 15 and later supports 256 colours, but we can't
           * identify it from the mud - everything prior to 1.1 claims
           * to be version 1.0, so we just don't know.
           */
          pProtocol->b256Support = eYES;
          pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;

          if ( strlen(pClientName) > 7 )
          {
            pClientName[6] = '\0';
            delete [] pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString;
            pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pClientName);
            delete [] pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString;
            pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString = AllocString(pClientName+7);
          }
        }
        else if ( MatchString(pClientName, "EMACS-RINZAI") )
        {
          /* We know for certain that this client has support */
          pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
          pProtocol->b256Support = eYES;
        }
        else if ( PrefixString("DecafMUD", pClientName) )
        {
          /* We know for certain that this client has support */
          pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
          pProtocol->b256Support = eYES;

          if ( strlen(pClientName) > 9 )
          {
            pClientName[8] = '\0';
            delete [] pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString;
            pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pClientName);
            delete [] pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString;
            pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString = AllocString(pClientName+9);
          }
        }
        else if ( MatchString(pClientName, "MUSHCLIENT") ||
          MatchString(pClientName, "CMUD") ||
          MatchString(pClientName, "ATLANTIS") ||
          MatchString(pClientName, "KILDCLIENT") ||
          MatchString(pClientName, "TINTIN++") ||
          MatchString(pClientName, "TINYFUGUE") )
        {
          /* We know that some versions of this client have support */
          pProtocol->b256Support = eSOMETIMES;
        }
        else if ( MatchString(pClientName, "ZMUD") )
        {
          /* We know for certain that this client does not have support */
          pProtocol->b256Support = eNO;
        }
        else if ( PrefixString(pClientName, "Beip") )
        {
          pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
          pProtocol->b256Support = eYES;
        }

        if (pClientName)
         delete [] pClientName;
        pClientName = NULL;
      }
      break;

    case (char)TELOPT_NAWS:
      if ( pProtocol->bNAWS )
      {
        /* Store the new width. */
        pProtocol->ScreenWidth = (unsigned char)apData[0];
        pProtocol->ScreenWidth <<= 8;
        pProtocol->ScreenWidth += (unsigned char)apData[1];

        /* Store the new height. */
        pProtocol->ScreenHeight = (unsigned char)apData[2];
        pProtocol->ScreenHeight <<= 8;
        pProtocol->ScreenHeight += (unsigned char)apData[3];
      }
      break;

    case (char)TELOPT_CHARSET:
      if ( pProtocol->bCHARSET )
      {
        /* Because we're only asking about UTF-8, we can just check the
         * first character.  If you ask for more than one CHARSET you'll
         * need to read through the results to see which are accepted.
         *
         * Note that the user must also use a unicode font!
         */
        if ( apData[0] == ACCEPTED )
          pProtocol->pVariables[eMSDP_UTF_8]->ValueInt = 1;
      }
      break;

    case (char)TELOPT_MSDP:
      if ( pProtocol->bMSDP )
      {
        ParseMSDP( apDescriptor, apData );
      }
      break;

    case (char)TELOPT_ATCP:
      if ( pProtocol->bATCP )
      {
        ParseATCP( apDescriptor, apData );
      }
      break;

    default: /* Unknown subnegotiation, so we simply ignore it. */
      break;
  }
}

static void SendNegotiationSequence( descriptor_t *apDescriptor, char aCmd, char aProtocol )
{
  char NegotiateSequence[4];

  NegotiateSequence[0] = (char)IAC;
  NegotiateSequence[1] = aCmd;
  NegotiateSequence[2] = aProtocol;
  NegotiateSequence[3] = '\0';

  Write(apDescriptor, NegotiateSequence);
}

static bool ConfirmNegotiation( descriptor_t *apDescriptor, negotiated_t aProtocol, bool abWillDo, bool abSendReply )
{
  bool bResult = FALSE;

  if ( aProtocol >= eNEGOTIATED_TTYPE && aProtocol < eNEGOTIATED_MAX )
  {
    /* Only negotiate if the state has changed. */
    if ( apDescriptor->pProtocol->Negotiated[aProtocol] != abWillDo )
    {
      /* Store the new state. */
      apDescriptor->pProtocol->Negotiated[aProtocol] = abWillDo;

      bResult = TRUE;

      if ( abSendReply )
      {
        switch ( aProtocol )
        {
          case eNEGOTIATED_TTYPE:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? DO : DONT), TELOPT_TTYPE );
            break;
          case eNEGOTIATED_ECHO:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? WILL : WONT), TELOPT_ECHO );
            break;
          case eNEGOTIATED_NAWS:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? DO : DONT), TELOPT_NAWS );
            break;
          case eNEGOTIATED_CHARSET:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? DO : DONT), TELOPT_CHARSET );
            break;
          case eNEGOTIATED_MSDP:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? WILL : WONT), TELOPT_MSDP );
            break;
          case eNEGOTIATED_MSSP:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? WILL : WONT), TELOPT_MSSP );
            break;
          case eNEGOTIATED_ATCP:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? DO : DONT), (char)TELOPT_ATCP );
            break;
          case eNEGOTIATED_MSP:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? WILL : WONT), TELOPT_MSP );
            break;
          case eNEGOTIATED_MXP:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? DO : DONT), TELOPT_MXP );
            break;
          case eNEGOTIATED_MXP2:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? WILL : WONT), TELOPT_MXP );
            break;
          case eNEGOTIATED_GMCP:
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? DO : WONT), TELOPT_GMCP );
            break;
          case eNEGOTIATED_MCCP:
#ifdef USING_MCCP
            SendNegotiationSequence( apDescriptor, (char)(abWillDo ? WILL : WONT), TELOPT_MCCP );
#endif /* USING_MCCP */
            break;
          default:
            bResult = FALSE;
            break;
        }
      }
    }
  }

  return bResult;
}

/******************************************************************************
 GMCP functions.
 ******************************************************************************/
void SendGMCPCoreSupports ( descriptor_t *apDescriptor )
{
  if (!apDescriptor || !apDescriptor->pProtocol->bGMCP) return;
  json j;

  j["Core"] = json::array();
  j["Core"].push_back("Supports");

  j["Room"] = json::array();
  j["Room"].push_back("Info");
  j["Room"].push_back("Exits");

  j["Char"] = json::array();
  j["Char"].push_back("Status");

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(apDescriptor, "Core.Supports", payload.c_str());
}

void SendGMCPExitsInfo( struct char_data *ch ) 
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  struct veh_data *veh = NULL;

  json j;
  j["exits"] = json::array();

  for (int door = 0; door < NUM_OF_DIRS; door++)
    if (EXIT(ch, door) && EXIT(ch, door)->to_room && EXIT(ch, door)->to_room != &world[0]) {
      if (ch->in_veh || ch->char_specials.rigging) {
        RIG_VEH(ch, veh);
        if (!ROOM_FLAGGED(EXIT(veh, door)->to_room, ROOM_ROAD) &&
            !ROOM_FLAGGED(EXIT(veh, door)->to_room, ROOM_GARAGE) &&
            !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN))
          j["exits"].push_back({"direction", exitdirs[door], "state", "INACCESSIBLE"});
        else if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED | EX_HIDDEN))
          j["exits"].push_back({"direction", exitdirs[door], "state", "OPEN"});
      } else {
        if (!IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN) || GET_LEVEL(ch) > LVL_MORTAL) {
          if (IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED))
            j["exits"].push_back({"direction", exitdirs[door], "state", "LOCKED"});
          else if (IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED))
            j["exits"].push_back({"direction", exitdirs[door], "state", "CLOSED"});
          else
            j["exits"].push_back({"direction", exitdirs[door], "state", "OPEN"});
        }
      }
    }

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Room.Exits", payload.c_str());
}

void SendGMCPRoomInfo( struct char_data *ch, struct room_data *room ) 
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;

  j["room_vnum"] = GET_ROOM_VNUM(room);
  j["room_name"] = GET_ROOM_NAME(room);
  j["zone_number"] = room->zone;
  // Only add coordinates if they are valid.
  if (room->x && room->y && room->z) {
    j["coords"] = { {"x", room->x}, {"y", room->y}, {"z", room->z} };
  }

  // Add room description if it exists; any quotes will be automatically escaped.
  if (*room->description) {
    j["room_description"] = room->description;
  }

  if (room->latitude) j["latitude"] = room->latitude;
  if (room->longitude) j["longitude"] = room->longitude;
  
  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Room.Info", payload.c_str());

  SendGMCPExitsInfo(ch);
}

void SendGMCP( descriptor_t *apDescriptor, const char *module, const char *apData )
{
  if (!apDescriptor->pProtocol->bGMCP) return;
  char buf[MAX_STRING_LENGTH];
  // Build the GMCP message: IAC SB GMCP [module] [json] IAC SE
  snprintf(buf, sizeof(buf), "%c%c%c%s %s%c%c", IAC, SB, TELOPT_GMCP, module, apData, IAC, SE);

  Write(apDescriptor, buf);
}

void SendGMCPCharStatus( struct char_data * ch )
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;

  j["physical"] = GET_PHYSICAL(ch);
  j["mental"] = GET_MENTAL(ch);
  j["physical_max"] = GET_MAX_PHYSICAL(ch);
  j["mental_max"] = GET_MAX_MENTAL(ch);
  j["karma"] = GET_KARMA(ch);
  j["pools"] = {"body", GET_BODY_POOL(ch), "magic", GET_MAGIC_POOL(ch), "combat", GET_COMBAT_POOL(ch), "hacking", GET_HACKING(ch)};
  j["task_pools"] = json::object();
  for (int x = 0; x < 7; x++)
    j["task_pools"][attributes[x]] = GET_TASK_POOL(ch, x);  

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Char.Status", payload.c_str());
}

/******************************************************************************
 Local MSDP functions.
 ******************************************************************************/

static void ParseMSDP( descriptor_t *apDescriptor, const char *apData )
{
  char Variable[MSDP_VAL][MAX_MSDP_SIZE+1] = { {'\0'}, {'\0'} };
  char *pPos = NULL, *pStart = NULL;

  while ( *apData )
  {
    switch ( *apData )
    {
      case MSDP_VAR: case MSDP_VAL:
        pPos = pStart = Variable[*apData++-1];
        break;
      default: /* Anything else */
        if ( pPos && pPos-pStart < MAX_MSDP_SIZE )
        {
          *pPos++ = *apData;
          *pPos = '\0';
        }

        if ( *++apData )
          continue;
    }

    ExecuteMSDPPair( apDescriptor, Variable[MSDP_VAR-1], Variable[MSDP_VAL-1] );
    Variable[MSDP_VAL-1][0] = '\0';
  }
}

static void ExecuteMSDPPair( descriptor_t *apDescriptor, const char *apVariable, const char *apValue )
{
  if ( apVariable[0] != '\0' && apValue[0] != '\0' )
  {
    if ( MatchString(apVariable, "SEND") )
    {
      bool bDone = FALSE;
      int i; /* Loop counter */
      for ( i = eMSDP_NONE+1; i < eMSDP_MAX && !bDone; ++i )
      {
        if ( MatchString(apValue, VariableNameTable[i].pName) )
        {
          MSDPSend( apDescriptor, (variable_t)i );
          bDone = TRUE;
        }
      }
    }
    else if ( MatchString(apVariable, "REPORT") )
    {
      bool bDone = FALSE;
      int i; /* Loop counter */
      for ( i = eMSDP_NONE+1; i < eMSDP_MAX && !bDone; ++i )
      {
        if ( MatchString(apValue, VariableNameTable[i].pName) )
        {
          apDescriptor->pProtocol->pVariables[i]->bReport = TRUE;
          apDescriptor->pProtocol->pVariables[i]->bDirty = TRUE;
          bDone = TRUE;
        }
      }
    }
    else if ( MatchString(apVariable, "RESET") )
    {
      if ( MatchString(apValue, "REPORTABLE_VARIABLES") ||
        MatchString(apValue, "REPORTED_VARIABLES") )
      {
        int i; /* Loop counter */
        for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
        {
          if ( apDescriptor->pProtocol->pVariables[i]->bReport )
          {
            apDescriptor->pProtocol->pVariables[i]->bReport = FALSE;
            apDescriptor->pProtocol->pVariables[i]->bDirty = FALSE;
          }
        }
      }
    }
    else if ( MatchString(apVariable, "UNREPORT") )
    {
      bool bDone = FALSE;
      int i; /* Loop counter */
      for ( i = eMSDP_NONE+1; i < eMSDP_MAX && !bDone; ++i )
      {
        if ( MatchString(apValue, VariableNameTable[i].pName) )
        {
          apDescriptor->pProtocol->pVariables[i]->bReport = FALSE;
          apDescriptor->pProtocol->pVariables[i]->bDirty = FALSE;
          bDone = TRUE;
        }
      }
    }
    else if ( MatchString(apVariable, "LIST") )
    {
      if ( MatchString(apValue, "COMMANDS") )
      {
        const char MSDPCommands[] = "LIST REPORT RESET SEND UNREPORT";
        MSDPSendList( apDescriptor, "COMMANDS", MSDPCommands );
      }
      else if ( MatchString(apValue, "LISTS") )
      {
        const char MSDPCommands[] = "COMMANDS LISTS CONFIGURABLE_VARIABLES REPORTABLE_VARIABLES REPORTED_VARIABLES SENDABLE_VARIABLES GUI_VARIABLES";
        MSDPSendList( apDescriptor, "LISTS", MSDPCommands );
      }
      /* Split this into two if some variables aren't REPORTABLE */
      else if ( MatchString(apValue, "SENDABLE_VARIABLES") ||
        MatchString(apValue, "REPORTABLE_VARIABLES") )
      {
        char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
        int i; /* Loop counter */

        for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
        {
          if ( !VariableNameTable[i].bGUI )
          {
            /* Add the separator between variables */
            strlcat( MSDPCommands, " ", MAX_OUTPUT_BUFFER );

            /* Add the variable to the list */
            strlcat( MSDPCommands, VariableNameTable[i].pName, MAX_OUTPUT_BUFFER );
          }
        }

        MSDPSendList( apDescriptor, apValue, MSDPCommands );
      }
      else if ( MatchString(apValue, "REPORTED_VARIABLES") )
      {
        char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
        int i; /* Loop counter */

        for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
        {
          if ( apDescriptor->pProtocol->pVariables[i]->bReport )
          {
            /* Add the separator between variables */
            if ( MSDPCommands[0] != '\0' )
              strlcat( MSDPCommands, " ", MAX_OUTPUT_BUFFER );

            /* Add the variable to the list */
            strlcat( MSDPCommands, VariableNameTable[i].pName, MAX_OUTPUT_BUFFER );
          }
        }

        MSDPSendList( apDescriptor, apValue, MSDPCommands );
      }
      else if ( MatchString(apValue, "CONFIGURABLE_VARIABLES") )
      {
        char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
        int i; /* Loop counter */

        for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
        {
          if ( VariableNameTable[i].bConfigurable )
          {
            /* Add the separator between variables */
            if ( MSDPCommands[0] != '\0' )
              strlcat( MSDPCommands, " ", MAX_OUTPUT_BUFFER );

            /* Add the variable to the list */
            strlcat( MSDPCommands, VariableNameTable[i].pName, MAX_OUTPUT_BUFFER );
          }
        }

        MSDPSendList( apDescriptor, "CONFIGURABLE_VARIABLES", MSDPCommands );
      }
      else if ( MatchString(apValue, "GUI_VARIABLES") )
      {
        char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
        int i; /* Loop counter */

        for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
        {
          if ( VariableNameTable[i].bGUI )
          {
            /* Add the separator between variables */
            if ( MSDPCommands[0] != '\0' )
              strlcat( MSDPCommands, " ", MAX_OUTPUT_BUFFER );

            /* Add the variable to the list */
            strlcat( MSDPCommands, VariableNameTable[i].pName, MAX_OUTPUT_BUFFER );
          }
        }

        MSDPSendList( apDescriptor, apValue, MSDPCommands );
      }
    }
    else /* Set any configurable variables */
    {
      int i; /* Loop counter */

      for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
      {
        if ( VariableNameTable[i].bConfigurable )
        {
          if ( MatchString(apVariable, VariableNameTable[i].pName) )
          {
            if ( VariableNameTable[i].bString )
            {
              /* A write-once variable can only be set if the value
               * is "Unknown".  This is for things like client name,
               * where we don't really want the player overwriting a
               * proper client name with junk - but on the other hand,
               * its possible a client may choose to use MSDP to
               * identify itself.
               */
              if ( !VariableNameTable[i].bWriteOnce ||
                !strcmp(apDescriptor->pProtocol->pVariables[i]->pValueString, "Unknown") )
              {
                /* Store the new value if it's valid */
                char *pBuffer = new char[VariableNameTable[i].Max+1];
                int j; /* Loop counter */

                for ( j = 0; j < VariableNameTable[i].Max && *apValue != '\0'; ++apValue )
                {
                  if ( isprint(*apValue) )
                    pBuffer[j++] = *apValue;
                }
                pBuffer[j++] = '\0';

                if ( j >= VariableNameTable[i].Min )
                {
                  delete [] apDescriptor->pProtocol->pVariables[i]->pValueString;
                  apDescriptor->pProtocol->pVariables[i]->pValueString = AllocString(pBuffer);
                }
              }
            }
            else /* This variable only accepts numeric values */
            {
              /* Strip any leading spaces */
              while ( *apValue == ' ' )
                ++apValue;

              if ( *apValue != '\0' && IsNumber(apValue) )
              {
                int Value = atoi(apValue);
                if ( Value >= VariableNameTable[i].Min &&
                  Value <= VariableNameTable[i].Max )
                {
                  apDescriptor->pProtocol->pVariables[i]->ValueInt = Value;
                }
              }
            }
          }
        }
      }
    }
  }
}

/******************************************************************************
 Local ATCP functions.
 ******************************************************************************/

static void ParseATCP( descriptor_t *apDescriptor, const char *apData )
{
  char Variable[MSDP_VAL][MAX_MSDP_SIZE+1] = { {'\0'}, {'\0'} };
  char *pPos = NULL, *pStart = NULL;

  while ( *apData )
  {
    switch ( *apData )
    {
      case '@':
        pPos = pStart = Variable[0];
        apData++;
        break;
      case ' ':
        pPos = pStart = Variable[1];
        apData++;
        break;
      default: /* Anything else */
        if ( pPos && pPos-pStart < MAX_MSDP_SIZE )
        {
          *pPos++ = *apData;
          *pPos = '\0';
        }

        if ( *++apData )
          continue;
    }

    ExecuteMSDPPair( apDescriptor, Variable[MSDP_VAR-1], Variable[MSDP_VAL-1] );
    Variable[MSDP_VAL-1][0] = '\0';
  }
}

#ifdef MUDLET_PACKAGE
static void SendATCP( descriptor_t *apDescriptor, const char *apVariable, const char *apValue )
{
  char ATCPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

  if ( apVariable != NULL && apValue != NULL )
  {
    protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

    /* Should really be replaced with a dynamic buffer */
    int RequiredBuffer = strlen(apVariable) + strlen(apValue) + 12;

    if ( RequiredBuffer >= MAX_VARIABLE_LENGTH )
    {
      if ( RequiredBuffer - strlen(apValue) < MAX_VARIABLE_LENGTH )
      {
        snprintf( ATCPBuffer, sizeof(ATCPBuffer),
          "SendATCP: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n",
          apVariable, RequiredBuffer, MAX_VARIABLE_LENGTH );
      }
      else /* The variable name itself is too long */
      {
        snprintf( ATCPBuffer, sizeof(ATCPBuffer),
          "SendATCP: Variable name has a length of %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n",
          RequiredBuffer, MAX_VARIABLE_LENGTH );
      }

      ReportBug( ATCPBuffer );
      ATCPBuffer[0] = '\0';
    }
    else if ( pProtocol->bATCP )
    {
      snprintf( ATCPBuffer, sizeof(ATCPBuffer), "%c%c%c%s %s%c%c",
        IAC, SB, TELOPT_ATCP, apVariable, apValue, IAC, SE );
    }

    /* Just in case someone calls this function without checking ATCP */
    if ( ATCPBuffer[0] != '\0' )
      Write( apDescriptor, ATCPBuffer );
  }
}
#endif /* MUDLET_PACKAGE */

/******************************************************************************
 Local MSSP functions.
 ******************************************************************************/

static const char *GetMSSP_Players()
{
  static char Buffer[32];
  snprintf( Buffer, sizeof(Buffer), "%d", s_Players );
  return Buffer;
}

static const char *GetMSSP_Uptime()
{
  static char Buffer[32];
  snprintf( Buffer, sizeof(Buffer), "%d", (int)s_Uptime );
  return Buffer;
}

/* Macro for readability, but you can remove it if you don't like it */
#define FUNCTION_CALL(f) "", f

static void SendMSSP( descriptor_t *apDescriptor )
{
  char MSSPBuffer[MAX_MSSP_BUFFER];
  char MSSPPair[128];
  int SizeBuffer = 3; /* IAC SB MSSP */
  int i; /* Loop counter */

  /* Before updating the following table, please read the MSSP specification:
   *
   * http://tintin.sourceforge.net/mssp/
   *
   * It's important that you use the correct format and spelling for the MSSP
   * variables, otherwise crawlers may reject the data as invalid.
   */
  static MSSP_t MSSPTable[] =
  {
    /* Required */
    { "NAME",          MUD_NAME, NULL },  /* Change this in protocol.h */
    { "PLAYERS",        FUNCTION_CALL( GetMSSP_Players ) },
    { "UPTIME" ,        FUNCTION_CALL( GetMSSP_Uptime ) },

    /* Generic */
    { "CRAWL DELAY",      "-1", NULL },

    { "HOSTNAME",        "awakemud.com", NULL },
    { "PORT",          "4000", NULL },
    { "CODEBASE",        *awakemud_version, NULL },
    { "CONTACT",        STAFF_CONTACT_EMAIL, NULL },
    { "CREATED",        "2016", NULL },
//    { "ICON",          "" },
//    { "IP",            "" },
    { "LANGUAGE",        "English", NULL },
    { "LOCATION",        "USA", NULL },
    { "MINIMUM AGE",      "13", NULL }, // COPPA compliance-- we do not service those under 13.
    { "WEBSITE",        "https://awakemud.com", NULL },
    /* Categorisation */

    { "FAMILY",         "Custom", NULL },
    { "GENRE",          "Science Fiction", NULL },
    { "GAMEPLAY",        "Player versus Environment", NULL },
    { "STATUS",         "Open Beta", NULL },
    { "GAMESYSTEM",      "Shadowrun 3rd Edition", NULL },
//    { "INTERMUD",        "Gossip", NULL },
    { "SUBGENRE",        "Cyberpunk", NULL },

    /* World */
/*
    { "AREAS",          "0" },
    { "HELPFILES",       "0" },
    { "MOBILES",        "0" },
    { "OBJECTS",        "0" },
    { "ROOMS",          "0" },
*/
    { "CLASSES",        "0", NULL }, // Indicates classless
    { "LEVELS",         "0", NULL }, // Indicates levelless
/*
    { "RACES",          "0" },
    { "SKILLS",         "0" },
*/
    /* Protocols */
    { "ANSI",          "1", NULL },
    { "GMCP",          "0", NULL },
#ifdef USING_MCCP
    { "MCCP",          "1", NULL },
#else
    { "MCCP",          "0", NULL },
#endif // USING_MCCP
    { "MCP",           "0", NULL },
    { "MSDP",          "1", NULL },
    { "MSP",           "1", NULL },
    { "MXP",           "1", NULL },
    { "PUEBLO",         "0", NULL },
    { "UTF-8",          "1", NULL },
    { "VT100",          "0", NULL },
    { "XTERM 256 COLORS",  "1", NULL },
    /* Commercial */

    { "PAY TO PLAY",      "0", NULL },
    { "PAY FOR PERKS",    "0", NULL },

    /* Hiring */

    { "HIRING BUILDERS",   "1", NULL },
    { "HIRING CODERS",    "1", NULL },

    /* Extended variables */

    /* World */
/*
    { "DBSIZE",         "0" },
    { "EXITS",          "0" },
    { "EXTRA DESCRIPTIONS", "0" },
    { "MUDPROGS",        "0" },
    { "MUDTRIGS",        "0" },
    { "RESETS",         "0" },
*/
    /* Game */

    { "ADULT MATERIAL",    "0", NULL },
    { "MULTICLASSING",    "0", NULL },
    { "NEWBIE FRIENDLY",   "1", NULL },
    { "PLAYER CITIES",    "0", NULL },
    { "PLAYER CLANS",     "1", NULL },
    { "PLAYER CRAFTING",   "1", NULL },
    { "PLAYER GUILDS",    "1", NULL },
    /*
    { "EQUIPMENT SYSTEM",  "" },
    { "MULTIPLAYING",     "" },
    { "PLAYERKILLING",    "" },
    { "QUEST SYSTEM",     "" },
    { "ROLEPLAYING",      "" },
    { "TRAINING SYSTEM",   "" },
    { "WORLD ORIGINALITY",  "" },
    */
    /* Protocols */
/*
    { "ATCP",          "1" },
    { "SSL",           "0" },
    { "ZMP",           "0" },
*/
    { NULL, NULL, NULL } /* This must always be last. */
  };

  /* Begin the subnegotiation sequence */
  snprintf( MSSPBuffer, sizeof(MSSPBuffer), "%c%c%c", IAC, SB, TELOPT_MSSP );

  for ( i = 0; MSSPTable[i].pName != NULL; ++i )
  {
    int SizePair;

    /* Retrieve the next MSSP variable/value pair */
    snprintf( MSSPPair, sizeof(MSSPPair), "%c%s%c%s", MSSP_VAR, MSSPTable[i].pName, MSSP_VAL,
      MSSPTable[i].pFunction ? (*MSSPTable[i].pFunction)() :
      MSSPTable[i].pValue );

    /* Make sure we don't overflow the buffer */
    SizePair = strlen(MSSPPair);
    if ( SizePair+SizeBuffer < MAX_MSSP_BUFFER-4 )
    {
      strlcat( MSSPBuffer, MSSPPair, MAX_MSSP_BUFFER );
      SizeBuffer += SizePair;
    }
  }

  /* End the subnegotiation sequence */
  snprintf( MSSPPair, sizeof(MSSPPair), "%c%c", IAC, SE );
  strlcat( MSSPBuffer, MSSPPair, MAX_MSSP_BUFFER );

  /* Send the sequence */
  Write( apDescriptor, MSSPBuffer );
}

/******************************************************************************
 Local MXP functions.
 ******************************************************************************/

static char *GetMxpTag( const char *apTag, const char *apText )
{
  static char MXPBuffer [64];
  const char *pStartPos = strstr(apText, apTag);

  if ( pStartPos != NULL )
  {
    const char *pEndPos = apText+strlen(apText);

    pStartPos += strlen(apTag); /* Add length of the tag */

    if ( pStartPos < pEndPos )
    {
      int Index = 0;

      /* Some clients use quotes...and some don't. */
      if ( *pStartPos == '\"' )
        pStartPos++;

      for ( ; pStartPos < pEndPos && Index < 60; ++pStartPos )
      {
        char Letter = *pStartPos;
        if ( Letter == '.' || isdigit(Letter) || isalpha(Letter) )
        {
          MXPBuffer[Index++] = Letter;
        }
        else /* Return the result */
        {
          MXPBuffer[Index] = '\0';
          return MXPBuffer;
        }
      }
    }
  }

  /* Return NULL to indicate no tag was found. */
  return NULL;
}

/******************************************************************************
 Local colour functions.
 ******************************************************************************/

static const char *GetAnsiColour( bool abBackground, int aRed, int aGreen, int aBlue )
{
  // Background colors.
  if (abBackground) {
    if (aRed == aGreen && aGreen == aBlue)
      return aRed == 5 ? s_BackWhite : s_BackBlack;
    if ( aRed > aGreen && aRed > aBlue )
      return s_BackRed;
    if ( aRed == aGreen && aRed > aBlue )
      return s_BackYellow;
    if ( aRed == aBlue && aRed > aGreen )
      return s_BackMagenta;
    if ( aGreen > aBlue )
      return s_BackGreen;
    if ( aGreen == aBlue )
      return s_BackCyan;
    /* aBlue is the highest */
    return s_BackBlue;
  }

  // Handle cases where R=G=B (greyscale). This takes care of Black and White.
  if ( aRed == aGreen && aGreen == aBlue) {
    switch (aRed) {
      case 5:
      case 4:
        return s_BoldWhite;
      case 3:
      case 2:
        return s_DarkWhite;
      case 1:
        return s_BoldBlack;
      case 0:
        return s_DarkBlack;
    }
  }

  // Pure RGB colors, AKA the easy cases.
  if (!aRed && !aGreen)
    return aBlue <= 3 ? s_DarkBlue : s_BoldBlue;
  if (!aRed && !aBlue)
    return aGreen <= 3 ? s_DarkGreen : s_BoldGreen;
  if (!aBlue && !aGreen)
    return aRed <= 3 ? s_DarkRed : s_BoldRed;

  // Yellow is when R and G are set equally with little blue.
  if (aRed == aGreen && aBlue < aRed)
    return aRed <= 3 ? s_DarkYellow : s_BoldYellow;
  // It's also when they're within 1 of each other.
  if ((aRed - 1 == aGreen || aGreen - 1 == aRed) && aBlue < MIN(aGreen, aRed))
    return MAX(aRed, aGreen) <= 3 ? s_DarkYellow : s_BoldYellow;
  // Also specifically match 554.
  if (aRed == 5 && aGreen == 5 && aBlue == 4)
    return s_BoldYellow;

  // Cyan is when G and B are set equally with little red.
  if (aGreen == aBlue && aRed < aGreen)
    return aGreen <= 3 ? s_DarkCyan : s_BoldCyan;
  // It's also when they're within 1 of each other.
  if ((aGreen - 1 == aBlue || aBlue - 1 == aGreen) && aRed < MIN(aGreen, aBlue))
    return MAX(aBlue, aGreen) <= 3 ? s_DarkCyan : s_BoldCyan;
  // Also specifically match 455.
  if (aRed == 4 && aGreen == 5 && aBlue == 5)
    return s_BoldCyan;

  // Magenta is when R and B are equal with little green.
  if (aRed == aBlue && aGreen < aRed)
    return aRed <= 3 ? s_DarkMagenta : s_BoldMagenta;
  // It's also when they're within 1 of each other.
  if ((aRed - 1 == aBlue || aBlue - 1 == aRed) && aRed < MIN(aRed, aBlue))
    return MAX(aBlue, aRed) <= 3 ? s_DarkMagenta : s_BoldMagenta;
  // Also specifically match 545.
  if (aRed == 5 && aGreen == 4 && aBlue == 5)
    return s_BoldMagenta;
    
  // Finally, we have the predominate color cases, where a single color is greater than the others.
  if (aRed > aGreen && aRed > aBlue)
    return aRed <= 3 ? s_DarkRed : s_BoldRed;

  if (aGreen > aRed && aGreen > aBlue)
    return aGreen <= 3 ? s_DarkGreen : s_BoldGreen;

  if (aBlue > aRed && aBlue > aGreen)
    return aBlue <= 3 ? s_DarkBlue : s_BoldBlue;

  // And we should never get here, but if we do...
  log_vfprintf("WARNING: Hit end of color conversion tree with non-handled RGB color %s%d%d%d. Using BoldWhite.", abBackground ? "B" : "F", aRed, aGreen, aBlue);
  return s_BoldWhite;
}

static const char *GetRGBColour( bool abBackground, int aRed, int aGreen, int aBlue )
{
  static char Result[16];
  int ColVal = 16 + (aRed * 36) + (aGreen * 6) + aBlue;
  snprintf( Result, sizeof(Result), "\033[%c8;5;%c%c%cm",
    '3'+abBackground,    /* Background */
    '0'+(ColVal/100),    /* Red      */
    '0'+((ColVal%100)/10), /* Green    */
    '0'+(ColVal%10) );    /* Blue     */
  return Result;
}

static bool IsValidColour( const char *apArgument )
{
  int i; /* Loop counter */

  /* The sequence is 4 bytes, but we can ignore anything after it. */
  if ( apArgument == NULL || strlen(apArgument) < 4 )
    return FALSE;

  /* The first byte indicates foreground/background. */
  if ( tolower(apArgument[0]) != 'f' && tolower(apArgument[0]) != 'b' )
    return FALSE;

  /* The remaining three bytes must each be in the range '0' to '5'. */
  for ( i = 1; i <= 3; ++i )
  {
    if ( apArgument[i] < '0' || apArgument[i] > '5' )
      return FALSE;
  }

  /* It's a valid foreground or background colour */
  return TRUE;
}

/******************************************************************************
 Other local functions.
 ******************************************************************************/

static bool MatchString( const char *apFirst, const char *apSecond )
{
  while ( *apFirst && tolower(*apFirst) == tolower(*apSecond) )
    ++apFirst, ++apSecond;
  return ( !*apFirst && !*apSecond );
}

static bool PrefixString( const char *apPart, const char *apWhole )
{
  while ( *apPart && tolower(*apPart) == tolower(*apWhole) )
    ++apPart, ++apWhole;
  return ( !*apPart );
}

static bool IsNumber( const char *apString )
{
  while ( *apString && isdigit(*apString) )
    ++apString;
  return ( !*apString );
}

static char *AllocString( const char *apString )
{
  char *pResult = NULL;

  if ( apString != NULL )
  {
    int Size = strlen(apString);
    const size_t pResultSize = Size+1;
    pResult = new char[pResultSize];
    if ( pResult != NULL )
      strlcpy( pResult, apString, pResultSize );
  }

  return pResult;
}
