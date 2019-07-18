/*
 * FreeRTOS+CLI V1.0.4 (C) 2014 Real Time Engineers ltd.  All rights reserved.
 *
 * This file is part of the FreeRTOS+CLI distribution.  The FreeRTOS+CLI license
 * terms are different to the FreeRTOS license terms.
 *
 * FreeRTOS+CLI uses a dual license model that allows the software to be used
 * under a standard GPL open source license, or a commercial license.  The
 * standard GPL license (unlike the modified GPL license under which FreeRTOS
 * itself is distributed) requires that all software statically linked with
 * FreeRTOS+CLI is also distributed under the same GPL V2 license terms.
 * Details of both license options follow:
 *
 * - Open source licensing -
 * FreeRTOS+CLI is a free download and may be used, modified, evaluated and
 * distributed without charge provided the user adheres to version two of the
 * GNU General Public License (GPL) and does not remove the copyright notice or
 * this text.  The GPL V2 text is available on the gnu.org web site, and on the
 * following URL: http://www.FreeRTOS.org/gpl-2.0.txt.
 *
 * - Commercial licensing -
 * Businesses and individuals that for commercial or other reasons cannot comply
 * with the terms of the GPL V2 license must obtain a low cost commercial
 * license before incorporating FreeRTOS+CLI into proprietary software for
 * distribution in any form.  Commercial licenses can be purchased from
 * http://shop.freertos.org/cli and do not require any source files to be
 * changed.
 *
 * FreeRTOS+CLI is distributed in the hope that it will be useful.  You cannot
 * use FreeRTOS+CLI unless you agree that you use the software 'as is'.
 * FreeRTOS+CLI is provided WITHOUT ANY WARRANTY; without even the implied
 * warranties of NON-INFRINGEMENT, MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. Real Time Engineers Ltd. disclaims all conditions and terms, be they
 * implied, expressed, or statutory.
 *
 * 1 tab == 4 spaces!
 *
 * http://www.FreeRTOS.org
 * http://www.FreeRTOS.org/FreeRTOS-Plus
 *
 */

/* Standard includes. */
#include <string.h>
#include <stdint.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Utils includes. */
#include "FreeRTOS_CLI.h"
#include "system.h"

/* If the application writer needs to place the buffer used by the CLI at a
fixed address then set configAPPLICATION_PROVIDES_cOutputBuffer to 1 in
FreeRTOSConfig.h, then declare an array with the following name and size in 
one of the application files:
    char cOutputBuffer[ config_MAX_OUTPUT_SIZE ];
*/
#ifndef configAPPLICATION_PROVIDES_cOutputBuffer
    #define configAPPLICATION_PROVIDES_cOutputBuffer 0
#endif

typedef struct xCOMMAND_INPUT_LIST
{
    const CLI_Command_Definition_t *pxCommandLineDefinition;
    struct xCOMMAND_INPUT_LIST *pxNext;
} CLI_Definition_List_Item_t;

/*
 * The callback function that is executed when "help" is entered.  This is the
 * only default command that is always present.
 */
static void cmd_help( char *chp,  int argc, const char *argv[] );

/*
 * Return the number of parameters that follow the command name.
 */
static int8_t prvGetNumberOfParameters( char *pcCommandString, const char *args[]);

/* The definition of the "help" command.  This command is always at the front
of the list of registered commands. */
static const CLI_Command_Definition_t x_cmd_help = {
"help", "lists all the registered commands\r\n", cmd_help, 0};

/* The definition of the list of commands.  Commands that are registered are
added to this list. */
static CLI_Definition_List_Item_t xRegisteredCommands =
{
    &x_cmd_help,  /* The first command in the list is always the help command, defined in this file. */
    NULL            /* The next pointer is initialised to NULL, as there are no other registered commands yet. */
};

/* A buffer into which command outputs can be written is declared here, rather
than in the command console implementation, to allow multiple command consoles
to share the same buffer.  For example, an application may allow access to the
command interpreter by UART and by Ethernet.  Sharing a buffer is done purely
to save RAM.  Note, however, that the command console itself is not re-entrant,
so only one command interpreter interface can be used at any one time.  For that
reason, no attempt at providing mutual exclusion to the cOutputBuffer array is
attempted.

configAPPLICATION_PROVIDES_cOutputBuffer is provided to allow the application
writer to provide their own cOutputBuffer declaration in cases where the
buffer needs to be placed at a fixed address (rather than by the linker). */
#if( configAPPLICATION_PROVIDES_cOutputBuffer == 0 )
    static char cOutputBuffer[ config_MAX_OUTPUT_SIZE ];
#else
    extern char cOutputBuffer[ config_MAX_OUTPUT_SIZE ];
#endif


/*-----------------------------------------------------------*/

BaseType_t FreeRTOS_CLIRegisterCommand( const CLI_Command_Definition_t * const pxCommandToRegister )
{
    static CLI_Definition_List_Item_t *pxLastCommandInList = &xRegisteredCommands;
    CLI_Definition_List_Item_t *pxNewListItem;
    BaseType_t xReturn = pdFAIL;

    /* Check the parameter is not NULL. */
    configASSERT( pxCommandToRegister );

    /* Create a new list item that will reference the command being registered. */
    pxNewListItem = ( CLI_Definition_List_Item_t * ) pvPortMalloc( sizeof( CLI_Definition_List_Item_t ) );
    configASSERT( pxNewListItem );

    if( pxNewListItem != NULL )
    {
        taskENTER_CRITICAL();
        {
            /* Reference the command being registered from the newly created
            list item. */
            pxNewListItem->pxCommandLineDefinition = pxCommandToRegister;

            /* The new list item will get added to the end of the list, so
            pxNext has nowhere to point. */
            pxNewListItem->pxNext = NULL;

            /* Add the newly created list item to the end of the already existing
            list. */
            pxLastCommandInList->pxNext = pxNewListItem;

            /* Set the end of list marker to the new list item. */
            pxLastCommandInList = pxNewListItem;
        }
        taskEXIT_CRITICAL();

        xReturn = pdPASS;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t FreeRTOS_CLIProcessCommand( char * pcCommandInput, char * pcWriteBuffer, size_t xWriteBufferLen  )
{
    static const CLI_Definition_List_Item_t *pxCommand = NULL;
    BaseType_t xReturn = pdTRUE;
    const char *pcRegisteredCommandString;
    size_t xCommandStringLength;
    static int n = 0;
    const char *args[10 + 1];

    /* Note:  This function is not re-entrant.  It must not be called from more
    thank one task. */

    if( pxCommand == NULL )
    {
        /* Search for the command string in the list of registered commands. */
        for( pxCommand = &xRegisteredCommands; pxCommand != NULL; pxCommand = pxCommand->pxNext )
        {
            pcRegisteredCommandString = pxCommand->pxCommandLineDefinition->pcCommand;
            xCommandStringLength = strlen( pcRegisteredCommandString );

            /* To ensure the string lengths match exactly, so as not to pick up
            a sub-string of a longer command, check the byte after the expected
            end of the string is either the end of the string or a space before
            a parameter. */
            if( ( pcCommandInput[ xCommandStringLength ] == ' ' ) || ( pcCommandInput[ xCommandStringLength ] == 0x00 ) )
            {
                if( strncmp( pcCommandInput, pcRegisteredCommandString, xCommandStringLength ) == 0 )
                {
                    /* The command has been found.  Check it has the expected
                    number of parameters.  If cExpectedNumberOfParameters is -1,
                    then there could be a variable number of parameters and no
                    check is made. */
                    if( pxCommand->pxCommandLineDefinition->cExpectedNumberOfParameters >= 0 )
                    {
                        n = prvGetNumberOfParameters( pcCommandInput, args);
                        if( n != pxCommand->pxCommandLineDefinition->cExpectedNumberOfParameters )
                        {
                            xReturn = pdFALSE; // 参数不匹配
                        }
                    } else {
                        n = prvGetNumberOfParameters( pcCommandInput, args);
                    }

                    break;
                }
            }
        }
    }

    if( ( pxCommand != NULL ) && ( xReturn == pdFALSE ) ) /* 命令找到，但命令参数不匹配 */
    {
        /* The command was found, but the number of parameters with the command
        was incorrect. */
        #if 0
        strncpy( pcWriteBuffer, "Incorrect command parameter(s).\r\n", xWriteBufferLen );
        #endif
        dbprintf("Incorrect command parameter(s).\r\n");
        pxCommand = NULL;
    }
    else if( pxCommand != NULL ) /* 命令执行 */
    {
        /* Call the callback function that is registered to this command. */
        /* 传入参数个数和参数指针 */
        pxCommand->pxCommandLineDefinition->pxCommandInterpreter( pcWriteBuffer, n, args);
        xReturn = pdFALSE;

        /* If xReturn is pdFALSE, then no further strings will be returned
        after this one, and    pxCommand can be reset to NULL ready to search
        for the next entered command. */
        if( xReturn == pdFALSE )  /* 修改后所有命令都只执行一次 */
        {
            pxCommand = NULL;
        }
    }
    else /* 命令没找到 */
    {
        /* pxCommand was NULL, the command was not found. */
        #if 0
        strncpy( pcWriteBuffer, "Command not recognised.\r\n", xWriteBufferLen );
        #endif
        dbprintf("Command not recognised.\r\n");
        xReturn = pdFALSE;
    }

    return xReturn; // 总是返回 pdFALSE
}
/*-----------------------------------------------------------*/

char *FreeRTOS_CLIGetOutputBuffer( void )
{
    return cOutputBuffer;
}
/*-----------------------------------------------------------*/

const char *FreeRTOS_CLIGetParameter( const char *pcCommandString, UBaseType_t uxWantedParameter, BaseType_t *pxParameterStringLength )
{
    UBaseType_t uxParametersFound = 0;
    const char *pcReturn = NULL;

    *pxParameterStringLength = 0;

    while( uxParametersFound < uxWantedParameter )
    {
        /* Index the character pointer past the current word.  If this is the start
        of the command string then the first word is the command itself. */
        while( ( ( *pcCommandString ) != 0x00 ) && ( ( *pcCommandString ) != ' ' ) )
        {
            pcCommandString++;
        }

        /* Find the start of the next string. */
        while( ( ( *pcCommandString ) != 0x00 ) && ( ( *pcCommandString ) == ' ' ) )
        {
            pcCommandString++;
        }

        /* Was a string found? */
        if( *pcCommandString != 0x00 )
        {
            /* Is this the start of the required parameter? */
            uxParametersFound++;

            if( uxParametersFound == uxWantedParameter )
            {
                /* How long is the parameter? */
                pcReturn = pcCommandString;
                while( ( ( *pcCommandString ) != 0x00 ) && ( ( *pcCommandString ) != ' ' ) )
                {
                    ( *pxParameterStringLength )++;
                    pcCommandString++;
                }

                if( *pxParameterStringLength == 0 )
                {
                    pcReturn = NULL;
                }

                break;
            }
        }
        else
        {
            break;
        }
    }

    return pcReturn;
}
/*-----------------------------------------------------------*/

static void cmd_help( char *chp, int argc, const char *argv[])
{
    const CLI_Definition_List_Item_t * pxCommand = NULL;
    int i, space;

    (void)chp;
    // (void)argc;
    // (void)argv;

    pxCommand = &xRegisteredCommands;
    dbprintf("There are all commands\r\n");

    while (pxCommand != NULL)
    {
        /* Return the next command help string, before moving the pointer on to
        the next command in the list. */
        #if 0
        strcpy( chp, pxCommand->pxCommandLineDefinition->pcHelpString);
        #endif
        dbprintf("%s:", pxCommand->pxCommandLineDefinition->pcCommand);
        space = 20 - strlen(pxCommand->pxCommandLineDefinition->pcCommand);
        for (i = 0; i<space; i++ ) {
            dbprintf(" ");
        }
        dbprintf("%s", pxCommand->pxCommandLineDefinition->pcHelpString);
        pxCommand = pxCommand->pxNext;
    }

    #if 0
    dbprintf("\r\ntotal %d args\r\n", argc);
    for (i = 0; i<argc; i++ ) {
        dbprintf("%s\r\n", argv[i]);
    }
    #endif
}
/*-----------------------------------------------------------*/

static int8_t prvGetNumberOfParameters( char *pcCommandString, const char *args[])
{
    int8_t cParameters = 0;
    BaseType_t xLastCharacterWasSpace = pdFALSE;

    /* Count the number of space delimited words in pcCommandString. */
    while( *pcCommandString != 0x00 )
    {
        if( ( *pcCommandString ) == ' ' )
        {
            if( xLastCharacterWasSpace != pdTRUE )
            {
                *pcCommandString = 0;
                args[cParameters++] = pcCommandString+1; /* 存储参数的地址 */
                xLastCharacterWasSpace = pdTRUE;
            }
        }
        else
        {
            xLastCharacterWasSpace = pdFALSE;
        }

        pcCommandString++;
    }

    /* If the command string ended with spaces, then there will have been too
    many parameters counted. */
    if( xLastCharacterWasSpace == pdTRUE )
    {
        cParameters--;
    }

    /* The value returned is one less than the number of space delimited words,
    as the first word should be the command itself. */
    return cParameters;
}
/*-----------------------------------------------------------*/
