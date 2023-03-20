/**
 * @brief Vector table init
 * */

// Stack address provided by the linker
extern int _stack_ptr;

// Weak linkage for default handlers
#define DEFAULT __attribute__((weak, alias("Default_Handler")))

// Default Handler that loops indefinitely
extern "C" void Default_Handler() { asm("bkpt 1"); while(1); };

// System Exception Handlers
void Reset_Handler(void);
DEFAULT void NMI_Handler(void);
DEFAULT void SVC_Handler(void);
DEFAULT void PendSV_Handler(void);
DEFAULT void SysTick_Handler(void);

// Fault Handlers
DEFAULT void HardFault_Handler(void);

// Peripheral ISRs
DEFAULT void WatchDogTimer_ISR(void);
DEFAULT void ProgVoltageDetector_ISR(void);
DEFAULT void RealTimeClock_ISR(void);
DEFAULT void Flash_ISR(void);
DEFAULT void RunClockConfig_ISR(void);

DEFAULT void ExternInterrupt_0_1_ISR(void);
DEFAULT void ExternInterrupt_3_2_ISR(void);
DEFAULT void ExternInterrupt_15_4_ISR(void);

DEFAULT void PD1_PD2_USB_ISR(void);

DEFAULT void DMA1Channel_1_ISR(void);
DEFAULT void DMA1Channel_2_3_ISR(void);
DEFAULT void DMA_1_2_MUX_Channel_ISR(void);

DEFAULT void ADC_Comp_ISR(void);

DEFAULT void Timer_1_ISR(void);
DEFAULT void Timer_1CC_ISR(void);
DEFAULT void Timer_2_ISR(void);
DEFAULT void Timer_3_4_ISR(void);
DEFAULT void Timer_6_ISR(void);
DEFAULT void Timer_7_ISR(void);
DEFAULT void Timer_14_ISR(void);
DEFAULT void Timer_15_ISR(void);
DEFAULT void Timer_16_FD_CAN0_ISR(void);
DEFAULT void Timer_17_FD_CAN1_ISR(void);

DEFAULT void I2C_1_ISR(void);
DEFAULT void I2C_2_3_ISR(void);

DEFAULT void SPI_1_ISR(void);
DEFAULT void SPI_2_3_ISR(void);

DEFAULT void USART_1_ISR(void);
DEFAULT void USART_2_LP2_ISR(void);
DEFAULT void USART_3_4_5_6_LP1_ISR(void);

DEFAULT void CEC_ISR(void);

DEFAULT void AES_RNG_ISR(void);

// Vector Table Element
typedef void (*ISRHandler)(void);
union VectorTableEntry {
    ISRHandler isr;   //all ISRs use this type
    void* stack_top;  //pointer to top of the stack
};

// Build and mark the vector table for the linker
__attribute__((section(".vector_table")))
const VectorTableEntry vectors[] = {
  {.stack_top = &_stack_ptr}, // 0x00
  Reset_Handler,              // 0x04
  NMI_Handler,                // 0x08
  HardFault_Handler,          // 0x0C
  0,                          // 0x10
  0,                          // 0x14
  0,                          // 0x18
  0,                          // 0x1C
  0,                          // 0x20
  0,                          // 0x24
  0,                          // 0x28
  SVC_Handler,                // 0x2C
  0,                          // 0x30
  0,                          // 0x34
  PendSV_Handler,             // 0x38
  SysTick_Handler,            // 0x3C
  WatchDogTimer_ISR,          // 0x40
  ProgVoltageDetector_ISR,    // 0x44
  RealTimeClock_ISR,          // 0x48
  Flash_ISR,                  // 0x4C
  RunClockConfig_ISR,         // 0x50
  ExternInterrupt_0_1_ISR,    // 0x54
  ExternInterrupt_3_2_ISR,    // 0x58
  ExternInterrupt_15_4_ISR,   // 0x5C
  PD1_PD2_USB_ISR,            // 0x60
  DMA1Channel_1_ISR,          // 0x64
  DMA1Channel_2_3_ISR,        // 0x68
  DMA_1_2_MUX_Channel_ISR,    // 0x6C
  ADC_Comp_ISR,               // 0x70
  Timer_1_ISR,                // 0x74
  Timer_1CC_ISR,              // 0x78
  Timer_2_ISR,                // 0x7C
  Timer_3_4_ISR,              // 0x80
  Timer_6_ISR,                // 0x84
  Timer_7_ISR,                // 0x88
  Timer_14_ISR,               // 0x8C
  Timer_15_ISR,               // 0x90
  Timer_16_FD_CAN0_ISR,       // 0x94
  Timer_17_FD_CAN1_ISR,       // 0x98
  I2C_1_ISR,                  // 0x9C
  I2C_2_3_ISR,                // 0xA0
  SPI_1_ISR,                  // 0xA4
  SPI_2_3_ISR,                // 0xA8
  USART_1_ISR,                // 0xAC
  USART_2_LP2_ISR,            // 0xB0
  USART_3_4_5_6_LP1_ISR,      // 0xB4
  CEC_ISR,                    // 0xB8
  AES_RNG_ISR                 // 0xBC
};


