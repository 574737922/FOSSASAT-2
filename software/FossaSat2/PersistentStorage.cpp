#include "PersistentStorage.h"

void PersistentStorage_Update_Stats(uint8_t flags) {
  if(flags & 0b00000001) {
    // temperatures
    int16_t tempValue = Sensors_Read_Temperature(tempSensorPanelY) * (TEMPERATURE_UNIT / TEMPERATURE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_TEMP_PANEL_Y, tempValue);
    
    tempValue = Sensors_Read_Temperature(tempSensorTop) * (TEMPERATURE_UNIT / TEMPERATURE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_TEMP_TOP, tempValue);
    
    tempValue = Sensors_Read_Temperature(tempSensorBottom) * (TEMPERATURE_UNIT / TEMPERATURE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_TEMP_BOTTOM, tempValue);
    
    tempValue = Sensors_Read_Temperature(tempSensorBattery) * (TEMPERATURE_UNIT / TEMPERATURE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_TEMP_BATTERY, tempValue);
    
    tempValue = Sensors_Read_Temperature(tempSensorSecBattery) * (TEMPERATURE_UNIT / TEMPERATURE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_TEMP_SEC_BATTERY, tempValue);
  }

  if(flags & 0b00000010) {
    // currents
    int16_t currentValue = currSensorXA.readCurrent() * (CURRENT_UNIT / CURRENT_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_XA, currentValue);

    currentValue = currSensorXB.readCurrent() * (CURRENT_UNIT / CURRENT_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_XB, currentValue);

    currentValue = currSensorZA.readCurrent() * (CURRENT_UNIT / CURRENT_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_ZA, currentValue);

    currentValue = currSensorZB.readCurrent() * (CURRENT_UNIT / CURRENT_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_ZB, currentValue);

    currentValue = currSensorY.readCurrent() * (CURRENT_UNIT / CURRENT_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_Y, currentValue);

    currentValue = currSensorMPPT.readCurrent() * (CURRENT_UNIT / CURRENT_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_MPPT, currentValue);
  }

  if(flags & 0b00000100) {
    // voltages
    uint8_t voltageValue = currSensorXA.readBusVoltage() * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_XA, voltageValue);

    voltageValue = currSensorXB.readBusVoltage() * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_XB, voltageValue);
    
    voltageValue = currSensorZA.readBusVoltage() * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_ZA, voltageValue);
    
    voltageValue = currSensorZB.readBusVoltage() * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_ZB, voltageValue);
    
    voltageValue = currSensorY.readBusVoltage() * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_Y, voltageValue);
    
    voltageValue = currSensorMPPT.readBusVoltage() * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
    PersistentStorage_Update_Stat(FLASH_STATS_CURR_MPPT, voltageValue);
  }

  if(flags & 0b00001000) {
    // lights
    PersistentStorage_Update_Stat(FLASH_STATS_LIGHT_PANEL_Y, lightSensorPanelY.readLux());
    PersistentStorage_Update_Stat(FLASH_STATS_LIGHT_TOP, lightSensorTop.readLux());
  }
}

void PersistentStorage_Increment_Counter(uint16_t addr) {
  uint16_t counter = PersistentStorage_Get<uint16_t>(addr);
  counter++;
  PersistentStorage_Set(addr, counter);
}

void PersistentStorage_Increment_Frame_Counter(bool valid) {
  uint16_t addr = FLASH_LORA_VALID_COUNTER;
  if(currentModem == MODEM_LORA) {
    if(!valid) {
      addr += 2;
    }
  } else {
    if(valid) {
      addr += 4;
    } else {
      addr += 6;
    }
  }

  PersistentStorage_Increment_Counter(addr);
}

void PersistentStorage_Get_Callsign(char* buff, uint8_t len) {
  PersistentStorage_Read(FLASH_CALLSIGN, (uint8_t*)buff, len);
  buff[len] = '\0';
}

void PersistentStorage_Set_Callsign(char* newCallsign) {
  // get length of the new callsign
  uint8_t newCallsignLen = (uint8_t)strlen(newCallsign);

  // check new callsign length
  if (newCallsignLen > MAX_STRING_LENGTH) {
    FOSSASAT_DEBUG_PRINTLN(F("New callsign too long!"));
    return;
  }

  // read the current system info page
  uint8_t sysInfoPage[FLASH_SYSTEM_INFO_LEN];
  PersistentStorage_Read(FLASH_SYSTEM_INFO_START, sysInfoPage, FLASH_SYSTEM_INFO_LEN);

  // update callsign entries
  sysInfoPage[FLASH_CALLSIGN_LEN] = newCallsignLen;
  memcpy(sysInfoPage + FLASH_CALLSIGN, newCallsign, newCallsignLen);
  PersistentStorage_Write(FLASH_SYSTEM_INFO_START, sysInfoPage, FLASH_SYSTEM_INFO_LEN);
}

uint32_t PersistentStorage_Get_Image_Len(uint8_t slot) {
  uint8_t buff[4];
  uint32_t addr = FLASH_IMAGE_LENGTHS_1;
  if(slot >= 64) {
    addr = FLASH_IMAGE_LENGTHS_2;
    slot -= 64;
  }
  PersistentStorage_Read(addr + slot*sizeof(uint32_t), buff, sizeof(uint32_t));
  uint32_t len;
  memcpy(&len, buff, sizeof(uint32_t));
  return(len);
}

void PersistentStorage_Set_Image_Len(uint8_t slot, uint32_t len) {
  // read the correct page
  uint8_t buff[FLASH_EXT_PAGE_SIZE];
  uint32_t addr = FLASH_IMAGE_LENGTHS_1;
  if(slot >= 64) {
    addr = FLASH_IMAGE_LENGTHS_2;
    slot -= 64;
  }
  PersistentStorage_Read(addr, buff, FLASH_EXT_PAGE_SIZE);
  
  // update value
  memcpy(buff + slot*sizeof(uint32_t), &len, sizeof(uint32_t));

  // write it back in (will automatically erase the sector)
  PersistentStorage_Write(addr, buff, FLASH_EXT_PAGE_SIZE);
}

void PersistentStorage_Set_Buffer(uint8_t addr, uint8_t* buff, uint8_t len) {
  // check address is in system info
  if(addr > FLASH_SYSTEM_INFO_LEN) {
    return;
  }
  
  // read the current system info page
  uint8_t currSysInfoPage[FLASH_SYSTEM_INFO_LEN];
  PersistentStorage_Read(FLASH_SYSTEM_INFO_START, currSysInfoPage, FLASH_SYSTEM_INFO_LEN);

  // check if we need to update
  uint8_t newSysInfoPage[FLASH_SYSTEM_INFO_LEN];
  memcpy(newSysInfoPage, currSysInfoPage, FLASH_SYSTEM_INFO_LEN);
  memcpy(newSysInfoPage + addr, buff, len);
  if(memcmp(currSysInfoPage, newSysInfoPage, FLASH_SYSTEM_INFO_LEN) == 0) {
    // the value is already there, no need to write
    return;
  }

  // we need to update
  PersistentStorage_Write(FLASH_SYSTEM_INFO_START, newSysInfoPage, FLASH_SYSTEM_INFO_LEN);
}

void PersistentStorage_Reset_System_Info() {
  // build a completely new system info page
  uint8_t sysInfoPage[FLASH_SYSTEM_INFO_LEN];
  uint8_t* sysInfoPagePtr = sysInfoPage;

  // set everything to 0 by default
  memset(sysInfoPage, 0, FLASH_SYSTEM_INFO_LEN);

  // set non-zero defaults

  // set default transmission configuration
  sysInfoPage[FLASH_TRANSMISSIONS_ENABLED] = 1;

  // set default callsign length
  sysInfoPage[FLASH_CALLSIGN_LEN] = strlen(CALLSIGN_DEFAULT);

  // set default callsign
  memcpy(sysInfoPagePtr + FLASH_CALLSIGN, CALLSIGN_DEFAULT, strlen(CALLSIGN_DEFAULT));

  // set default receive windows
  sysInfoPage[FLASH_FSK_RECEIVE_LEN] = FSK_RECEIVE_WINDOW_LENGTH;
  sysInfoPage[FLASH_LORA_RECEIVE_LEN] = LORA_RECEIVE_WINDOW_LENGTH;

  // set default low power mode configuration
  sysInfoPage[FLASH_LOW_POWER_MODE_ENABLED] = 1;

  // set default voltage limits
  int16_t voltageLimit = DEPLOYMENT_BATTERY_VOLTAGE_LIMIT;
  memcpy(sysInfoPagePtr + FLASH_DEPLOYMENT_BATTERY_VOLTAGE_LIMIT, &voltageLimit, sizeof(int16_t));
  voltageLimit = HEATER_BATTERY_VOLTAGE_LIMIT;
  memcpy(sysInfoPagePtr + FLASH_HEATER_BATTERY_VOLTAGE_LIMIT, &voltageLimit, sizeof(int16_t));
  voltageLimit = BATTERY_CW_BEEP_VOLTAGE_LIMIT;
  memcpy(sysInfoPagePtr + FLASH_BATTERY_CW_BEEP_VOLTAGE_LIMIT, &voltageLimit, sizeof(int16_t));
  voltageLimit = LOW_POWER_MODE_VOLTAGE_LIMIT;
  memcpy(sysInfoPagePtr + FLASH_LOW_POWER_MODE_VOLTAGE_LIMIT, &voltageLimit, sizeof(int16_t));

  // set default temperature limits
  float tempLimit = BATTERY_HEATER_TEMP_LIMIT;
  memcpy(sysInfoPagePtr + FLASH_BATTERY_HEATER_TEMP_LIMIT, &tempLimit, sizeof(float));
  tempLimit = MPPT_TEMP_LIMIT;
  memcpy(sysInfoPagePtr + FLASH_MPPT_TEMP_LIMIT, &tempLimit, sizeof(float));

  // set default heater duty cycle
  uint8_t dutyCycle = BATTERY_HEATER_DUTY_CYCLE;
  memcpy(sysInfoPagePtr + FLASH_BATTERY_HEATER_DUTY_CYCLE, &dutyCycle, sizeof(uint8_t));

  // set default MPPT temperature switch mode
  sysInfoPage[FLASH_MPPT_TEMP_SWITCH_ENABLED] = 1;

  // write the default system info
  PersistentStorage_Write(FLASH_SYSTEM_INFO_START, sysInfoPage, FLASH_SYSTEM_INFO_LEN);
}

uint8_t PersistentStorage_Get_Message(uint16_t slotNum, uint8_t* buff) {
  // read the message slot
  uint8_t messageBuff[MAX_STRING_LENGTH];
  PersistentStorage_Read(FLASH_STORE_AND_FORWARD_START + slotNum*MAX_STRING_LENGTH, messageBuff, MAX_STRING_LENGTH);

  // get message length
  uint8_t messageLen = messageBuff[sizeof(uint32_t)];

  // copy the message without length and ID
  if(messageLen < MAX_STRING_LENGTH) {
    memcpy(buff, messageBuff + sizeof(uint32_t) + sizeof(uint8_t), messageLen);
  }

  return(messageLen);
}

void PersistentStorage_Set_Message(uint16_t slotNum, uint8_t* buff, uint8_t len) {
  // read the current sector
  uint8_t sectorBuff[FLASH_SECTOR_SIZE];
  uint32_t addr = FLASH_STORE_AND_FORWARD_START + slotNum*MAX_STRING_LENGTH;
  PersistentStorage_Read(addr & 0xFFFFF000, sectorBuff, FLASH_SECTOR_SIZE);

  // update buffer
  memcpy(sectorBuff + ((slotNum*MAX_STRING_LENGTH) & 0x00000FFF), buff, len);

  // write updated buffer
  PersistentStorage_Write(addr & 0xFFFFF000, sectorBuff, FLASH_SECTOR_SIZE);
  FOSSASAT_DEBUG_PRINT_FLASH(addr, FLASH_EXT_PAGE_SIZE);
}

void PersistentStorage_Read(uint32_t addr, uint8_t* buff, size_t len) {
  uint8_t cmdBuff[] = {MX25L51245G_CMD_READ, (uint8_t)((addr >> 24) & 0xFF), (uint8_t)((addr >> 16) & 0xFF), (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF)};
  PersistentStorage_SPItranscation(cmdBuff, 5, false, buff, len);
}

void PersistentStorage_Write(uint32_t addr, uint8_t* buff, size_t len, bool autoErase) {
  // erase requested sector
  if(autoErase) {
    PersistentStorage_SectorErase(addr);
  }

  // set WEL bit again
  PersistentStorage_WaitForWriteEnable();

  // check if all bytes are in the same page
  uint32_t addrInPage = (addr & 0xFF) + len;
  if(addrInPage <= FLASH_EXT_PAGE_SIZE) {
    // all bytes are in the same page, write it
    uint8_t cmdBuff[] = {MX25L51245G_CMD_PP, (uint8_t)((addr >> 24) & 0xFF), (uint8_t)((addr >> 16) & 0xFF), (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF)};
    PersistentStorage_SPItranscation(cmdBuff, 5, true, buff, len);
  } else {
    // some bytes are in the following page
    // TODO: extend for arbitrary number of pages in the same sector

    // get the number of bytes in the first page
    size_t firstPageLen = FLASH_EXT_PAGE_SIZE - (addr & 0xFF);

    // write the first page
    uint32_t newAddr = addr;
    uint8_t cmdBuff[] = {MX25L51245G_CMD_PP, (uint8_t)((newAddr >> 24) & 0xFF), (uint8_t)((newAddr >> 16) & 0xFF), (uint8_t)((newAddr >> 8) & 0xFF), (uint8_t)(newAddr & 0xFF)};
    PersistentStorage_SPItranscation(cmdBuff, 5, true, buff, firstPageLen);

    // wait until page is written
    PersistentStorage_WaitForWriteInProgress();

    // set WEL bit again
    PersistentStorage_WaitForWriteEnable();

    // write the remainder
    newAddr = (addr & 0xFFFFFF00) + FLASH_EXT_PAGE_SIZE;
    cmdBuff[1] = (uint8_t)((newAddr >> 24) & 0xFF);
    cmdBuff[2] = (uint8_t)((newAddr >> 16) & 0xFF);
    cmdBuff[3] = (uint8_t)((newAddr >> 8) & 0xFF);
    cmdBuff[4] = (uint8_t)(newAddr & 0xFF);
    PersistentStorage_SPItranscation(cmdBuff, 5, true, buff + firstPageLen, len - firstPageLen);
  }
  
  // wait until page is written
  PersistentStorage_WaitForWriteInProgress();
}

void PersistentStorage_SectorErase(uint32_t addr) {
  // set WEL bit
  PersistentStorage_WaitForWriteEnable();

  // erase required sector
  uint8_t cmdBuf[] = {MX25L51245G_CMD_SE, (uint8_t)((addr >> 24) & 0xFF), (uint8_t)((addr >> 16) & 0xFF), (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF)};
  PersistentStorage_SPItranscation(cmdBuf, 5, false, NULL, 0);

  // wait until sector is erased
  PersistentStorage_WaitForWriteInProgress(1000);
}

void PersistentStorage_64kBlockErase(uint32_t addr) {
  // set WEL bit
  PersistentStorage_WaitForWriteEnable();

  // erase required sector
  uint8_t cmdBuf[] = {MX25L51245G_CMD_BE, (uint8_t)((addr >> 24) & 0xFF), (uint8_t)((addr >> 16) & 0xFF), (uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF)};
  PersistentStorage_SPItranscation(cmdBuf, 5, false, NULL, 0);

  // wait until sector is erased
  PersistentStorage_WaitForWriteInProgress(3000);
}

void PersistentStorage_WriteEnable() {
  PersistentStorage_SPItranscation(MX25L51245G_CMD_WREN);
}

void PersistentStorage_WriteDisable() {
  PersistentStorage_SPItranscation(MX25L51245G_CMD_WRDI);
}

uint8_t PersistentStorage_ReadManufacturerID() {
  uint8_t cmdBuf[] = {MX25L51245G_CMD_REMS, 0x00, 0x00, 0x00};
  uint8_t buf[2];
  PersistentStorage_SPItranscation(cmdBuf, 4, false, buf, 2);
  return(buf[0]);
}

uint8_t PersistentStorage_ReadStatusRegister() {
  uint8_t buf[1];
  PersistentStorage_SPItranscation(MX25L51245G_CMD_RDSR, false, buf, 1);
  return(buf[0]);
}

uint8_t PersistentStorage_ReadConfigRegister() {
  uint8_t buf[1];
  PersistentStorage_SPItranscation(MX25L51245G_CMD_RDCR, false, buf, 1);
  return(buf[0]);
}

uint8_t PersistentStorage_ReadSecurityRegister() {
  uint8_t buf[1];
  PersistentStorage_SPItranscation(MX25L51245G_CMD_RDSCUR, false, buf, 1);
  return(buf[0]);
}

void PersistentStorage_Enter4ByteMode() {
  PersistentStorage_SPItranscation(MX25L51245G_CMD_EN4B);
}

void PersistentStorage_Exit4ByteMode() {
  PersistentStorage_SPItranscation(MX25L51245G_CMD_EX4B);
}

void PersistentStorage_Reset() {
  pinMode(FLASH_RESET, OUTPUT);
  digitalWrite(FLASH_RESET, LOW);
  delayMicroseconds(100);
  pinMode(FLASH_RESET, INPUT);
}

void PersistentStorage_WriteStatusRegister(uint8_t sr, uint8_t cr) {
  uint8_t buf[] = {sr, cr};
  PersistentStorage_WaitForWriteEnable();
  PersistentStorage_SPItranscation(MX25L51245G_CMD_WRSR, true, buf, 2);
  PersistentStorage_WriteDisable();
}

bool PersistentStorage_WaitForWriteEnable(uint32_t timeout) {
  // start the timer
  uint32_t start = millis();

  // repeat until WEL bit is set
  while(!(PersistentStorage_ReadStatusRegister() & MX25L51245G_SR_WEL)) {
    PersistentStorage_WriteEnable();
    delayMicroseconds(10);

    // check timeout
    if(millis() - start >= timeout) {
      return(false);
    }
  }
  return(true);
}

bool PersistentStorage_WaitForWriteInProgress(uint32_t timeout) {
  // start the timer
  uint32_t start = millis();

  // repeat as long as WIP bit is set
  while(PersistentStorage_ReadStatusRegister() & MX25L51245G_SR_WIP) {
    delayMicroseconds(10);

    // check timeout
    if(millis() - start >= timeout) {
      return(false);
    }
  }
  return(true);
}

void PersistentStorage_SPItranscation(uint8_t cmd, bool write, uint8_t* data, size_t numBytes) {
  uint8_t cmdBuf[] = {cmd};
  PersistentStorage_SPItranscation(cmdBuf, 1, write, data, numBytes);
}

void PersistentStorage_SPItranscation(uint8_t* cmd, uint8_t cmdLen, bool write, uint8_t* data, size_t numBytes) {
  digitalWrite(FLASH_CS, LOW);
  FlashSPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));

  // send command
  for(uint8_t n = 0; n < cmdLen; n++) {
    // send byte
    FlashSPI.transfer(cmd[n]);
  }

  // send data
  if(write) {
    for(size_t n = 0; n < numBytes; n++) {
      // send byte
      FlashSPI.transfer(data[n]);
    }

  } else {
    for(size_t n = 0; n < numBytes; n++) {
      data[n] = FlashSPI.transfer(MX25L51245G_CMD_NOP);
    }
  }

  FlashSPI.endTransaction();
  digitalWrite(FLASH_CS, HIGH);
}
