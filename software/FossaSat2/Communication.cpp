#include "Communication.h"

void Communication_Receive_Interrupt() {
  // check interrups are enabled
  if (!interruptsEnabled) {
    return;
  }

  // set flag
  dataReceived = true;
}

void Communication_Change_Modem(HardwareTimer* tmr) {
  (void)tmr;

  // check interrups are enabled
  if (!interruptsEnabled) {
    return;
  }

  // set flag
  switchModem = true;
}

void Communication_Set_Modem(uint8_t modem) {
  int16_t state = ERR_NONE;
  FOSSASAT_DEBUG_PRINT(F("Set modem "));
  FOSSASAT_DEBUG_PRINTLN(modem);

  // initialize requested modem
  switch (modem) {
    case MODEM_LORA:
      // config function for Lora is already available due to custom retransmission
      state = Communication_Set_LoRa_Configuration(LORA_BANDWIDTH,
              LORA_SPREADING_FACTOR,
              LORA_CODING_RATE,
              LORA_PREAMBLE_LENGTH,
              true,
              LORA_OUTPUT_POWER);
      break;
    case MODEM_FSK: {
        state = radio.beginFSK(CARRIER_FREQUENCY,
                               FSK_BIT_RATE,
                               FSK_FREQUENCY_DEVIATION,
                               FSK_RX_BANDWIDTH,
                               FSK_OUTPUT_POWER,
                               FSK_CURRENT_LIMIT,
                               FSK_PREAMBLE_LENGTH,
                               FSK_DATA_SHAPING);
        const uint8_t syncWordFSK[] = FSK_SYNC_WORD;
        radio.setSyncWord((uint8_t*)syncWordFSK, sizeof(syncWordFSK) / sizeof(uint8_t));
        radio.setCRC(2);
      } break;
    default:
      FOSSASAT_DEBUG_PRINT(F("Unkown modem "));
      FOSSASAT_DEBUG_PRINTLN(modem);
      return;
  }

  // handle possible error codes
  FOSSASAT_DEBUG_PRINT(F("Radio init done, code "));
  FOSSASAT_DEBUG_PRINTLN(state);
  if ((state == ERR_CHIP_NOT_FOUND) || (state == ERR_SPI_CMD_FAILED) || (state == ERR_SPI_CMD_INVALID)) {
    // radio chip was not found, restart
#ifdef ENABLE_RADIO_ERROR_RESET
    PowerControl_Watchdog_Restart();
#endif
  }

  // set TCXO
  radio.setTCXO(TCXO_VOLTAGE);
}

int16_t Communication_Set_LoRa_Configuration(float bw, uint8_t sf, uint8_t cr, uint16_t preambleLen, bool crc, int8_t power) {
  // set LoRa radio config
  int16_t state = radio.begin(CARRIER_FREQUENCY, bw, sf, cr, LORA_SYNC_WORD, power, LORA_CURRENT_LIMIT, preambleLen);
  if (state != ERR_NONE) {
    return (state);
  }

  // set CRC
  state = radio.setCRC(crc);
  return (state);
}

void Communication_System_Info_Add(uint8_t** buffPtr, uint8_t val, const char* name, uint32_t mult, const char* unit) {
  memcpy(*buffPtr, &val, sizeof(uint8_t));
  (*buffPtr) += sizeof(uint8_t);
  FOSSASAT_DEBUG_PRINT(name);
  FOSSASAT_DEBUG_PRINT(F(" = "));
  FOSSASAT_DEBUG_PRINT(val);
  FOSSASAT_DEBUG_PRINT('*');
  FOSSASAT_DEBUG_PRINT(mult);
  FOSSASAT_DEBUG_PRINT(' ');
  FOSSASAT_DEBUG_PRINTLN(unit);

}

void Communication_Send_System_Info() {
  // build response frame
  static const uint8_t optDataLen = sizeof(uint8_t);
  uint8_t optData[optDataLen];
  uint8_t* optDataPtr = optData;

  FOSSASAT_DEBUG_PRINTLN(F("--- System info: ---"));

  // set mpptOutputVoltage variable
  uint8_t mpptOutputVoltage = currSensorMPPT.readBusVoltage() * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
  Communication_System_Info_Add(&optDataPtr, mpptOutputVoltage, "mpptOutputVoltage", VOLTAGE_MULTIPLIER, "mV");

  FOSSASAT_DEBUG_PRINTLN(F("--------------------"));

  // TODO add more info

  // send response
  Communication_Send_Response(RESP_SYSTEM_INFO, optData, optDataLen);
}

void Communication_Send_Morse_Beacon() {
  // check transmit enable flag
#ifdef ENABLE_TRANSMISSION_CONTROL
  if (!PersistentStorage_Read_Internal<uint8_t>(EEPROM_TRANSMISSIONS_ENABLED)) {
    FOSSASAT_DEBUG_PRINTLN(F("Tx off by cmd"));
    return;
  }
#endif

  // set modem to FSK
  uint8_t modem = currentModem;
  if (modem != MODEM_FSK) {
    Communication_Set_Modem(MODEM_FSK);
  }

  // read callsign
  uint8_t callsignLen = PersistentStorage_Read_Internal<uint8_t>(EEPROM_CALLSIGN_LEN_ADDR);
  char callsign[MAX_STRING_LENGTH + 1];
  PersistentStorage_Get_Callsign(callsign, callsignLen);

  // TODO measure battery voltage
  float batt = 0;//currSensorMPPT.readBusVoltage();
  char battStr[6];
  dtostrf(batt, 5, 3, battStr);

  // build Morse beacon frame
  char morseFrame[MAX_OPT_DATA_LENGTH];
  sprintf(morseFrame, "%s %s", callsign, battStr);
  FOSSASAT_DEBUG_PRINT(F("Morse beacon data: "));
  FOSSASAT_DEBUG_PRINTLN(morseFrame);

  // send Morse data
  int16_t state = morse.begin(CARRIER_FREQUENCY, MORSE_SPEED);
  FOSSASAT_DEBUG_PRINT(F("Morse init done, code "));
  FOSSASAT_DEBUG_PRINTLN(state);
  if (state == ERR_NONE) {
    FOSSASAT_DEBUG_PRINTLN(F("Sending data"));
    FOSSASAT_DEBUG_STOPWATCH_START();

    // send preamble
    for (uint8_t i = 0; i < MORSE_PREAMBLE_LENGTH; i++) {
      morse.startSignal();
      FOSSASAT_DEBUG_PRINT('x');
      PowerControl_Watchdog_Heartbeat();
      PowerControl_Wait(100, LOW_POWER_NONE);
    }

    // print frame single symbol at a time to reset watchdog
    for (uint8_t i = 0; i < strlen(morseFrame); i++) {
      morse.print(morseFrame[i]);
      FOSSASAT_DEBUG_PRINT(morseFrame[i]);
      PowerControl_Watchdog_Heartbeat();
    }
    morse.println();
    FOSSASAT_DEBUG_PRINTLN('+');
    PowerControl_Watchdog_Heartbeat();

    FOSSASAT_DEBUG_PRINTLN(F("Done!"));
    FOSSASAT_DEBUG_STOPWATCH_STOP();
  }

  // set modem back to previous configuration
  if (modem != MODEM_FSK) {
    Communication_Set_Modem(modem);
  }
}

void Comunication_Parse_Frame(uint8_t* frame, uint8_t len) {
  // get callsign from EEPROM
  uint8_t callsignLen = PersistentStorage_Read_Internal<uint8_t>(EEPROM_CALLSIGN_LEN_ADDR);
  char callsign[MAX_STRING_LENGTH];
  PersistentStorage_Get_Callsign(callsign, callsignLen);

  // check callsign
  if (memcmp(frame, (uint8_t*)callsign, callsignLen - 1) != 0) {
    // check failed
    FOSSASAT_DEBUG_PRINT(F("Callsign mismatch, expected "));
    FOSSASAT_DEBUG_PRINTLN(callsign);
    return;
  }

  // get functionID
  int16_t functionId = FCP_Get_FunctionID(callsign, frame, len);
  if (functionId < 0) {
    FOSSASAT_DEBUG_PRINT(F("Unable to get function ID 0x"));
    FOSSASAT_DEBUG_PRINTLN(functionId, HEX);
    return;
  }
  FOSSASAT_DEBUG_PRINT(F("Function ID = 0x"));
  FOSSASAT_DEBUG_PRINTLN(functionId, HEX);

  // check encryption
  int16_t optDataLen = 0;
  uint8_t optData[MAX_OPT_DATA_LENGTH];
  if (functionId >= PRIVATE_OFFSET) {
    // private frame, decrypt

  } else {
    // no decryption necessary

    // get optional data length
    optDataLen = FCP_Get_OptData_Length(callsign, frame, len);
    if (optDataLen < 0) {
      // optional data extraction failed,
      FOSSASAT_DEBUG_PRINT(F("Failed to get optDataLen, code "));
      FOSSASAT_DEBUG_PRINTLN(optDataLen);
      return;
    }

    // get optional data
    if (optDataLen > 0) {
      FCP_Get_OptData(callsign, frame, len, optData);
    }
  }

  // check optional data presence
  if (optDataLen > 0) {
    // execute with optional data
    FOSSASAT_DEBUG_PRINT(F("optDataLen = "));
    FOSSASAT_DEBUG_PRINTLN(optDataLen);
    FOSSASAT_DEBUG_PRINT_BUFF(optData, (uint8_t)optDataLen);
    Communication_Execute_Function(functionId, optData, optDataLen);

  } else {
    // execute without optional data
    Communication_Execute_Function(functionId);
  }

}

void Communication_Execute_Function(uint8_t functionId, uint8_t* optData, size_t optDataLen) {
  // execute function based on ID
  switch (functionId) {

    // public function IDs

    case CMD_PING:
      // send pong
      Communication_Send_Response(RESP_PONG);
      break;

    case CMD_RETRANSMIT: {
        // check message length
        if (optDataLen <= MAX_OPT_DATA_LENGTH) {
          // respond with the requested data
          Communication_Send_Response(RESP_REPEATED_MESSAGE, optData, optDataLen);
        }
      } break;

    case CMD_RETRANSMIT_CUSTOM: {
        // check message length
        if ((optDataLen >= 8) && (optDataLen <= MAX_STRING_LENGTH + 7)) {
          // check bandwidth value (loaded from array - rest of settings are checked by library)
          if (optData[0] > 7) {
            FOSSASAT_DEBUG_PRINT(F("Invalid BW "));
            FOSSASAT_DEBUG_PRINTLN(optData[0]);
            break;
          }

          // attempt to change the settings
          float bws[] = {7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0};
          uint16_t preambleLength = 0;
          memcpy(&preambleLength, optData + 3, sizeof(uint16_t));

          // change modem configuration
          int16_t state = Communication_Set_LoRa_Configuration(bws[optData[0]], optData[1], optData[2], preambleLength, optData[5], optData[6]);

          // check if the change was successful
          if (state != ERR_NONE) {
            FOSSASAT_DEBUG_PRINT(F("Custom config failed, code "));
            FOSSASAT_DEBUG_PRINTLN(state);
          } else {
            // configuration changed successfully, transmit response
            Communication_Send_Response(RESP_REPEATED_MESSAGE_CUSTOM, optData + 7, optDataLen - 7, true);
          }
        }
      } break;

    case CMD_TRANSMIT_SYSTEM_INFO:
      // send system info via LoRa
      Communication_Send_System_Info();
      break;

    case CMD_GET_LAST_PACKET_INFO: {
        // get last packet info and send it
        uint8_t respOptData[] = {(uint8_t)(radio.getSNR() * 4.0), (uint8_t)(radio.getRSSI() * -2.0)};
        Communication_Send_Response(RESP_LAST_PACKET_INFO, respOptData, 2);
      } break;

    // TODO new public frames

    // TODO private function IDs

    default:
      FOSSASAT_DEBUG_PRINT(F("Unknown function ID!"));
      return;
  }
}

int16_t Communication_Send_Response(uint8_t respId, uint8_t* optData, size_t optDataLen, bool overrideModem) {
  // get callsign from EEPROM
  uint8_t callsignLen = PersistentStorage_Read_Internal<uint8_t>(EEPROM_CALLSIGN_LEN_ADDR);
  char callsign[MAX_STRING_LENGTH];
  PersistentStorage_Get_Callsign(callsign, callsignLen);

  // build response frame
  uint8_t len = FCP_Get_Frame_Length(callsign, optDataLen);
  uint8_t frame[MAX_RADIO_BUFFER_LENGTH];
  FCP_Encode(frame, callsign, respId, optDataLen, optData);

  // send response
  return (Communication_Transmit(frame, len, overrideModem));
}

int16_t Communication_Transmit(uint8_t* data, uint8_t len, bool overrideModem) {
  // check transmit enable flag
#ifdef ENABLE_TRANSMISSION_CONTROL
  if (!PersistentStorage_Read_Internal<uint8_t>(EEPROM_TRANSMISSIONS_ENABLED)) {
    FOSSASAT_DEBUG_PRINTLN(F("Tx off by cmd"));
    return (ERR_TX_TIMEOUT);
  }
#endif

  // disable receive interrupt
  detachInterrupt(digitalPinToInterrupt(RADIO_DIO1));

  // print frame for debugging
  FOSSASAT_DEBUG_PRINT(F("Sending LoRa frame, len = "));
  FOSSASAT_DEBUG_PRINTLN(len);
  FOSSASAT_DEBUG_PRINT_BUFF(data, len);

  // send frame by non-ISM LoRa
  uint8_t modem = currentModem;

  // check if modem should be switched - required for transmissions with custom settings
  if (!overrideModem) {
    Communication_Set_Modem(MODEM_LORA);
  }

  // get timeout
  uint32_t timeout = (float)radio.getTimeOnAir(len) * 1.5;

  // start transmitting
  int16_t state = radio.startTransmit(data, len);
  if (state != ERR_NONE) {
    FOSSASAT_DEBUG_PRINT(F("Tx failed "));
    FOSSASAT_DEBUG_PRINTLN(state);
    return (state);
  }

  // wait for transmission finish
  uint32_t start = micros();
  uint32_t lastBeat = 0;
  while (!digitalRead(RADIO_DIO1)) {
    // pet watchdog every second
    if (micros() - lastBeat > (uint32_t)WATCHDOG_LOOP_HEARTBEAT_PERIOD * (uint32_t)1000) {
      PowerControl_Watchdog_Heartbeat();
      lastBeat = micros();
    }

    // check timeout
    if (micros() - start > timeout) {
      // timed out while transmitting
      radio.standby();
      Communication_Set_Modem(modem);
      FOSSASAT_DEBUG_PRINT(F("Tx timeout"));
      return (ERR_TX_TIMEOUT);
    }
  }

  // transmission done, set mode standby
  state = radio.standby();
  Communication_Set_Modem(modem);

  // set receive ISR
  radio.setDio1Action(Communication_Receive_Interrupt);
  radio.startReceive();

  return (state);
}
