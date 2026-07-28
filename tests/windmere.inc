; WINDMERE.INC
;
; Copyright (c) 1998-1999 Symbian Ltd.  All rights reserved.
;

;
; Windermere hardware addresses and constants
;***************************************************************************************
;


; Use DebugBootLoaderDramList to display the contents of the DRAM block
; list that the bootloader passes us in professional mode.
		GBLL	DebugBootLoaderDramListPhase1
DebugBootLoaderDramListPhase1   SETL	{FALSE}
		GBLL	DebugBootLoaderDramListPhase2
DebugBootLoaderDramListPhase2   SETL	{FALSE}
		GBLL	DebugMmuPageTables
DebugMmuPageTables   SETL	{FALSE}

; Physical addresses

WndmRomBank0Address		EQU 0x00000000			; CS0 ROM
WndmRomBank1Address		EQU 0x10000000			; CS1 ROM
WndmBaseAddress		    EQU	0x80000000			; physical address of Windermere registers
WndmRamBank0Address		EQU 0xC0000000			; DRAM bank 0
WndmRamBank1Address		EQU 0xD0000000			; DRAM bank 1
WndmRamBank2Address		EQU 0xE0000000			; DRAM bank 2
WndmRamBank3Address		EQU 0xF0000000			; DRAM bank 3

; Static memory controller module
WndmMemcfg1				EQU 0x000				; Memory configuration 1
WndmMemcfg2				EQU 0x001				; Memory configuration 2

; MEMCFG register bits
WndmMcfgBw32			EQU 0x00				; 32-bit bus width
WndmMcfgBw16			EQU 0x01				; 16-bit bus width
WndmMcfgBw8				EQU 0x02				; 8-bit bus width
WndmMcfgBwPcmcia		EQU 0x03				; PCMCIA bus mode
WndmMcfgAc225			EQU 0x00				; 225ns access time @ 36MHz
WndmMcfgAc200			EQU 0x04				; 200ns access time @ 36MHz
WndmMcfgAc175			EQU 0x08				; 175ns access time @ 36MHz
WndmMcfgAc150			EQU 0x0c				; 150ns access time @ 36MHz
WndmMcfgAc125			EQU 0x10				; 125ns access time @ 36MHz
WndmMcfgAc100			EQU 0x14				; 100ns access time @ 36MHz
WndmMcfgAc75			EQU 0x18				; 75ns access time @ 36MHz
WndmMcfgAc50			EQU 0x1c				; 50ns access time @ 36MHz
WndmMcfgSac40			EQU 0x00				; 40ns sequential access time @ 36MHz
WndmMcfgSac20			EQU 0x20				; 20ns sequential access time @ 36MHz
WndmMcfgSqaen			EQU 0x40				; set to enable sequential accesses
WndmMcfgClken			EQU 0x80				; set to enable EXPCLK for this chip select

; DRAM controller module
WndmDramControl			EQU 0x100				; DRAM control register

; DRAM control register bits
WndmRfdivMask			EQU 0x007f				; 7-bit refresh clock divide ratio
WndmRfshen				EQU 0x0080				; set to enable DRAM refresh
WndmDram0width			EQU 0x0100				; set for 16-bit wide DRAM on bank 0
WndmDram1width			EQU 0x0200				; set for 16-bit wide DRAM on bank 1
WndmDram2width			EQU 0x0400				; set for 16-bit wide DRAM on bank 2
WndmDram3width			EQU 0x0800				; set for 16-bit wide DRAM on bank 3
WndmTrpctrl				EQU 0x1000				; set to reduce RAS precharge time by 1 clock

; LCD controller module
WndmLcdCtrl				EQU 0x200				; LCD control register
WndmLcdStatus			EQU	0x204				; LCD status register
WndmDbar1				EQU 0x210				; DMA channel 1 base address register
WndmDcar1				EQU 0x214				; DMA channel 1 current address register
WndmDbar2				EQU 0x218				; DMA channel 2 base address register
WndmDcar2				EQU 0x21c				; DMA channel 2 current address register
WndmLcdTim0				EQU 0x220				; LCD timing 0 register
WndmLcdTim1				EQU 0x224				; LCD timing 1 register
WndmLcdTim2				EQU 0x228				; LCD timing 2 register
WndmLcdPalette			EQU 0x1000				; LCD Palette RAM

; LCD control register bits
WndmLcdEn				EQU 0x00000001			; set to enable LCD controller
WndmLcdBW				EQU 0x00000002			; 1=monochrome 0=colour mode
WndmLcdDP				EQU 0x00000004			; set for for dual-panel mode
WndmLcdDoneMask			EQU 0x00000008			; 0=mask frame done bit in LCDSR
WndmLcdNextMask			EQU 0x00000010			; 0=mask next frame bit in LCDSR
WndmLcdErrMask			EQU 0x00000020			; 0=mask bus error status bit in LCDSR
WndmLcdTft				EQU 0x00000080			; 0=passive STN mode, 1=active TFT mode
WndmLcdM8b				EQU 0x00000200			; set for Mono 8-bit mode
WndmLcdFddMask			EQU 0x000ff000			; 8-bit FIFO DMA request delay value

; LCD status register bits
WndmLcdDone				EQU 0x01				; Frame Done
WndmLcdNext				EQU 0x02				; Frame Next
WndmLcdBerr				EQU 0x04				; LCD DMA bus error
WndmLcdAcbcs			EQU 0x08				; AC bias count status
WndmLcdFuf				EQU 0x20				; FIFO underflow


; Power management/State controller module
WndmPwrsr				EQU 0x400				; Power controller state
WndmPwrcnt				EQU 0x404				; Clock/debug control status
WndmHalt				EQU 0x408				; Write to enter idle mode
WndmStby				EQU 0x40c				; Write to enter standby mode
WndmBleoi				EQU 0x410				; Write to clear BLINT
WndmMceoi				EQU 0x414				; Write to clear MCINT
WndmTeoi				EQU 0x418				; Write to clear TINT
WndmStfclr				EQU 0x41C				; Write to clear startup type flags
WndmE2eoi				EQU 0x420				; Write to clear EINT2

; PWRSR bits
WndmRtcDivMask			EQU 0x003f				; lower 6 bits give tick number
WndmMcdr				EQU 0x0040				; non-latched media change state
WndmDcdet				EQU 0x0080				; set if external power present
WndmWudr				EQU 0x0100				; non-latched wake up input state
WndmWuon				EQU 0x0200				; set if wake up due to wakeup signal
WndmNbflg				EQU 0x0400				; set if batteries changed (NBATCHG)
WndmRstflg				EQU 0x0800				; set if user reset has occurred (NURESET)
WndmPfflg				EQU 0x1000				; set if power-fail reset has occurred
WndmCldflg				EQU 0x2000				; set if power-on reset has occurred (NPOR)
WndmVeridMask			EQU 0xc000				; 2 bits for version ID

; PWRCNT bits
WndmExcken				EQU 0x0001				; set to enable EXPCLK continuously
WndmWakedis				EQU 0x0002				; set to disable wakeup input
WndmClkflg				EQU 0x0004				; 0=18MHz core clock, 1=36MHz core clock
WndmAdcclkMask			EQU 0xff00				; division ratio for ADCCLK


; Interrupt controller module
WndmIntsr				EQU 0x500				; Interrupt status register
WndmIntrsr				EQU 0x504				; Interrupt raw status register
WndmIntMask				EQU 0x508				; Read interrupt mask
WndmIntEnb				EQU 0x508				; Write 1 to enable interrupt
WndmIntDis				EQU 0x50c				; Write 1 to disable interrupt
WndmIntTest1			EQU 0x514				;
WndmIntTest2			EQU 0x518				;

; Bit allocations for interrupt mask
WndmExtFiq				EQU 0x0001				; External FIQ
WndmBlint				EQU 0x0002				; Battery low FIQ
WndmWeint				EQU 0x0004				; Watchdog FIQ
WndmMcint				EQU 0x0008				; Media change FIQ
WndmCsint				EQU 0x0010				; Codec sound IRQ
WndmEint1				EQU 0x0020				; External IRQ 1
WndmEint2				EQU 0x0040				; External IRQ 2
WndmEint3				EQU 0x0080				; External IRQ 3
WndmTc1oi				EQU 0x0100				; Timer 1 IRQ
WndmTc2oi				EQU 0x0200				; Timer 2 IRQ
WndmRtcmi				EQU 0x0400				; RTC match IRQ
WndmTint				EQU 0x0800				; 64Hz tick IRQ
WndmUart1int			EQU 0x1000				; UART 1 IRQ
WndmUart2int			EQU	0x2000				; UART 2 IRQ
WndmLcdint				EQU 0x4000				; LCD controller IRQ
WndmSseoti				EQU 0x8000				; Synchronous serial interface IRQ

; UART1 (IRDA UART)
WndmUart1Base			EQU 0x600

; UART2 (COM port UART)
WndmUart2Base			EQU 0x700

; Register definitions for both UARTs
WndmUartdr				EQU 0x00				; UART data register
WndmUartfcr				EQU 0x04				; UART frame control register
WndmUbrcr				EQU 0x08				; UART Baud rate control register
WndmUartcon				EQU 0x0c				; UART control register
WndmUartflg				EQU 0x10				; UART flag register
WndmUartint				EQU 0x14				; UART raw interrupt status
WndmUartintm			EQU 0x18				; UART interrupt mask
WndmUartintr			EQU 0x1c				; UART masked interrupt status

; UARTFCR bits
WndmBreak				EQU 0x01				; set to generate break
WndmPrten				EQU 0x02				; set to enable parity
WndmEvenprt				EQU 0x04				; 1=even parity, 0=odd parity
WndmXstop				EQU 0x08				; 1 to transmit extra stop bit
WndmUfifoen				EQU 0x10				; set to enable FIFO
WndmWrdlen5				EQU 0x00				; 5 data bits
WndmWrdlen6				EQU 0x20				; 6 data bits
WndmWrdlen7				EQU 0x40				; 7 data bits
WndmWrdlen8				EQU 0x60				; 8 data bits

; UBRCR bits
WndmBaud115200			EQU 3

; UARTCON bits
WndmUarten				EQU 0x1					; set to enable UART
WndmSiren				EQU 0x2					; set to enable SIR (UART1 only)
WndmIrtxm				EQU 0x4					; set to reduce IR pulse width (UART1 only)

; UARTFLG bits
WndmFlgCts				EQU 0x01				; CTS state (UART2 only)
WndmFlgDsr				EQU 0x02				; DSR state (UART2 only)
WndmFlgDcd				EQU 0x04				; DCD state (UART2 only)
WndmFlgUbusy			EQU 0x08				; set while TX in progress
WndmFlgUrxfe			EQU 0x10				; set when RX FIFO/holding register empty
WndmFlgUtxff			EQU 0x20				; set when TX FIFO/holding register full

; UARTINT, UARTINTM, UARTINTR bits
WndmUartRxint			EQU 0x1					; RX interrupt
WndmUartTxint			EQU 0x2					; TX interrupt
WndmUartMsint			EQU 0x4					; Modem status change interrupt (UART2 only)

; DC/DC converter module
WndmPmpcon				EQU 0x900				; DC/DC converter pump control register

; Sound CODEC interface module
WndmCodr				EQU 0xa00				; CODEC data register
WndmCocfg				EQU 0xa04				; CODEC configuration register
WndmCoflg				EQU 0xa08				; CODEC flags register
WndmCoeoi				EQU 0xa0c				; write to clear CODEC interrupt

; CODEC config bits
WndmCodouten			EQU 0x1					; set to enable CODEC output
WndmCodinen				EQU 0x2					; set to enable CODEC input

; CODEC flag bits
WndmCrxfe				EQU 0x1					; set when CODEC RX FIFO empty
WndmCtxff				EQU 0x2					; set when CODEC TX FIFO full

; Synchronous serial interface module
WndmSscr0				EQU 0xb00				; SSI control register 0
WndmSscr1				EQU 0xb04				; SSI control register 1
WndmSsdr				EQU 0xb0c				; SSI data register
WndmSssr				EQU 0xb14				; SSI status register

; SSI control register 0 bits
WndmDssMask				EQU 0x000f				; Data size select 4-16 bits (write n-1)
WndmFrfMotSpi			EQU 0x0000				; Motorola SPI frame format
WndmFrfTI				EQU 0x0010				; TI synchronous serial frame format
WndmFrfNatMwr			EQU 0x0020				; National Microwire frame format
WndmSse					EQU 0x0080				; set to enable SSI
WndmSsiScrMask			EQU 0xff00				; Serial clock rate divisor

; SSI control register 1 bits
WndmSsiRim				EQU 0x01				; set to enable receive interrupt
WndmSsiTim				EQU 0x02				; set to enable transmit interrupt
WndmSsiLbm				EQU 0x04				; Set to enable loopback mode
WndmSsiSpo				EQU 0x08				; SPI mode SCLKSS polarity
WndmSsiSph				EQU 0x10				; SPI mode SCLKSS phase
WndmSsiFen				EQU 0x40				; set to enable FIFOs

; SSI status register bits
WndmSsiTnf				EQU 0x02				; set if TX FIFO not full
WndmSsiRne				EQU 0x04				; set if RX FIFO not empty
WndmSsiBsy				EQU 0x08				; set if SSI currently TXing or RXing
WndmSsiTfs				EQU 0x10				; set if TX FIFO  <= half full
WndmSsiRfs				EQU 0x20				; set if RX FIFO has >=8 full entries
WndmSsiRor				EQU 0x40				; set if RX overrun has occurred

; Timers
WndmTimer1load			EQU 0xc00				; load timer 1
WndmTimer1value			EQU 0xc04				; read timer 1
WndmTimer1ctrl			EQU 0xc08				; timer 1 control register
WndmTimer1clr			EQU 0xc0c				; write to clear timer 1 interrupt
WndmTimer2load			EQU 0xc20				; load timer 2
WndmTimer2value			EQU 0xc24				; read timer 2
WndmTimer2ctrl			EQU 0xc28				; timer 2 control register
WndmTimer2clr			EQU 0xc2c				; write to clear timer 2 interrupt
WndmBuzzerCtrl			EQU 0xc40				; Buzzer control

; Timer control register bits
WndmTctrlClksel			EQU 0x04				; 0=2kHz clock, 1=512kHz clock
WndmTctrlMode			EQU 0x40				; 1=periodic, 0=free running
WndmTctrlEnable			EQU 0x80				; set to enable timer

; Buzzer control register bits
WndmBzmod				EQU 0x2					; 1=timer 1 drives buzzer, 0=BZTOG drives
WndmBztog				EQU 0x1					; direct control of buzzer output

; Real-Time Clock module
WndmRtcDataLow			EQU 0xd00				; lower 16 bits of RTC data
WndmRtcDataHigh			EQU 0xd04				; upper 16 bits of RTC data
WndmRtcMatchLow			EQU 0xd08				; lower 16 bits of RTC match value
WndmRtcMatchHigh		EQU 0xd0c				; upper 16 bits of RTC match value
WndmRtceoi				EQU 0xd10				; write to clear RTC match interrupt




; GPIO/Keyboard module
WndmPadr				EQU 0xe00				; Port A data register
WndmPbdr				EQU 0xe04				; Port B data register
WndmPcdr				EQU 0xe08				; Port C data register
WndmPortCRs232Enable        EQU 0x0008              ; Turn on the debug UART
WndmPddr				EQU 0xe0c				; Port D data register
WndmPaddr				EQU 0xe10				; Port A data direction register
WndmPbddr				EQU 0xe14				; Port B data direction register
WndmPcddr				EQU 0xe18				; Port C data direction register
WndmPdddr				EQU 0xe1c				; Port D data direction register
WndmPedr				EQU 0xe20				; Port E data register
WndmPeddr				EQU 0xe24				; Port E data direction register
WndmKscan				EQU 0xe28				; Keyboard column drive state
WndmLcdmux				EQU 0xe2c				; LCD output via GPIO control

; KSCAN register
WndmKscanHigh			EQU 0					; all outputs high
WndmKscanLow			EQU 1					; all outputs low
WndmKscanHiZ			EQU 2					; all outputs high impedance
WndmKscan0				EQU 8					; Column 0 high, others High-Z
WndmKscan1				EQU 9					; Column 1 high, others High-Z
WndmKscan2				EQU 10					; Column 2 high, others High-Z
WndmKscan3				EQU 11					; Column 3 high, others High-Z
WndmKscan4				EQU 12					; Column 4 high, others High-Z
WndmKscan5				EQU 13					; Column 5 high, others High-Z
WndmKscan6				EQU 14					; Column 6 high, others High-Z
WndmKscan7				EQU 15					; Column 7 high, others High-Z

; LCDMUX register
WndmPeocon				EQU 1					; set to use PE0-3 for lcd data 4-7
WndmPdocon				EQU 2					; set to use PD0-7 for lcd data 8-15

; *** CRAP and BASIC debug MACRO when we haven't got a STACK.
		MACRO
		OUT_BYTE_WINDY	$aReg, $aByte
    [ DebugBootRom
        MOV     $aReg, #$aByte
        STRB    $aReg, [r9, #(WndmUart2Base+WndmUartdr)]
10		LDR		$aReg, [r9, #(WndmUart2Base+WndmUartflg)]
		TST		$aReg, #WndmFlgUtxff
		BNE		%BT10
    ]
        MEND

;
;***************************************************************************************
;
    	END

