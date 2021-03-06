#include "PowerControl.h"

uint32_t PowerControl_Get_Sleep_Interval() {
  // sleep interval in ms
  uint32_t interval = 0;

  #ifdef ENABLE_INTERVAL_CONTROL
    // get battery voltage
    float batt = PowerControl_Get_Battery_Voltage();

    if(batt > 4.05f) {
      interval = (uint32_t)20 * (uint32_t)1000;
    } else if(batt > 4.0f) {
      interval = (uint32_t)35 * (uint32_t)1000;
    } else if(batt > 3.9f) {
      interval = (uint32_t)100 * (uint32_t)1000;
    } else if(batt > 3.8f) {
      interval = (uint32_t)160 * (uint32_t)1000;
    } else if(batt > 3.7f) {
      interval = (uint32_t)180 * (uint32_t)1000;
    } else {
      interval = (uint32_t)240 * (uint32_t)1000;
    }
  #endif

  return(interval);
}

void PowerControl_Wait(uint32_t ms, uint8_t type, bool radioSleep) {
  if (ms == 0) {
    return;
  }

  // calculate number of required loops (rounded up)
  float stepSize = 500.0;
  if (type == LOW_POWER_NONE) {
    // 10 ms steps when no sleep mode is active
    stepSize = 10.0;
  }
  float numLoops = 0.5 + (float)ms / stepSize;

  // set radio to sleep
  if(radioSleep) {
    radio.sleep();
  }

  // perform all loops
  for (uint32_t i = 0; i < (uint32_t)numLoops; i++) {
    PowerControl_Watchdog_Heartbeat();
    switch(type) {
      case LOW_POWER_NONE:
        delay((uint32_t)stepSize);
        break;
      case LOW_POWER_IDLE:
        LowPower.idle((uint32_t)stepSize);
        break;
      case LOW_POWER_SLEEP:
        LowPower.sleep((uint32_t)stepSize);
        break;
      case LOW_POWER_DEEP_SLEEP:
        LowPower.deepSleep((uint32_t)stepSize);
        break;
      case LOW_POWER_SHUTDOWN:
        LowPower.shutdown((uint32_t)stepSize);
        break;
      default:
        return;
    }
  }

  // wake up radio
  if(radioSleep) {
    radio.standby();
  }
}

void PowerControl_Watchdog_Heartbeat(bool manageBattery) {
  // toggle watchdog pin
  digitalWrite(WATCHDOG_IN, !digitalRead(WATCHDOG_IN));

  // manage battery (low power, heater, charging)
  if(manageBattery) {
    PowerControl_Manage_Battery();
  }
}

void PowerControl_Watchdog_Restart() {
  // do not pet watchdog for more than 15 seconds to restart
  FOSSASAT_DEBUG_PRINTLN(F("Restart in 15 seconds ..."));
  FOSSASAT_DEBUG_DELAY(10);
  LowPower.deepSleep(16000);
}

void PowerControl_Deploy() {
  FOSSASAT_DEBUG_PRINTLN(F("Deploy"));
  FOSSASAT_DEBUG_DELAY(10);

  // enable MOSFETs one at a time
  digitalWrite(DEPLOYMENT_FET_1, HIGH);
  PowerControl_Wait(1000, LOW_POWER_SLEEP);
  digitalWrite(DEPLOYMENT_FET_1, LOW);
  
  digitalWrite(DEPLOYMENT_FET_2, HIGH);
  PowerControl_Wait(1000, LOW_POWER_SLEEP);
  digitalWrite(DEPLOYMENT_FET_2, LOW);
}

float PowerControl_Get_Battery_Voltage() {
  return(currSensorMPPT.readBusVoltage());
}

void PowerControl_Manage_Battery() {
  // check battery voltage
  if((PowerControl_Get_Battery_Voltage() <= PersistentStorage_Get<uint16_t>(FLASH_LOW_POWER_MODE_VOLTAGE_LIMIT)) && (PersistentStorage_Get<uint8_t>(FLASH_LOW_POWER_MODE_ENABLED) == 1)) {
    // activate low power mode
    PersistentStorage_Set<uint8_t>(FLASH_LOW_POWER_MODE, LOW_POWER_SLEEP);
  } else {
    // deactivate low power mode
    PersistentStorage_Set<uint8_t>(FLASH_LOW_POWER_MODE, LOW_POWER_NONE);
  }

  // check temperature limit to enable/disable charging
  float mpptTempLimit = PersistentStorage_Get<float>(FLASH_MPPT_TEMP_LIMIT);
  uint8_t mpptKeepAlive = PersistentStorage_Get<uint8_t>(FLASH_MPPT_KEEP_ALIVE_ENABLED);
  uint8_t mpptTempSwitch = PersistentStorage_Get<uint8_t>(FLASH_MPPT_TEMP_SWITCH_ENABLED);
  if(mpptKeepAlive == 1) {
    // MPPT keep alive is enabled, force charging regardless of everything else
    digitalWrite(MPPT_OFF, LOW);
  } else if((mpptTempSwitch == 1) && ((Sensors_Read_Temperature(tempSensorBattery) <= mpptTempLimit) || (Sensors_Read_Temperature(tempSensorSecBattery) <= mpptTempLimit))) {
    // at least one battery has temperature below limit, disable charging
    digitalWrite(MPPT_OFF, HIGH);
  } else {
    // temperature above limit, enable charging
    digitalWrite(MPPT_OFF, LOW);
  }

  // check temperature and voltage limit to enable heater
  float heaterTempLimit = PersistentStorage_Get<float>(FLASH_BATTERY_HEATER_TEMP_LIMIT);
  if((Sensors_Read_Temperature(tempSensorBattery) <= heaterTempLimit) && 
     (Sensors_Read_Temperature(tempSensorSecBattery) <= heaterTempLimit) && 
     (PowerControl_Get_Battery_Voltage() >= PersistentStorage_Get<int16_t>(FLASH_HEATER_BATTERY_VOLTAGE_LIMIT))) {
    // both temperatures are below limit and battery is above limit, enable heater
    analogWrite(BATTERY_HEATER_FET, PersistentStorage_Get<uint8_t>(BATTERY_HEATER_DUTY_CYCLE));
  } else {
    // temperature is too high or battery is too low, disable heater
    digitalWrite(BATTERY_HEATER_FET, LOW);
  }
}
