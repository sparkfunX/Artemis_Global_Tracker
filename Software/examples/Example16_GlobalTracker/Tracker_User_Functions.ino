// Artemis Global Tracker: User Functions

// Add your own code to these functions to return USERVAL's or execute USERFUNC's

void USER_FUNC_1()
{
  debugPrintln(F("Executing USERFUNC1..."));
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC1" (0x58) message field
}

void USER_FUNC_2()
{
  debugPrintln(F("Executing USERFUNC2..."));
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC2" (0x59) message field
}

void USER_FUNC_3()
{
  debugPrintln(F("Executing USERFUNC3..."));
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC3" (0x5a) message field
}

void USER_FUNC_4()
{
  debugPrintln(F("Executing USERFUNC4..."));
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC4" (0x5b) message field
}

void USER_FUNC_5(uint16_t myVar)
{
  debugPrintln(F("Executing USERFUNC5..."));
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC5" (0x5c) message field
}

void USER_FUNC_6(uint16_t myVar)
{
  debugPrintln(F("Executing USERFUNC6..."));
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC6" (0x5d) message field
}

void USER_FUNC_7(uint32_t myVar)
{
  debugPrintln(F("Executing USERFUNC7..."));
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC7" (0x5e) message field
}

void USER_FUNC_8(uint32_t myVar)
{
  debugPrintln(F("Executing USERFUNC8..."));
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC8" (0x5f) message field
}

byte USER_VAL_1()
{
  // Add your own code here - e.g. read an external sensor. The return value will be sent as a "USERVAL1" (0x20) message field
  byte retVal; // The return value.
  retVal = 1; // Set retVal to 1 for testing - delete this line if you add your own code
  return (retVal);
}

byte USER_VAL_2()
{
  // Add your own code here - e.g. read an external sensor. The return value will be sent as a "USERVAL2" (0x21) message field
  byte retVal; // The return value.
  retVal = 2; // Set retVal to 2 for testing - delete this line if you add your own code
  return (retVal);
}

uint16_t USER_VAL_3()
{
  // Add your own code here - e.g. read an external sensor. The return value will be sent as a "USERVAL3" (0x22) message field
  uint16_t retVal; // The return value.
  retVal = 3; // Set retVal to 3 for testing - delete this line if you add your own code
  return (retVal);
}
uint16_t USER_VAL_4()
{
  // Add your own code here - e.g. read an external sensor. The return value will be sent as a "USERVAL4" (0x23) message field
  uint16_t retVal; // The return value.
  retVal = 4; // Set retVal to 4 for testing - delete this line if you add your own code
  return (retVal);
}

uint32_t USER_VAL_5()
{
  // Add your own code here - e.g. read an external sensor. The return value will be sent as a "USERVAL5" (0x24) message field
  uint32_t retVal; // The return value.
  retVal = 5; // Set retVal to 5 for testing - delete this line if you add your own code
  return (retVal);
}

uint32_t USER_VAL_6()
{
  // Add your own code here - e.g. read an external sensor. The return value will be sent as a "USERVAL6" (0x25) message field
  uint32_t retVal; // The return value.
  retVal = 6; // Set retVal to 6 for testing - delete this line if you add your own code
  return (retVal);
}

float USER_VAL_7()
{
  // Add your own code here - e.g. read an external sensor. The return value will be sent as a "USERVAL7" (0x26) message field
  float retVal; // The return value.
  retVal = 7.0; // Set retVal to 7.0 for testing - delete this line if you add your own code
  return (retVal);
}

float USER_VAL_8()
{
  // Add your own code here - e.g. read an external sensor. The return value will be sent as a "USERVAL8" (0x27) message field
  float retVal; // The return value.
  retVal = 8.0E8; // Set retVal to 8.0E8 for testing - delete this line if you add your own code
  return (retVal);
}

void ALARM_FUNC(uint8_t alarmType)
{
  debugPrintln(F("Executing ALARM_FUNC ..."));
  // Add your own code to be executed when an alarm is set off.
}
