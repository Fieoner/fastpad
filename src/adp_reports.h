#ifndef __ADP_REPORTS_H
#define __ADP_REPORTS_H

#include <stdint.h>

#define ADP_SENSOR_COUNT   32  /* wire format always 32; only first 8 are active */
#define ADP_BUTTON_COUNT   16  /* uint16 button field, only 8 physically used */
#define ADP_MAX_NAME_SIZE  50
#define ADP_MAX_SENSOR_VALUE 1023

/* Report IDs matching analog-dance-pad protocol */
#define REPORT_ID_INPUT           0x01
#define REPORT_ID_PAD_CONFIG      0x02
#define REPORT_ID_RESET           0x03
#define REPORT_ID_SAVE            0x04
#define REPORT_ID_NAME            0x05
#define REPORT_ID_JOYSTICK        0x06
#define REPORT_ID_LIGHT_RULE      0x07
#define REPORT_ID_FACTORY_RESET   0x08
#define REPORT_ID_IDENTIFICATION  0x09
#define REPORT_ID_LED_MAPPING     0x0A
#define REPORT_ID_SET_PROPERTY    0x0B
#define REPORT_ID_SENSOR          0x0C
#define REPORT_ID_IDENTIFICATION_V2 0x0E
#define REPORT_ID_BOOTLOADER      0x0F

/* Input report: sent every microframe */
typedef struct {
    uint16_t buttons;
    uint16_t sensorValues[ADP_SENSOR_COUNT];
} __attribute__((packed)) InputReport;

#define INPUT_REPORT_SIZE sizeof(InputReport) /* 2 + 64 = 66 */

/* Feature report 0x02: pad configuration.
 * releaseMultiplier stored as raw bytes to avoid float in IRQ context. */
typedef struct {
    uint16_t sensorThresholds[ADP_SENSOR_COUNT];
    uint8_t  releaseMultiplier[4]; /* IEEE 754 float, little-endian */
    int8_t   sensorToButtonMapping[ADP_SENSOR_COUNT];
} __attribute__((packed)) PadConfiguration;

#define PAD_CONFIG_REPORT_SIZE sizeof(PadConfiguration) /* 64+4+32 = 100 */

/* Feature report 0x05: name */
typedef struct {
    uint8_t size;
    char    name[ADP_MAX_NAME_SIZE];
} __attribute__((packed)) NameReport;

#define NAME_REPORT_SIZE sizeof(NameReport) /* 51 */

/* Feature report 0x09: identification */
typedef struct {
    uint16_t firmwareVersionMajor;
    uint16_t firmwareVersionMinor;
    uint8_t  buttonCount;
    uint8_t  sensorCount;
    uint8_t  ledCount;
    uint16_t maxSensorValue;
    char     boardType[32];
} __attribute__((packed)) IdentificationReport;

#define IDENTIFICATION_REPORT_SIZE sizeof(IdentificationReport) /* 41 */

/* Feature report 0x0E: identification v2 */
typedef struct {
    IdentificationReport parent;
    uint16_t features;
} __attribute__((packed)) IdentificationV2Report;

#define IDENTIFICATION_V2_REPORT_SIZE sizeof(IdentificationV2Report) /* 43 */

/* Sensor config for report 0x0C */
typedef struct {
    uint16_t threshold;
    uint16_t releaseThreshold;
    int8_t   buttonMapping;
    uint8_t  resistorValue;
    uint16_t flags;
} __attribute__((packed)) SensorConfig;

typedef struct {
    uint8_t      index;
    SensorConfig sensor;
} __attribute__((packed)) SensorReport;

#define SENSOR_REPORT_SIZE sizeof(SensorReport) /* 9 */

/* Light rule report 0x07 - stub */
typedef struct {
    uint8_t index;
    uint8_t data[7]; /* placeholder */
} __attribute__((packed)) LightRuleReport;

#define LIGHT_RULE_REPORT_SIZE sizeof(LightRuleReport) /* 8 */

/* LED mapping report 0x0A - stub */
typedef struct {
    uint8_t index;
    uint8_t data[3]; /* placeholder */
} __attribute__((packed)) LedMappingReport;

#define LED_MAPPING_REPORT_SIZE sizeof(LedMappingReport) /* 4 */

/* Set property report 0x0B */
typedef struct {
    uint32_t propertyId;
    uint32_t propertyValue;
} __attribute__((packed)) SetPropertyReport;

#define SET_PROPERTY_REPORT_SIZE sizeof(SetPropertyReport) /* 8 */

#endif
