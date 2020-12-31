#include <pspkernel.h>
#include <pspsdk.h> 
#include <pspctrl.h>

PSP_MODULE_INFO("KernelBoot", 0x1006, 1, 1);
PSP_MAIN_THREAD_ATTR(0);

#define KERNEL_ENTRY        0x88000000
#define KERNEL_PARAM_OFFSET 0x00000008

#define CONFIG_MAX_PARAM    256



void transferControl(void * buf_, int size_, char* s_paramCmd);
void disableIrq();
void memCopy(void * dest_, const void * sour_, int size_);
void pspClearIcache(void);
void pspClearDcache(void);



/*---------------------------------------------------------------------------*/
/* Type definitions                                                          */
/*---------------------------------------------------------------------------*/
typedef void ( *KernelEntryFunc )(int zero_, int arch_, void * params_);


/*---------------------------------------------------------------------------*/
/* Generic module bits                                                       */
/*---------------------------------------------------------------------------*/
int module_start(SceSize args, void *argp)
{
	return 0;
}

int module_stop()
{
	return 0;
} 

/*---------------------------------------------------------------------------*/
void transferControl(void * buf_, int size_, char* s_paramCmd)
{
	u32 k1;
	k1 = pspSdkSetK1(0);
	
	if ( buf_ != NULL && size_ > 0 )
	{
		KernelEntryFunc kernelEntry = (KernelEntryFunc)( KERNEL_ENTRY );
		void * kernelParam = NULL;

		//printf( "Transfering control to 0x%08x...\n", kernelEntry );

		/* disable IRQ */
		disableIrq();

		/* prepare kernel image */
		memCopy( (void *)( KERNEL_ENTRY ), buf_, size_ );

		/* prepare boot command line */
		if ( *s_paramCmd != 0 )
		{
			kernelParam = (void *)( KERNEL_ENTRY + KERNEL_PARAM_OFFSET );
			memCopy( kernelParam, s_paramCmd, CONFIG_MAX_PARAM );
		}

#ifdef CONFIG_UART3
		if ( s_paramBaud > 1 )
		{
			uart3_setbaud( s_paramBaud );
			uart3_puts( "Booting Linux kernel...\n" );
		}
#endif
   
		/* flush all caches */
		pspClearIcache();
		pspClearDcache();

		kernelEntry( 0, 0, kernelParam );
		/* never returns */
	}

	pspSdkSetK1(k1);
	
	//Need to use atleast one import otherwise you get the build error:
	// psp-fixup-imports kernelboot.elf
	// Error, no .lib.stub section found
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	
	//sceKernelExitGame();
}
/*---------------------------------------------------------------------------*/
void disableIrq()
{
	__asm__ __volatile__ (
						  "mfc0   $8, $12\n"
			"li     $9, 0xfffe\n"
			"and    $8, $8, $9\n"
			"mtc0   $8, $12\n"
						 );
}
/*---------------------------------------------------------------------------*/
void memCopy(void * dest_, const void * sour_, int size_)
{
	const char * from = (const char *)sour_;
	char * to = (char *)dest_;

	while ( size_-- > 0 )
	{
		*to++ = *from++;
	}
}
/*---------------------------------------------------------------------------*/
void pspClearIcache(void)
{
	asm("\
			.word 0x40088000;\
			.word 0x24091000;\
			.word 0x7D081240;\
			.word 0x01094804;\
			.word 0x4080E000;\
			.word 0x4080E800;\
			.word 0x00004021;\
			.word 0xBD010000;\
			.word 0xBD030000;\
			.word 0x25080040;\
			.word 0x1509FFFC;\
			.word 0x00000000;\
			"::);
}
/*---------------------------------------------------------------------------*/
void pspClearDcache(void)
{
	asm("\
			.word 0x40088000;\
			.word 0x24090800;\
			.word 0x7D081180;\
			.word 0x01094804;\
			.word 0x00004021;\
			.word 0xBD140000;\
			.word 0xBD140000;\
			.word 0x25080040;\
			.word 0x1509FFFC;\
			.word 0x00000000;\
			.word 0x0000000F;\
			.word 0x00000000;\
			"::);
}
