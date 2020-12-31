
/*---------------------------------------------------------------------------*/
/* PSPBoot 0.2 Created by Jackson Mo */
/*---------------------------------------------------------------------------*/
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <stdio.h>
#include <string.h>
#include "zlib.h"


/*---------------------------------------------------------------------------*/
/* Module info                                                               */
/*---------------------------------------------------------------------------*/
/* Define the module info section, note the 0x1000 flag to enable start in
 * kernel mode
 */
PSP_MODULE_INFO( "PSPBoot", 0, 1, 1 );

/* Define the thread attribute as 0 so that the main thread does not get
 * converted to user mode
 */
PSP_MAIN_THREAD_ATTR( PSP_THREAD_ATTR_USER );


PSP_HEAP_SIZE_KB(12*1024); //Should be heaps!

/*---------------------------------------------------------------------------*/
/* Macros                                                                    */
/*---------------------------------------------------------------------------*/
#if 1
#define CONFIG_UART3
#endif

#define BOOL                int
#define TRUE                1
#define FALSE               0

#define KERNEL_ENTRY        0x88000000
#define KERNEL_PARAM_OFFSET 0x00000008
#define KERNEL_MAX_SIZE     ( 4 * 1024 * 1024 )   /* 4M */

#ifdef CONFIG_UART3
#define UART3_RXBUF         ( *(volatile char *)0xbe500000 )
#define UART3_TXBUF         ( *(volatile char *)0xbe500000 )
#define UART3_STATUS        ( *(volatile unsigned int *)0xbe500018 )
#define UART3_DIV1          ( *(volatile unsigned int *)0xbe500024 )
#define UART3_DIV2          ( *(volatile unsigned int *)0xbe500028 )
#define UART3_CTRL          ( *(volatile unsigned int *)0xbe50002c )
#define UART3_MASK_RXEMPTY  ( (unsigned int)0x00000010 )
#define UART3_MASK_TXFULL   ( (unsigned int)0x00000020 )
#define UART3_MASK_SETBAUD  ( (unsigned int)0x00000060 )
#endif

#define BANNER              "=====================================\n" \
                            " PSP Boot v0.2 Created by Jackson Mo\n"  \
                            "=====================================\n" \
                            " PRX Port by danzel.\n"  \
                            "=====================================\n\n" \
                            
#define BLUE                0x00ff0000
#define GREEN               0x0000ff00
#define RED                 0x000000ff
#define CONFIG_FILE         "pspboot.conf"
#define CONFIG_MAX_PARAM    256
#define CONFIG_PARAM_CMD    "cmdline"
#define CONFIG_PARAM_KERNEL "kernel"
#define CONFIG_PARAM_BAUD   "baud"

#define printf              pspDebugScreenPrintf
#define sleep(t)            sceKernelDelayThread( (t) )
#define exit()              transferControl( NULL, 0, s_paramCmd )
#define END_OF_LINE(p)      ( *(p) == '\r' || *(p) == '\n' || *(p) == 0 )




/*---------------------------------------------------------------------------*/
/* Static Data                                                               */
/*---------------------------------------------------------------------------*/
static char s_paramCmd[ CONFIG_MAX_PARAM ];
static char s_paramKernel[ CONFIG_MAX_PARAM ];
#ifdef CONFIG_UART3
static int  s_paramBaud = 1;
#endif


/*---------------------------------------------------------------------------*/
/* Prototypes                                                                */
/*---------------------------------------------------------------------------*/
BOOL loadConfig();
BOOL parseParam(const char * name_, const char * value_);
BOOL loadKernel(void ** buf_, int * size_);

//Moved to seperate kernel mode file
void transferControl(void * buf_, int size_, char* s_paramCmd);
//void disableIrq();
//void memCopy(void * dest_, const void * sour_, int size_);
//void pspClearIcache(void);
//void pspClearDcache(void);

//New
void LoadKernelModule();

#ifdef CONFIG_UART3
BOOL uart3_setbaud(int baud_);
int uart3_write(const char * buf_, int size_);
int uart3_puts(const char * str_);
#endif


/*---------------------------------------------------------------------------*/
/* Implementations                                                           */
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
	void * kernelBuf;
	int kernelSize;

	pspDebugScreenInit();
 
	LoadKernelModule();
	
	/* Print the banner */
	pspDebugScreenSetTextColor( RED );
	printf( BANNER );
	pspDebugScreenSetTextColor( GREEN );

	/* Load the config file first */
	if ( !loadConfig() )
	{
		sleep( 3000 );
		exit();
	}

	/* Load the kernel image */
	if ( !loadKernel( &kernelBuf, &kernelSize ) )
	{
		sleep( 3000 );
		exit();
	}

	transferControl( kernelBuf, kernelSize, s_paramCmd );   /* no return from here */
	return 0;
}
/*---------------------------------------------------------------------------*/
void LoadKernelModule()
{
	SceUID mod = pspSdkLoadStartModule("kernelboot.prx", PSP_MEMORY_PARTITION_KERNEL);

	if (mod < 0)
	{
		printf("Error 0x%08X loading/starting kernelboot.prx.\n", mod);
	}
	else
	{
		printf("Successfully Loaded kernelboot.prx\n");
	}
}

/*---------------------------------------------------------------------------*/
BOOL loadConfig()
{
	FILE * fconf;
	const int bufLen = 1024;
	char buf[ bufLen ];
	char * p;
	char * end;
	char * paramName;
	char * paramValue;
	int line;

	printf( "Loading config file\n" );

	fconf = fopen( CONFIG_FILE, "r" );
	if ( fconf == NULL )
	{
		printf( "Failed to open config file %s\n", CONFIG_FILE );
		return FALSE;
	}

	for ( line = 1; fgets( buf, bufLen, fconf ) != NULL; line++ )
	{
		buf[ bufLen - 1 ] = 0;
		p = buf;
		end = buf + bufLen;

		/* skip all leading white space of tab */
		while ( ( *p == ' ' || *p == '\t' ) && *p != 0 && p < end )
		{
			p++;
		}

		/* skip empty line and comment */
		if ( END_OF_LINE( p ) || p >= end || *p == '#' )
		{
			continue;
		}

		paramName = p;    /* get the parameter name */

		while ( *p != '=' && !END_OF_LINE( p ) && p < end ) p++;
		if ( END_OF_LINE( p ) || p >= end )
		{
			printf( "Syntax error at line %d\n", line );
			fclose( fconf );
			return FALSE;
		}

						*p++ = 0;           /* set the end of the param name */
						paramValue = p;     /* get the parameter value */

						/* set the end of the param value */
						while ( !END_OF_LINE( p ) && p < end ) p++;
						*p = 0;

						/* Parse the parameter */
						if ( !parseParam( paramName, paramValue ) )
						{
							fclose( fconf );
							return FALSE;
						}
 
						line++;
	}

	fclose( fconf );
	return TRUE;
}
/*---------------------------------------------------------------------------*/
BOOL parseParam(const char * name_, const char * value_)
{
	/* cmdline=... */
	if ( stricmp( name_, CONFIG_PARAM_CMD ) == 0 )
	{
		strncpy( s_paramCmd, value_, CONFIG_MAX_PARAM );
		s_paramCmd[ CONFIG_MAX_PARAM - 1 ] = 0;
		printf( "  %s: %s\n", CONFIG_PARAM_CMD, s_paramCmd );
	}

	/* kernel=... */
	else if ( stricmp( name_, CONFIG_PARAM_KERNEL ) == 0 )
	{
		strncpy( s_paramKernel, value_, CONFIG_MAX_PARAM );
		s_paramKernel[ CONFIG_MAX_PARAM - 1 ] = 0;
		printf( "  %s: %s\n", CONFIG_PARAM_KERNEL, s_paramKernel );
	}

#ifdef CONFIG_UART3
	/* baud=9600|115200 */
	else if ( stricmp( name_, CONFIG_PARAM_BAUD ) == 0 )
	{
		sscanf( value_, "%d", &s_paramBaud );
		printf( "  %s: %d\n", CONFIG_PARAM_BAUD, s_paramBaud );
	}
#endif

	else
	{
		printf( "  Unsupported param %s\n", name_ );
	}

	return TRUE;
}
/*---------------------------------------------------------------------------*/
BOOL loadKernel(void ** buf_, int * size_)
{
	gzFile zf;
	void * buf;
	int size;

	printf( "Decompressing linux kernel..." );

	if ( buf_ == NULL || size_ == NULL )
	{
		printf( "Internal error\n" );
		return FALSE;
	}

	*buf_ = NULL;
	*size_ = 0;

	if ( *s_paramKernel == 0 )
	{
		printf( "Invalid kernel file\n" );
		return FALSE;
	}

	zf = gzopen( s_paramKernel, "r" );
	if ( zf == NULL )
	{
		printf( "Failed to open file %s\n", s_paramKernel );
		return FALSE;
	}

	buf = malloc( KERNEL_MAX_SIZE );
	if ( buf == NULL )
	{
		printf( "Failed to allocate buffer\n" );
		gzclose( zf );
		return FALSE;
	}

	size = gzread( zf, buf, KERNEL_MAX_SIZE );
	if ( size < 0 )
	{
		printf( "Failed to read file\n" );
		free( buf );
		gzclose( zf );
		return FALSE;
	}

	gzclose( zf );
	printf( "%d bytes loaded\n", size );
 
	*buf_ = buf;
	*size_ = size;

	return TRUE;
}
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_UART3
BOOL uart3_setbaud(int baud_)
{
	int div1, div2;

	div1 = 96000000 / baud_;
	div2 = div1 & 0x3F;
	div1 >>= 6;

	UART3_DIV1 = div1;
	UART3_DIV2 = div2;
	UART3_CTRL = UART3_MASK_SETBAUD;

	return TRUE;
}
/*---------------------------------------------------------------------------*/
int uart3_write(const char * buf_, int size_)
{
	int bytesWritten = 0;
	while ( bytesWritten < size_ )
	{
		if ( *buf_ == '\n' )
		{
			while ( UART3_STATUS & UART3_MASK_TXFULL );
			UART3_TXBUF = '\n';
			while ( UART3_STATUS & UART3_MASK_TXFULL );
			UART3_TXBUF = '\r';
		}
		else
		{
			while ( UART3_STATUS & UART3_MASK_TXFULL );
			UART3_TXBUF = *buf_;
		}

		++buf_;
		++bytesWritten;
	}
 
	return bytesWritten;
}
/*---------------------------------------------------------------------------*/
int uart3_puts(const char * str_)
{
	int len = 0;
	while ( str_[ len ] != 0 ) ++len;

	return uart3_write( str_, len );
}
#endif


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/ 