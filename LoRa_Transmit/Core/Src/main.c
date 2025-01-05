/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 *
 *
 *RECEIVE RECEIVE RECEIVE RECEIVE RECEIVE RECEIVE RECEIVE RECEIVE RECEIVE RECEIVE
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include "stdio.h"
#include "string.h"

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FIFO_OVERFLOW 1
#define NULL_POINTER 2
#define FIFO_SIZE 10      // Beispielgröße des FIFO-Puffers
#define PACKET_SIZE 8     // Beispielgröße eines Pakets

//typedef enum {
//    RADIO_TICK_SIZE_0015_US, // Beispiel für eine Zeitgröße, z. B. 0,015 Mikrosekunden
//    RADIO_TICK_SIZE_1_US,    // Beispiel für 1 Mikrosekunde
//    RADIO_TICK_SIZE_100_US,  // Beispiel für 100 Mikrosekunden
//    // Weitere Größen können hier hinzugefügt werden.
//} RadioTickSizes_t;
//
//typedef struct TickTime_s {
//    RadioTickSizes_t PeriodBase;   //!< Die Basiseinheit der Zeit (Tickgröße)
//    uint16_t PeriodBaseCount;      //!< Die Anzahl der PeriodBase-Einheiten
//} TickTime_t;


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
uint8_t txData_const = 0xC0;  // Beispiel: Opcode für GetStatus
uint8_t rxData_const = 0x00;  // Hier wird die Antwort gespeichert
char uart_buf[100];
int uart_buf_len;
uint8_t busy, nss, reset;
uint8_t tx_fifo[FIFO_SIZE][PACKET_SIZE];  // FIFO für Pakete
uint8_t *tx_eprt = tx_fifo[0];            // Zeiger auf das erste Paket
uint8_t tx_length = 0;                    // Anzahl der Pakete im FIFO
uint8_t tx_activated = 0;
uint8_t buf_out[100]; // Beispielgröße für buf_out, abhängig von deinem Bedarf
uint8_t test_packet[8] = {1, 2, 3, 4, 5, 6, 7, 8};
uint8_t rx_buffer[8]; // Empfangspuffer für Daten

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

void Erase_Flash(void) {
	FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t SectorError;

	// Unlock the Flash
	HAL_FLASH_Unlock();

	// Configure the erase operation
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
	EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	EraseInitStruct.Sector = FLASH_SECTOR_0; // Specify sector to erase
	EraseInitStruct.NbSectors = 1;

	// Erase the sector
	if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
		// Handle error
	}

	// Lock the Flash
	HAL_FLASH_Lock();
}
void ResetChip(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
	HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
	HAL_Delay(20);
	HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
	HAL_Delay(20);
}

void SelectChip(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState state) {
	HAL_GPIO_WritePin(GPIOx, GPIO_Pin, state);
	HAL_Delay(2); // Falls notwendig
}
void SPI_TransmitReceiveWithDebug(SPI_HandleTypeDef* hspi, uint8_t* txData, uint8_t* rxData, uint16_t length, const char* debugMessage) {
	HAL_SPI_TransmitReceive(hspi, txData, rxData, length, HAL_MAX_DELAY);
	if (debugMessage) {
		HAL_UART_Transmit(&huart2, (uint8_t*)debugMessage, strlen(debugMessage), 100);
	}
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t ProcessStatusByte(uint8_t* statusByte) {
	// Extrahiere die Bits 7:5 (Circuit Mode) und 4:2 (Command Status)
	uint8_t circuitMode = (*statusByte >> 5) & 0x07; // Maske 0x07 für 3 Bits
	uint8_t commandStatus = (*statusByte >> 2) & 0x07; // Maske 0x07 für 3 Bits

	// Debug-Ausgabe für UART
	char uart_buf[50];
	int uart_buf_len = sprintf(uart_buf, "Circuit Mode: %u, Command Status: %u\r\n", circuitMode, commandStatus);
	HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 100);

	// Verarbeitung von Circuit Mode
	switch (circuitMode) {
	case 0x2:
		// STDBY_RC
		HAL_UART_Transmit(&huart2, (uint8_t *)"Mode: STDBY_RC\r\n", 17, 100);
		break;
	case 0x3:
		// STDBY_XOSC
		HAL_UART_Transmit(&huart2, (uint8_t *)"Mode: STDBY_XOSC\r\n", 19, 100);
		break;
	case 0x4:
		// FS
		HAL_UART_Transmit(&huart2, (uint8_t *)"Mode: FS\r\n", 10, 100);
		break;
	case 0x5:
		// Rx
		HAL_UART_Transmit(&huart2, (uint8_t *)"Mode: Rx\r\n", 10, 100);
		break;
	case 0x6:
		// Tx
		HAL_UART_Transmit(&huart2, (uint8_t *)"Mode: Tx\r\n", 10, 100);
		break;
	default:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Mode: Unknown\r\n", 16, 100);


	}

	// Verarbeitung von Command Status
	switch (commandStatus) {
	case 0x1:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Command Status: Success\r\n", 26, 100);
		break;
	case 0x2:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Command Status: Data Available\r\n", 33, 100);
		break;
	case 0x3:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Command Status: Timeout\r\n", 26, 100);
		break;
	case 0x4:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Command Status: Error\r\n", 24, 100);
		break;
	case 0x5:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Command Status: Failure\r\n", 26, 100);
		break;
	case 0x6:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Command Status: Tx Done\r\n", 26, 100);
		break;
	default:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Command Status: Unknown\r\n", 27, 100);
	}
	return circuitMode;
}



void SPI_WaitUntilReady(SPI_HandleTypeDef *hspi) {
	while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) {
		busy=uint8_t(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9));
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); // Select the chip
		nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));

		HAL_Delay(5);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // Select the chip
		nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));
		HAL_Delay(5);
			}
	busy=uint8_t(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9));


}
static void SPI1_TRANSCEIVER_Delay(uint8_t* txData, uint8_t* rxData, uint8_t lengh)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
	nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));
	HAL_Delay(10); // Small delay to ensure stability

	SPI_WaitUntilReady(&hspi1);
	HAL_SPI_TransmitReceive(&hspi1, txData, rxData, lengh, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
	nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));

}
//void SPI_WaitPause(SPI_HandleTypeDef *hspi) {
//	while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_RESET) {
//		busy=uint8_t(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9));
//		HAL_SPI_Receive(&hspi1, &rxData_const, 1, HAL_MAX_DELAY);   // Lese die Antwort
//		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // Select the chip
//		nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));
//
//	}
//	busy=uint8_t(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9));
//	HAL_Delay(10); // Small delay to ensure stability
//}


void SPI_WaitOpcod(SPI_HandleTypeDef *hspi) {
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); // Select the chip
	nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));
	while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) {
		busy=uint8_t(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9));
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // Select the chip
		nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));

		HAL_Delay(5);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); // Select the chip
		nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));
		HAL_Delay(10); // Small delay to ensure stability


		//SPI1_TRANSCEIVER_Delay(&tx, &rx, 1);
	}
	busy=uint8_t(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9));

}

void SPI_WaitTransmit(SPI_HandleTypeDef *hspi) {
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // Select the chip
	nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));
	while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) != GPIO_PIN_SET) {
		busy=uint8_t(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9));
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); // Select the chip
		nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));

		HAL_Delay(5); // Small delay to ensure stability
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // Select the chip
		nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));



	}
	busy=uint8_t(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9));
	HAL_Delay(10); // Small delay to ensure stability

}
void GetStatus() {
	uint8_t command = 0xC0;  // Nur Low Byte wird verwendet
	uint8_t response = 0;

	// Sende 16-Bit-Datenrahmen (High Byte wird ignoriert)
	HAL_SPI_TransmitReceive(&hspi1, &command, &response, 1, HAL_MAX_DELAY);

	// Extrahiere nur das Low Byte aus der Antwort
	HAL_Delay(10);


}

void SetStandby() {
	uint8_t command[2] = {0x80, 0x01};
	//uint8_t response[2]= {0x00, 0x00};
	// Sende und empfange 16-Bit-Datenrahmen
	// Zuerst Daten senden
	HAL_SPI_Transmit(&hspi1, (uint8_t*)command, 2, HAL_MAX_DELAY);

	// Dann die Antwort empfangen
	//HAL_SPI_Receive(&hspi1, (uint8_t*)&response, 2, HAL_MAX_DELAY);
	HAL_Delay(10);

}

// Funktion zum Aktivieren des LoRa Modus
void setLoRaMode() {
	//*(uint16_t*)tx = 0x8A | 0x01 << 8; // LoRa mode

	uint8_t tx[2] = {0x8A, 0x01}; // LoRa Mode aktivieren
	HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart2, (uint8_t *)"Set LoRa mode\r\n", 15, 100);
	HAL_Delay(10);

}
void getPacketType(SPI_HandleTypeDef* hspi) {
	//uint8_t txData;
	//*(uint32_t*)txData = 0x03 | 0x00 << 16; // LoRa mode
	uint8_t txData[3] = {0x03, 0x00, 0x00};  // Opcode = 0x03, NOP, NOP
	uint8_t rxData[3] = {0x00, 0x00, 0x00};

	// SPI-Übertragung und Empfang
	HAL_SPI_Transmit(&hspi1,txData, 3, HAL_MAX_DELAY);   // Lese die Antwort
	HAL_SPI_Receive(&hspi1, rxData, 3, HAL_MAX_DELAY);   // Lese die Antwort

	// SPI_TransmitReceiveWithDebug(hspi, txData[0], rxData, 3, "GetPacketType: ");
	// Der Pakettyp befindet sistatusch im dritten Byte (rxData[2])
	uint8_t packetType = rxData[2];

	// Debug-Ausgabe
	switch (packetType) {
	case 0x00:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Packet Type: GFSK (0x00)\r\n", 30, 100);
		break;
	case 0x01:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Packet Type: LoRa (0x01)\r\n", 30, 100);

		break;
	case 0x02:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Packet Type: Ranging (0x02)\r\n", 30, 100);

		break;
	case 0x03:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Packet Type: FLRC (0x03)\r\n", 30, 100);

		break;
	case 0x04:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Packet Type: BLE (0x04)\r\n", 30, 100);

		break;
	default:
		HAL_UART_Transmit(&huart2, (uint8_t *)"Unknown Packet Type:", 20, 100);
		HAL_UART_Transmit(&huart2, &packetType, 1, 100);
		HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 4, 100);

		break;
	}
}


// Funktion zum Setzen der Frequenz
void setFrequency() {
	uint8_t tx[4] = {0x86, 0xB8, 0x9D, 0x89}; // Frequenzdaten
	uint8_t rx[4] = {0};
	SPI_TransmitReceiveWithDebug(&hspi1, tx, rx, 4, "Set Frequency\r\n");  // SPI mit Debugger
	HAL_Delay(10);

}

// Funktion zum Setzen der Basisadresse des Buffers
void setBufferBaseAddress() {
	uint8_t tx[3] = {0x8F, 0x80, 0x00}; // Basisadresse
	uint8_t rx[3] = {0};
	SPI_TransmitReceiveWithDebug(&hspi1, tx, rx, 3, "Set Buffer Base Address\r\n");  // SPI mit Debugger
	HAL_Delay(10);

}

// Funktion zum Setzen der Modulationsparameter
void setModulationParams() {
	uint8_t tx[4] = {0x8B, 0x70, 0x18, 0x01}; // Modulationsparameter
	uint8_t rx[4] = {0};
	SPI_TransmitReceiveWithDebug(&hspi1, tx, rx, 4, "Set Modulation Params\r\n");  // SPI mit Debugger
	HAL_Delay(10);
}

// Funktion zum Setzen der Paketparameter
void setPacketParams() {
	uint8_t tx[8] = {0x8C, 0x0C, 0x80, 0x08, 0x20, 0x40, 0x00, 0x00}; // Paketparameter
	uint8_t rx[8] = {0};
	SPI_TransmitReceiveWithDebug(&hspi1, tx, rx, 8, "Set Packet Params\r\n");  // SPI mit Debugger
	HAL_Delay(10);
}

// Funktion zum Setzen der IRQ-Parameter (DIO1, DIO2, DIO3)
//void setDioIrqParams() {
//	uint8_t tx[9] = {0x8E, 0x1F, 0xE0, 0x00, 0x01, 0x00, 0x02, 0x40, 0x20}; // IRQ Masken
//	uint8_t rx[9] = {0};
//	SPI_TransmitReceiveWithDebug(&hspi1, tx, rx, 9, "Set DIO IRQ Params\r\n");  // SPI mit Debugger
//	HAL_Delay(10);
//}

// Funktion zum Setzen der RX-Periode (Empfangsparameter)
void setTxParams() {
	uint8_t tx[3] = {0x8E, 0x1F, 0xE0}; // TX Periode
	uint8_t rx[3] = {0};
	SPI_TransmitReceiveWithDebug(&hspi1, tx, rx, 3, "Set TX Period\r\n");  // SPI mit Debugger
	HAL_Delay(10);
}
void setTx() {
	uint8_t tx[4] = {0x83, 0x00, 0x00, 0x00}; // TX Periode
	uint8_t rx[4] = {0};
	SPI_TransmitReceiveWithDebug(&hspi1, tx, rx, 4, "Set TX\r\n");  // SPI mit Debugger
	HAL_Delay(10);
}
void InterruptSetting() {
    uint8_t tx[3] = {0};
    uint8_t rx[3] = {0};

    // SPI-Befehl: ClrIrqStatus
    tx[0] = 0x97;          // Opcode für ClrIrqStatus
    tx[1] = 0xFF;          // Alle Interrupt-Bits löschen (High-Byte)
    tx[2] = 0xFF;          // Alle Interrupt-Bits löschen (Low-Byte)

    // Sende SPI-Befehl
	SPI_TransmitReceiveWithDebug(&hspi1, tx, rx, 3, "Clear interrupt Settings\r\n");  // SPI mit Debugger
	HAL_Delay(10);

}
void setDioIrqParams(uint16_t irqMask, uint16_t dioMask, uint16_t dio1Mask, uint16_t dio2Mask) {
    uint8_t tx[10] = {0};
    uint8_t rx[10] = {0};

    // SPI-Befehl: SetDioIrqParams
    tx[0] = 0x8D;               // Opcode für SetDioIrqParams
    tx[1] = (irqMask >> 8);     // IRQ-Maske (High-Byte)
    tx[2] = (irqMask & 0xFF);   // IRQ-Maske (Low-Byte)
    tx[3] = (dioMask >> 8);     // DIO-Maske (High-Byte)
    tx[4] = (dioMask & 0xFF);   // DIO-Maske (Low-Byte)
    tx[5] = (dio1Mask >> 8);    // DIO1-Maske (High-Byte)
    tx[6] = (dio1Mask & 0xFF);  // DIO1-Maske (Low-Byte)
    tx[7] = (dio2Mask >> 8);    // DIO2-Maske (High-Byte)
    tx[8] = (dio2Mask & 0xFF);  // DIO2-Maske (Low-Byte)

    // Sende SPI-Befehl
	SPI_TransmitReceiveWithDebug(&hspi1, tx, rx, 9, "Set Irq Params\r\n");  // SPI mit Debugger
	HAL_Delay(10);

}
void DerTakt()
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // NSS auf HIGH
	nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));
	HAL_Delay(2);
	SPI_WaitUntilReady(&hspi1);
	HAL_Delay(2);
	SPI_WaitTransmit(&hspi1);

}
//
//void SetTxParams(uint8_t power, uint8_t rampTime) {
//    uint8_t tx[3] = {0};
//    uint8_t rx[3] = {0};
//
//    // SPI-Befehl: SetTxParams
//    tx[0] = 0x8E;          // Opcode für SetTxParams
//    tx[1] = power;         // Sendeleistung (0–22 dBm, je nach Hardware)
//    tx[2] = rampTime;      // Rampenzeit (z. B. RADIO_RAMP_02_US)
//
//    // Sende SPI-Befehl
//	HAL_SPI_TransmitReceive(&hspi1, tx, rx, 3, HAL_MAX_DELAY);
//}

//void WriteCommand(uint8_t opcode, uint8_t *buffer, uint16_t size) {
//    uint8_t merged_buf[size + 1];
//    merged_buf[0] = opcode;
//    memcpy(merged_buf + 1, buffer, size);
//
//    DerTakt();
//	HAL_SPI_Transmit(&hspi1, buf_out, sizeof(buf_out), HAL_MAX_DELAY);
//}
//
//void SetTx(TickTime_t timeout) {
//    uint8_t buf[3];
//    buf[0] = timeout.PeriodBase;
//    buf[1] = (uint8_t)((timeout.PeriodBaseCount >> 8) & 0x00FF);
//    buf[2] = (uint8_t)(timeout.PeriodBaseCount & 0x00FF);
//
//    InterruptSetting();
//
//    //HalPostRx();
//    //HalPreTx();
//    WriteCommand(RADIO_SET_TX, buf, sizeof(buf));
//    //OperatingMode = MODE_TX;
//}

void WriteBuffer(uint8_t offset, uint8_t *data, uint8_t length) {
    uint8_t txBuffer[length + 2];
    uint8_t rxBuffer[length + 2];  // Empfangspuffer für Status

    // Erstelle das Sendepaket
    txBuffer[0] = 0x1A;  // Opcode
    txBuffer[1] = offset;               // Offset
    for (uint8_t i = 0; i < length; i++) {
        txBuffer[i + 2] = data[i];      // Nutzdaten
    }

    // Sende das Paket und empfange Status
    HAL_SPI_TransmitReceive(&hspi1, txBuffer, rxBuffer, length + 2, HAL_MAX_DELAY);

    // Statusprüfung (optional)
    for (uint8_t i = 0; i < length + 2; i++) {
        HAL_UART_Transmit(&huart2,(uint8_t*)&rxBuffer[i], 1, HAL_MAX_DELAY);
        HAL_UART_Transmit(&huart2, (uint8_t*)" ", 1, HAL_MAX_DELAY);

    }
    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n ", 4, HAL_MAX_DELAY);
	HAL_Delay(10);

}

void SendPayload( uint8_t offset, uint8_t *data, uint8_t length) {
    WriteBuffer(offset, data, length);
    DerTakt();
	setTx();

}

// Die Funktion zum Einfügen eines Pakets
uint8_t PutPacket(uint8_t* in)
{
    //TickTime_t timeout = {RADIO_TICK_SIZE_1_US,0};  // Beispielwerte, anpassen
    uint8_t offset = 0x80;
    // Fehlerbehandlung für NULL-Pointer
    if (in == NULL) {
        return NULL_POINTER; // Eingabepuffer ist NULL
    }

    // FIFO-Überlaufprüfung
    if (tx_length == FIFO_SIZE) {
        return FIFO_OVERFLOW; // FIFO ist voll
    }

    // Paket in FIFO einfügen
    for (int i = 0; i < PACKET_SIZE; i++) {
        *(tx_eprt + i) = *(in + i);
    }

    // FIFO-Pointer zyklisch bewegen
    if (tx_eprt == &tx_fifo[FIFO_SIZE - 1][0]) {
        tx_eprt = tx_fifo[0];  // Zurück zum Anfang des FIFOs
    } else {
        tx_eprt += PACKET_SIZE; // Nächstes Paket im FIFO
    }

    // FIFO-Länge erhöhen
    tx_length++;

    // Wenn der Transmitter noch nicht aktiviert ist, beginne mit der Übertragung
    if (tx_length == 1) {  // Erster Paket, Sendeanforderung
        // SendPayload aufrufen, um das erste Paket zu übertragen
    	SendPayload(offset, tx_fifo[0], PACKET_SIZE);
    }
	HAL_Delay(10);

    return 0; // Erfolgreich eingefügt
}





/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	//Erase_Flash();

    uint8_t data[PACKET_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8};  // Beispiel-Daten
	int i=0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start(&htim1);
	ResetChip(GPIOC, GPIO_PIN_7);
	HAL_Delay(5);

	/*=======================================================================*/
	//NSSNSSNSSNSSNSSNSSNSSNSSNSSNSSNSSNSSNSSNSS
	/*=======================================================================*/

	// NSS Test
	SelectChip(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
	nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));

	HAL_Delay(5);

	SelectChip(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
	nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));

	HAL_Delay(5);

	SelectChip(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
	nss = uint8_t(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6));
	//SPI_WaitUntilReady(&hspi1);

	SPI_WaitTransmit(&hspi1);


	SetStandby();
	HAL_Delay(5);

	DerTakt();



   // PutPacket(data);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{

		while(i!=3)
		{
		GetStatus();

		DerTakt();

		setLoRaMode();
		// LoRa Modus setzen
		DerTakt();

		getPacketType(&hspi1);

		DerTakt();

		setTx();

		DerTakt();

		GetStatus();

		DerTakt();

		setFrequency();
		// Frequenz setzen
		DerTakt();


		setBufferBaseAddress(); // Basisadresse setzen

		DerTakt();


		setModulationParams();  // Modulationsparameter setzen

		DerTakt();


		setPacketParams();

		DerTakt();

		// Paketparameter setzen


		//SPI_SetStandby();
		//SPI_WaitTransmit(&hspi1);

	//	setTxParams();
	//	DerTakt();

		InterruptSetting();
		DerTakt();

		setDioIrqParams(0xFFFF, 0xFFFF, 0x0000, 0x0000);

		DerTakt();

		i++;
		HAL_UART_Transmit(&huart2,(uint8_t *)"\r\n" ,4, HAL_MAX_DELAY);


	    HAL_Delay(500);

		}


    	PutPacket(data);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 80;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */