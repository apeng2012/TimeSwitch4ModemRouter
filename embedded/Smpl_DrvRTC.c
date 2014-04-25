/*---------------------------------------------------------------------------------------------------------*/
/*                                                                                                         */
/* Copyright(c) 2009 Nuvoton Technology Corp. All rights reserved.                                         */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "NUC122.h"
#include "Driver/DrvUART.h"
#include "Driver/DrvRTC.h"
#include "Driver/DrvGPIO.h"
#include "Driver/DrvSYS.h"
#include "Driver/DrvFMC.h"

#define Int2BCD(x) ((((x)/10)<<4) + (((x)%10)&0x0F))
#define BCD2Int(x) ((0xFF&((x)>>4))*10 + (0x0F&(x)))

typedef union {
    uint32_t alarm32;
//     struct {
//         uint16_t on;
//         uint16_t off;
//     } a16;
    struct {
        uint8_t fgO :4;
        uint8_t fsO :4;
        uint8_t sgO :4;
        uint8_t ssO :4;
        
        uint8_t fgF :4;
        uint8_t fsF :4;
        uint8_t sgF :4;
        uint8_t ssF :4;
    } a4;
}AlarmType;

/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
volatile uint8_t gFlagHaveCMD = FALSE;
volatile uint8_t gFlagSetMode = TRUE;
volatile uint8_t gFlagRxOver = FALSE;
volatile uint8_t uartRxBuf[256];
volatile uint8_t uartRxIndex = 0;

uint8_t alarmBuf[256]; // all day on 0000-2400; all day off FFFF-FFFF
                       // 32bytes = 1day; can set 8 part on time
uint32_t alarmHourOn[7];
uint32_t alarmHourOff[7];

S_DRVRTC_TIME_DATA_T sDataTime;

static void GC_PowerOn(void) {
    DrvGPIO_SetBit(E_GPD,0);
    DrvGPIO_SetBit(E_GPD,1);
    DrvGPIO_SetBit(E_GPD,2);
}

static void GC_PowerOff(void) {
    DrvGPIO_ClrBit(E_GPD,0);
    DrvGPIO_ClrBit(E_GPD,1);
    DrvGPIO_ClrBit(E_GPD,2);
}


void EINT1Callback(void)
{
    GC_PowerOn();
}

void SysTick_Handler(void) {
    SysTick->CTRL = 0;
    if (gFlagHaveCMD == FALSE) {
        gFlagSetMode = FALSE;
    }
}

void UART_CallBackfn(uint32_t intState) {
    UART_T * tUART;

    tUART = (UART_T *)((uint32_t)UART0 + UART_PORT1);
    
    while(tUART->FSR.RX_EMPTY != 1) {
        uartRxBuf[uartRxIndex++] = tUART->DATA;
    }
    
    if (intState & DRVUART_TOUTINT) {
        gFlagRxOver = TRUE;
    }
}

/*---------------------------------------------------------------------------------------------------------*/
/* RTC Alarm Callback function                                                                             */
/*---------------------------------------------------------------------------------------------------------*/
void RTC_AlarmCallBackfn(void) {
    uint8_t i, type, week;
    uint16_t alarm;
    uint16_t *p;
    
    DrvRTC_Read(DRVRTC_ALARM_TIME, &sDataTime);
    week = sDataTime.u32cDayOfWeek;
    alarm = Int2BCD(sDataTime.u32cHour);
    alarm <<= 8;
    alarm += Int2BCD(sDataTime.u32cMinute);
    p = (uint16_t*)&alarmBuf[32*(week+1)];
    for (i=0; i<16; i++) {
        if (*p == 0xFFFF) {
            GC_PowerOff();
            break;
        }
        if (*p++ == alarm) {
            if (i & 1) {
                GC_PowerOff();
            } else { // on
                GC_PowerOn();
            }
            break;
        }
    }
    
    // find next alarm
    type = 0;
    if (i != 16) {
        if ((*p != 0xFFFF) && (*p != 0x2400)) {
            type = 1;
            i = week;
            goto SetNextAlarm;
        }
    }
    for (i=week+1; i<7; i++) {
        p = (uint16_t*)&alarmBuf[32*(i+1)];
        if ((*p != 0xFFFF) && (*p != 0x2400)) {
            type = 2;
            goto SetNextAlarm;
        }
    }
    for (i=0; i<7; i++) {
        p = (uint16_t*)&alarmBuf[32*(i+1)];
        if ((*p != 0xFFFF) && (*p != 0x2400)) {
            type = 3;
            goto SetNextAlarm;
        }
    }
SetNextAlarm:
    alarm = *p;
    DrvRTC_Read(DRVRTC_CURRENT_TIME, &sDataTime);
    switch (type) {
        case 1:
        case 2:
            type = i - week;
            break;
        case 3:
            type = 7 - week + i;
            break;
        default:
            while(1){}
    }
    switch (sDataTime.u32cMonth) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            week = 31;
            break;
        case 4:
        case 6:
        case 9:
        case 11:
            week = 31;
            break;
        case 2:
            if (DrcRTC_IsLeapYear()) {
                week = 29;
            } else {
                week = 28;
            }
            break;
        default:
            while(1);
    }
    sDataTime.u32cDay += type;
    if (sDataTime.u32cDay > week) {
        sDataTime.u32cDay = week - sDataTime.u32cDay;
        sDataTime.u32cMonth++;
        if (sDataTime.u32cMonth > 12) {
            sDataTime.u32cMonth = 1;
            sDataTime.u32Year++;
        }
    }
    
    sDataTime.u32cDayOfWeek = i;
    i = alarm>>12; i *= 10; i += (alarm>>8) & 0xF;
    sDataTime.u32cHour = i;
    alarm &= 0xFF;
    i = alarm>>4; i *= 10; i += alarm & 0xF;
    sDataTime.u32cMinute = i;
    sDataTime.u32cSecond = 0;
    sDataTime.u8cClockDisplay = DRVRTC_CLOCK_24;
    
    DrvRTC_Write(DRVRTC_ALARM_TIME, &sDataTime);
    DrvRTC_EnableInt(DRVRTC_ALARM_INT, RTC_AlarmCallBackfn);
}

void InitUARTCmdPort(void)
{
    STR_UART_T param;

    /* Select UART Clock Source From Internal 22M */
    DrvSYS_SelectIPClockSource(E_SYS_UART_CLKSRC, 3);

    param.u32BaudRate        = 9600;
    param.u8cDataBits        = DRVUART_DATABITS_8;
    param.u8cStopBits        = DRVUART_STOPBITS_1;
    param.u8cParity          = DRVUART_PARITY_NONE;
    param.u8cRxTriggerLevel  = DRVUART_FIFO_14BYTES;
    param.u8TimeOut          = param.u32BaudRate/10; // 100ms

//     if (DEBUG_PORT == 1)
//     {
        /* Set UART1 pins */
        DrvGPIO_InitFunction(E_FUNC_UART1);
        
        /* Set UART1 configuration */
        DrvUART_Open(UART_PORT1, &param);
        
        DrvUART_EnableInt(UART_PORT1, (DRVUART_TOUTINT | DRVUART_RDAINT),UART_CallBackfn);
//     }else
//     {
//         /* Set UART0 pins */
//         DrvGPIO_InitFunction(E_FUNC_UART0);
//         
//         /* Set UART0 configuration */
//         DrvUART_Open(UART_PORT0, &param);
//     }
}

static void SysClk_SetMode_Init(void) {
    /* Enable 32k Xtal */
	DrvSYS_SetOscCtrl(E_SYS_XTL32K, ENABLE);
    /* Waiting for 32K stable */
    while (SYSCLK->CLKSTATUS.XTL32K_STB == 0);
	
	/* Enable Internal 22M Xtal */
	DrvSYS_SetOscCtrl(E_SYS_OSC22M, ENABLE);
	/* Waiting for 22M Xtal stable */
	while (DrvSYS_GetChipClockSourceStatus(E_SYS_OSC22M) != 1);
	
    /* Set 22M as HCLK */
	DrvSYS_SelectHCLKSource(7);
	
	DrvSYS_SetOscCtrl(E_SYS_PLL, DISABLE);
	DrvSYS_SetOscCtrl(E_SYS_OSC10K, DISABLE);
	DrvSYS_SetOscCtrl(E_SYS_XTL12M, DISABLE);
}

static void SysClk_NormalMode_Init(void) {
    /* Set 32K as HCLK */
	DrvSYS_SelectHCLKSource(1);
	DrvSYS_SetOscCtrl(E_SYS_OSC22M, DISABLE);
}

static void InitAlarmBuf(void) {
    uint8_t i;
    uint32_t *p;
    uint32_t addr;
    
    p = (uint32_t*)alarmBuf;
    addr = 0x1F000;
    DrvFMC_EnableISP();
    for (i=0; i<64; i++) {
        DrvFMC_Read(addr, p++);
        addr += 4;
    }
    DrvFMC_DisableISP();
    
    p = (uint32_t*)alarmBuf;
    if (*p != 0x55AA55AA) {
        for (i=0; i<64; i++) {
            *p++ = 0xFFFFFFFF;
        }
        p = (uint32_t*)alarmBuf;
        *p++ = 0x55AA55AA;
        *p = 0;
    }
}

static uint8_t isAlarmBufInit() {
    uint8_t i;
    uint32_t *p = (uint32_t*)&alarmBuf[32];
    
    for (i=0; i<56; i++) {
        if (*p++ != 0xFFFFFFFF) {
            return i+1;
        }
    }
    
    return 0;
}

static void ReadAlarm(void) {
    uint8_t i, j;
    AlarmType *p;
    uint8_t addr;
    uint32_t tmp;
    
    tmp = *(uint32_t*)(&alarmBuf[4]);
    printf("ReadAlarm %d\r\n", tmp);
    addr = 32;
    for (i=0; i<7; i++) {
        p = (AlarmType*)&alarmBuf[addr];
        addr += 32;
        if (p->alarm32 == 0xFFFFFFFF) {
            printf("no alarm\r\n");
        } else {
            for (j=0; j<8; j++) {
                tmp = p->a4.ssO; tmp *= 10; tmp += p->a4.sgO; printf("%02d", tmp);
                tmp = p->a4.fsO; tmp *= 10; tmp += p->a4.fgO; printf(":%02d", tmp);
                tmp = p->a4.ssF; tmp *= 10; tmp += p->a4.sgF; printf("-%02d", tmp);
                tmp = p->a4.fsF; tmp *= 10; tmp += p->a4.fgF; printf(":%02d \r\n", tmp);
                p++;
                if (p->alarm32 == 0xFFFFFFFF) {
                    break;
                }
            }
        }
        printf("weekday %d over\r\n", i);
    }
}

static void ProgramAlarm(void) {
    uint8_t i;
    uint32_t addr = 0x1F000;
    uint32_t *p = (uint32_t*)&alarmBuf[4];
    
    if (memcmp((void*)uartRxBuf, "ProgramAlarm", 12) != 0) {
        return;
    }
    *p = *p + 1;
    p = (uint32_t*)alarmBuf;
    DrvFMC_EnableISP();
    DrvFMC_Erase(addr);
    for (i=0; i<64; i++) {
        DrvFMC_Write(addr, *p++);
        addr += 4;
    }
    DrvFMC_DisableISP();
    printf("ProgramAlarmOver\r\n");
}

static void WriteAlarm(void) {
    uint8_t addr;
    AlarmType *p;
    uint8_t i;
    
    if (memcmp((void*)uartRxBuf, "WriteAlarm", 10) == 0) {
        addr = uartRxBuf[10]-'0';
        if (addr > 6) {
            return;
        }
        addr++;
        addr *= 32;
        p = (AlarmType*)&alarmBuf[addr];
        for (i=0; i<8; i++) {
            p->alarm32 = 0xFFFFFFFF;
            p++;
        }
        p = (AlarmType*)&alarmBuf[addr];
        //          9   
        // WriteAlarmX 1>hh:mm-hh:mm 2>...
        addr = 12;
        while(1) {
            if (uartRxBuf[addr+1] != '>') {
                printf("WriteAlarmOver\r\n");
                return;
            }
            if (uartRxIndex < addr + 13) {
                printf("WriteAlarmOver\r\n");
                return;
            }
            p->a4.ssO = uartRxBuf[addr+2]-'0';
            p->a4.sgO = uartRxBuf[addr+3]-'0';
            p->a4.fsO = uartRxBuf[addr+5]-'0';
            p->a4.fgO = uartRxBuf[addr+6]-'0';
            p->a4.ssF = uartRxBuf[addr+8]-'0';
            p->a4.sgF = uartRxBuf[addr+9]-'0';
            p->a4.fsF = uartRxBuf[addr+11]-'0';
            p->a4.fgF = uartRxBuf[addr+12]-'0';
            p++;
            addr += 14;
        }
    }
}

static void SetDateTime(void) {
    if (memcmp((void*)uartRxBuf, "SetDateTime", 11) == 0) {
        sDataTime.u32Year       = (uartRxBuf[12]-'0')*1000
            + (uartRxBuf[13]-'0')*100
            + (uartRxBuf[14]-'0')*10
            + (uartRxBuf[15]-'0');
        sDataTime.u32cMonth     = (uartRxBuf[17]-'0')*10
            + (uartRxBuf[18]-'0');
        sDataTime.u32cDay       = (uartRxBuf[20]-'0')*10
            + (uartRxBuf[21]-'0');
        sDataTime.u32cHour      = (uartRxBuf[23]-'0')*10
            + (uartRxBuf[24]-'0');
        sDataTime.u32cMinute    = (uartRxBuf[26]-'0')*10
            + (uartRxBuf[27]-'0');
        sDataTime.u32cSecond    = (uartRxBuf[29]-'0')*10
            + (uartRxBuf[30]-'0');
        sDataTime.u32cDayOfWeek = (uartRxBuf[32]-'0');
        sDataTime.u8cClockDisplay = DRVRTC_CLOCK_24;
        if (DrvRTC_Open(&sDataTime) != E_SUCCESS) {
            printf("RTC Open Fail !! \r\n");
        } else {
            printf("RTC Set OK \r\n");
        }
    } else {
        printf("error cmd\r\n");
    }
}

static void GetDateTime(void) {
    DrvRTC_Read(DRVRTC_CURRENT_TIME, &sDataTime);
    printf("CurTime %d", sDataTime.u32Year);
    printf("-%d", sDataTime.u32cMonth);
    printf("-%d", sDataTime.u32cDay);
    printf(" %d", sDataTime.u32cHour);
    printf(":%d", sDataTime.u32cMinute);
    printf(":%d", sDataTime.u32cSecond);
    printf(" %d", sDataTime.u32cDayOfWeek);
}

static void AlarmHourInit(void) {
    uint8_t i, j;
    int8_t k;
    AlarmType *p;
    uint8_t on, off;
    
    for (i=0; i<7; i++) {
        alarmHourOn[i] = 0;
        alarmHourOff[i] = 0xFFFFFF;
    }
    for (i=0; i<7; i++) {
        p = (AlarmType*)&alarmBuf[32*(i+1)];
        for (j=0; j<8; j++) {
            if (p->alarm32 == 0xFFFFFFFF) {
                break;
            }
            on = p->a4.ssO*10 + p->a4.sgO;
            off = p->a4.ssF*10 + p->a4.sgF;
            if (off < 24) {
                alarmHourOff[i] &= ~(1<<off);
            }
            alarmHourOff[i] &= ~(1<<on);
            for (k=on+1; k<off; k++) {
                alarmHourOn[i] |= 1<<k;
                alarmHourOff[i] &= ~(1<<k);
            }
        }
    }
}

static void ControlPowerPin_Init(void) {
    uint8_t i;
    uint16_t *p;
    uint32_t tmp;
    
    DrvGPIO_Open(E_GPD, 0, E_IO_OUTPUT);
    DrvGPIO_Open(E_GPD, 1, E_IO_OUTPUT);
    DrvGPIO_Open(E_GPD, 2, E_IO_OUTPUT);
    GC_PowerOff();
    AlarmHourInit();
    DrvRTC_Read(DRVRTC_CURRENT_TIME, &sDataTime);
    if (alarmHourOn[sDataTime.u32cDayOfWeek] & (1<<sDataTime.u32cHour)) {
        GC_PowerOn();
        return;
    }
    
    tmp = (~alarmHourOn[sDataTime.u32cDayOfWeek]) & (~alarmHourOff[sDataTime.u32cDayOfWeek]);
    tmp &= 0x00FFFFFF;
    if (tmp & (1<<sDataTime.u32cHour)) {
        tmp = Int2BCD(sDataTime.u32cHour);
        tmp <<= 8;
        tmp += Int2BCD(sDataTime.u32cMinute);
        p = (uint16_t*)&alarmBuf[(sDataTime.u32cDayOfWeek + 1)*32];
        for (i=0; i<16; i++) {
            if (tmp < *p++) {
                if (i & 1) {
                    GC_PowerOn();
                }
                return;
            }
        }
    }
}

static void SetAlarmInitValue(void) {
    uint16_t cur;
    uint16_t *p;
    uint8_t i;
    uint8_t type;
    uint8_t week;
    uint32_t ret = 1;
    
    DrvRTC_Read(DRVRTC_CURRENT_TIME, &sDataTime);
    cur = Int2BCD(sDataTime.u32cHour);
    cur <<= 8;
    cur += Int2BCD(sDataTime.u32cMinute);
    
    p = (uint16_t*)&alarmBuf[(sDataTime.u32cDayOfWeek + 1)*32];
    for (i=0; i<16; i++) {
        if (*p >= 0x2400) {
            break;
        }
        if (*p > cur) {
            sDataTime.u32cSecond = 0;
            sDataTime.u32cMinute = BCD2Int((*p)&0x00FF);
            sDataTime.u32cHour = BCD2Int((*p)>>8);
            sDataTime.u8cClockDisplay = DRVRTC_CLOCK_24;
            while (ret) {
                ret = DrvRTC_Write(DRVRTC_ALARM_TIME, &sDataTime);
            }
            return;
        }
        p++;
    }
    
    for (i=sDataTime.u32cDayOfWeek+1; i<7; i++) {
        p = (uint16_t*)&alarmBuf[32*(i+1)];
        if ((*p != 0xFFFF) && (*p != 0x2400)) {
            type = 2;
            goto SetInitAlarm;
        }
    }
    for (i=0; i<7; i++) {
        p = (uint16_t*)&alarmBuf[32*(i+1)];
        if ((*p != 0xFFFF) && (*p != 0x2400)) {
            type = 3;
            goto SetInitAlarm;
        }
    }
SetInitAlarm:
    switch (type) {
        case 1:
        case 2:
            type = i - sDataTime.u32cDayOfWeek;
            break;
        case 3:
            type = 7 - sDataTime.u32cDayOfWeek + i;
            break;
        default:
            while(1){}
    }
    switch (sDataTime.u32cMonth) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            week = 31;
            break;
        case 4:
        case 6:
        case 9:
        case 11:
            week = 31;
            break;
        case 2:
            if (DrcRTC_IsLeapYear()) {
                week = 29;
            } else {
                week = 28;
            }
            break;
        default:
            while(1);
    }
    sDataTime.u32cDay += type;
    if (sDataTime.u32cDay > week) {
        sDataTime.u32cDay = week - sDataTime.u32cDay;
        sDataTime.u32cMonth++;
        if (sDataTime.u32cMonth > 12) {
            sDataTime.u32cMonth = 1;
            sDataTime.u32Year++;
        }
    }
    
    sDataTime.u32cDayOfWeek = i;
    sDataTime.u32cMinute = BCD2Int((*p)&0x00FF);
    sDataTime.u32cHour = BCD2Int((*p)>>8);
    sDataTime.u32cSecond = 0;
    sDataTime.u8cClockDisplay = DRVRTC_CLOCK_24;
    while (ret) {
        ret = DrvRTC_Write(DRVRTC_ALARM_TIME, &sDataTime);
    }
}

int32_t main()
{
    UNLOCKREG();
	
    SysClk_SetMode_Init();

    /* Initial UART debug message function */
    InitUARTCmdPort();
    
    InitAlarmBuf();
    
    /* sysTick source = 32k; 10s = 32k*10 */
    DrvSYS_SelectSysTickSource(1);
    SysTick_Config(320000); // 10s
    
    DrvRTC_Init();

#if 0    
    /* Time Setting */
    sDataTime.u32Year       = 2010;
    sDataTime.u32cMonth     = 12;   
    sDataTime.u32cDay       = 7;
    sDataTime.u32cHour      = 13;
    sDataTime.u32cMinute    = 20;
    sDataTime.u32cSecond    = 0;
    sDataTime.u32cDayOfWeek = DRVRTC_TUESDAY;
    sDataTime.u8cClockDisplay = DRVRTC_CLOCK_24;            
    
    /* Initialization the RTC timer */
    if (DrvRTC_Open(&sDataTime) != E_SUCCESS)
    {
        printf("DrvRTC_Open err");
    }
    
    /* Get the currnet time */
    DrvRTC_Read(DRVRTC_CURRENT_TIME, &sDataTime);

    printf("Start Time: %d/%02d/%02d %02d:%02d:%02d \n",
            sDataTime.u32Year, sDataTime.u32cMonth, sDataTime.u32cDay,
            sDataTime.u32cHour, sDataTime.u32cMinute, sDataTime.u32cSecond);
    
    /* The alarm time setting */    
    sDataTime.u32cSecond = sDataTime.u32cSecond + 10;     
    
    /* Set the alarm time (Install the call back function and enable the alarm interrupt)*/
    DrvRTC_Write(DRVRTC_ALARM_TIME, &sDataTime);
            
    /* Enable RTC Alarm Interrupt & Set Alarm call back function*/
    DrvRTC_EnableInt(DRVRTC_ALARM_INT, RTC_AlarmCallBackfn);
    
    printf("Wait initterpt");
    while (1) {
    }
#endif
    
    printf("Wait UART\r\n");

WAIT_UART:
    while (gFlagSetMode) {
        if (gFlagRxOver) {
            gFlagRxOver = FALSE;
            gFlagHaveCMD = TRUE;
            switch (uartRxBuf[0]) {
                case 'H':
                case 'h':
                    printf("Hello\r\n");
                    break;
                case 'S': // SetDateTime 2014-01-27 13:21:25 1
                    SetDateTime();
                    break;
                case 'G': // GetDateTime
                    GetDateTime();
                    break;
                case 'R': // ReadAlarm
                    ReadAlarm();
                    break;
                case 'W': // WriteAlarmX 1>hh:mm-hh:mm 2>...
                    WriteAlarm();
                    break;
                case 'P': // ProgramAlarm
                    ProgramAlarm();
                    break;
                default:
                    gFlagHaveCMD = FALSE;
                    printf("error cmd\r\n");
                    break;
            }
            uartRxIndex = 0;
        }
    }
    printf("No UART\r\n");
    
    if (isAlarmBufInit() == 0) {
        printf("Alarm Buf no init \r\n");
        gFlagSetMode = TRUE;
        goto WAIT_UART;
    }
    
    
    UNLOCKREG();
    DrvUART_DisableInt(UART_PORT1,DRVUART_TOUTINT | DRVUART_RDAINT);
    DrvUART_Close(UART_PORT1);

    ControlPowerPin_Init();
    SetAlarmInitValue();
    
    // init button on
    DrvGPIO_SetDebounceTime(10, E_DBCLKSRC_HCLK);
    DrvGPIO_EnableDebounce(E_GPB, 15);
    DrvGPIO_InitFunction(E_FUNC_EXTINT1);
    DrvGPIO_EnableEINT1(E_IO_FALLING, E_MODE_EDGE, EINT1Callback);
    
	SysClk_NormalMode_Init();
    DrvRTC_EnableInt(DRVRTC_ALARM_INT, RTC_AlarmCallBackfn);
	
    while(1) {
        UNLOCKREG();
        /* Enable deep sleep */
        SCB->SCR = 4;
        
        /* Enable wait for CPU function and enter in Power Down */
        DrvSYS_EnterPowerDown(E_SYS_WAIT_FOR_CPU);
        
        /* Wait for interrupt and enter in Power Down mode */
        __WFI();
    }
}
	 
