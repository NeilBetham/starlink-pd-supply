/**
 * @brief TCPC PHY related defs
 */

#pragma once


// Phy Register Defines
#define PHY_REG_VENDOR_ID           0x00
#define PHY_REG_PROD_ID             0x02
#define PHY_REG_DEV_ID              0x04
#define PHY_REG_TC_REV              0x06
#define PHY_REG_PD_REV              0x08
#define PHY_REG_PD_INTER_REV        0x0A
#define PHY_REG_ALERT               0x10
#define PHY_REG_ALERT_MASK          0x12
#define PHY_REG_POWER_STAT_MASK     0x14
#define PHY_REG_FAULT_STAT_MASK     0x15
#define PHY_REG_EXT_STAT_MASK       0x16
#define PHY_REG_ALERT_EXT_STAT_MASK 0x17
#define PHY_REG_CONF_STD_OUTPUT     0x18
#define PHY_REG_TCPC_CTL            0x19
#define PHY_REG_ROLE_CTL            0x1A
#define PHY_REG_FAULT_CTL           0x1B
#define PHY_REG_POWER_CTL           0x1C
#define PHY_REG_CC_STAT             0x1D
#define PHY_REG_POWER_STAT          0x1E
#define PHY_REG_FAULT_STAT          0x1F
#define PHY_REG_EXTEND_STAT         0x20
#define PHY_REG_ALERT_EXT_STAT      0x21
#define PHY_REG_COMMAND             0x23
#define PHY_REG_DEVICE_CAPS_1       0x24
#define PHY_REG_DEVICE_CAPS_2       0x26
#define PHY_REG_STD_IN_CAPS         0x28
#define PHY_REG_STD_OUT_CAPS        0x29
#define PHY_REG_CONF_EXT_1          0x2A
#define PHY_REG_GEN_TIMER           0x2C
#define PHY_REG_MSG_HDR_INFO        0x2E
#define PHY_REG_RECV_DETECT         0x2F
#define PHY_REG_READ_BYTE_COUNT     0x30
#define PHY_REG_TRANSMIT            0x50
#define PHY_REG_I2C_WRITE_COUNT     0x51
#define PHY_REG_VBUS_VOLTAGE        0x70
#define PHY_REG_VBUS_SNK_DISCON_THR 0x72
#define PHY_REG_VBUS_STOP_DISCH_THR 0x74
#define PHY_REG_VBUS_V_ALRM_HI_CONF 0x76
#define PHY_REG_VBUS_V_ALRM_LO_CONF 0x78
#define PHY_REG_VBUS_HV_TARGET      0x7A
#define PHY_REG_EXT_CFG_ID          0x80
#define PHY_REG_EXT_ALERT           0x82
#define PHY_REG_EXT_ALERT_MASK      0x84
#define PHY_REG_EXT_CONF            0x86
#define PHY_REG_EXT_FAULT_CONF      0x88
#define PHY_REG_EXT_CTL             0x8E
#define PHY_REG_EXT_STAT            0x90
#define PHY_REG_EXT_GPIO_CONF       0x92
#define PHY_REG_EXT_GPIO_CTL        0x93
#define PHY_REG_EXT_GPIO_ALERT_CONF 0x94
#define PHY_REG_EXT_GPIO_STAT       0x96
#define PHY_REG_SRC_HV_MB4B_TIME    0x97
#define PHY_REG_ADC_FILT_CTL_1      0x9A
#define PHY_REG_ADC_FILT_CTL_2      0x9B
#define PHY_REG_VCONN_CONF          0x9C
#define PHY_REG_VCONN_FLT_DEBNC     0x9D
#define PHY_REG_VCONN_FLT_RECOV     0x9E
#define PHY_REG_VCONN_FLT_ATTMP     0x9F

