/*
 * modbusRTU.c
 *
 *  Created on: 13.02.2023
 *      Author: lukas
 */
#include "modbusRTU.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <shalf1.h>
#include <rs485uart.h>



//global variables
static uint16_t registers[2] = {69, 96};
static uint8_t deviceAddress =1;
static uint8_t readData[8];
static uint8_t slaveAdr = 0;
static uint8_t funcCode = 0;
static uint16_t regAdr = 0;
static uint16_t payload = 0;
static uint8_t responseLen = 0;
static uint8_t idx = 0;
static uint16_t buff = 0;
static uint16_t crc = 0;
static uint16_t crcReceived = 0;

extern uint16_t registerRead(uint8_t regAdr){
	if(regAdr < 2){
		return registers[regAdr];
	}
	else{
		return regOutOfBound;
	}
}

extern modbusErrCode registerWrite(uint8_t regAdr, uint16_t regData){
	if(regAdr < 2){
			registers[regAdr] = regData;
			return modbusOK;
	}
	else{
		return regOutOfBound;
	}
}

extern modbusErrCode setSlaveAddress(uint8_t address){
	deviceAddress = address;
	return modbusOK;
}

extern uint16_t modbusCRC(uint8_t *data, uint8_t len){
	uint16_t crc = ~0x0000;
	uint8_t i;
	uint8_t b;
	for(i = 0; i < len; i++){
		crc ^= data[i];
		for(b = 0; b < 8; b++){
			if((crc & 1) != 0){
				crc = (crc>>1)^0xA001;
			}
			else{
				crc >>= 1;
			}
		}
	}
	return crc;
}

extern bool modbusCheckCRC(){
	crc = modbusCRC(readData, 6);
	if(crc == crcReceived){
		crc = 0;
		return true;
	}
	else{
		crc = 0;
		return false;
	}
}

extern void modbusResponse(char *data, uint8_t len){
	slaveAdr = 0;
	funcCode = 0;
	regAdr = 0;
	payload = 0;
	responseLen = 0;
	idx = 0;
	buff = 0;
	crc = 0;
	memcpy(readData, data, 6);
	slaveAdr = readData[0]; //Bit shifting überprüfen! -> passt!
	funcCode = readData[1];
	regAdr = readData[2] << 8;
	regAdr |= readData[3];
	payload = readData[4] << 8;
	payload |= readData[5];
	crcReceived = readData[6] << 8;
	crcReceived |= readData[7];
	if(slaveAdr != deviceAddress){
		return;
	}
	if(!(modbusCheckCRC())){
		return;
	}
	switch(funcCode){
	case 0x03: //read Holdregisters
		if((regAdr == 0x00) || (regAdr == 0x01)){
			responseLen = 2*payload+5;
			char response[responseLen];
			response[0] = deviceAddress;
			response[1] = 0x03;
			response[2] = 2*payload+5;
			idx = 3;
			uint8_t i;
			//uint16_t buff;
			for(i=payload; i > 0; i--){
				buff = registerRead(i-1);
				response[idx++] = (buff >> 8) & 0xFF;
				response[idx++] = buff;
			}
			crc = modbusCRC(response, responseLen-2);
			response[responseLen-2] = crc;
			response[responseLen-1] = crc >> 8;
			USARTSendStringMB(USART1, response, responseLen+1);
		}
		break;
	case 0x06: //set a analog read register
		if((regAdr == 0x00) || (regAdr == 0x01)){
			registerWrite(regAdr, payload);
			responseLen = 2*1+5;
			char response[responseLen];
			response[0] = deviceAddress;
			response[1] = 0x06;
			response[2] = responseLen;
			idx = 3;
			//uint16_t buff;
			buff = registerRead(regAdr);
			response[3] = (buff >> 8) & 0xFF;
			response[4] = buff;
			crc = modbusCRC(response, responseLen-2);
			response[responseLen-2] = crc;
			response[responseLen-1] = crc >> 8;
			USARTSendStringMB(USART1, response, responseLen+1);
		}
		break;
	case 0x69: //set new modbus adr
		if((payload >= 0)&&(payload <= 255)){
			setSlaveAddress(payload & 0x00FF);
			responseLen = 2*1+5;
			char response[responseLen];
			response[0] = deviceAddress;
			response[1] = 0x69;
			response[2] = responseLen;
			response[3] = 0x00;
			response[4] = payload & 0x00FF;
			crc = modbusCRC(response, responseLen-2);
			response[responseLen-2] = crc;
			response[responseLen-1] = crc >> 8;
			USARTSendStringMB(USART1, response, responseLen+1);
		}
		break;
	default:
		return;
		break;
	}
}


//TODO
//-function for checking crc
//-implement more fuccodes
