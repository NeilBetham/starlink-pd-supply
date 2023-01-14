/**
 * @brief Vector table init
 * */

// Stack address provided by the linker
extern int _stack_ptr;

// Weak linkage for default handlers
#define DEFAULT __attribute__((weak, alias("Default_Handler")))

// Default Handler that loops indefinitely
extern "C" void Default_Handler() { while(1); };

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

DEFAULT void DMAChannel_1_ISR(void);
DEFAULT void DMAChannel_3_2_ISR(void);
DEFAULT void DMAChannel_7_4_ISR(void);

DEFAULT void ADC_Comp_ISR(void);

DEFAULT void LPTimer_ISR(void);

DEFAULT void Timer_2_ISR(void);
DEFAULT void Timer_3_ISR(void);
DEFAULT void Timer_6_ISR(void);
DEFAULT void Timer_7_ISR(void);
DEFAULT void Timer_21_ISR(void);
DEFAULT void Timer_22_ISR(void);

DEFAULT void I2C_1_ISR(void);
DEFAULT void I2C_2_ISR(void);
DEFAULT void I2C_3_ISR(void);

DEFAULT void SPI_1_ISR(void);
DEFAULT void SPI_2_ISR(void);

DEFAULT void LPUART_1_AES_ISR(void);

DEFAULT void USART_1_ISR(void);
DEFAULT void USART_2_ISR(void);
DEFAULT void USART_4_5_ISR(void);


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
  0,                          // 0x60
  DMAChannel_1_ISR,           // 0x64
  DMAChannel_3_2_ISR,         // 0x68
  DMAChannel_7_4_ISR,         // 0x6C
  ADC_Comp_ISR,               // 0x70
  LPTimer_ISR,                // 0x74
  USART_4_5_ISR,              // 0x78
  Timer_2_ISR,                // 0x7C
  Timer_3_ISR,                // 0x80
  Timer_6_ISR,                // 0x84
  Timer_7_ISR,                // 0x88
  0,                          // 0x8C
  Timer_21_ISR,               // 0x90
  I2C_3_ISR,                  // 0x94
  Timer_22_ISR,               // 0x98
  I2C_1_ISR,                  // 0x9C
  I2C_2_ISR,                  // 0xA0
  SPI_1_ISR,                  // 0xA4
  SPI_2_ISR,                  // 0xA8
  USART_1_ISR,                // 0xAC
  USART_2_ISR,                // 0xB0
  LPUART_1_AES_ISR            // 0xB4
};


