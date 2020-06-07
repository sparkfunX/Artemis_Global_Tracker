// Artemis Global Tracker: battery voltage definitions and functions

// We check if the battery voltage is low in several places. This function simplifies that and
// lets us define a single "low voltage" to cause the tracker to go to sleep: 20 * 10mV less than LOWBATT
bool battVlow ()
{
  if (myTrackerSettings.BATTV.the_data < (myTrackerSettings.LOWBATT.the_data - 20))
    return (true);
  else
    return (false);
}

// Get the battery (bus) voltage
// Enable the bus voltage monitor
// Read the bus voltage and store it in vbat
// Disable the bus voltage monitor to save power
// Converts the analogread into V * 10^-2, compensating for
// the voltage divider (/3) and the Apollo voltage reference (2.0V)
// Include a correction factor of 1.09 to correct for the divider impedance
void get_vbat()
{
  digitalWrite(busVoltageMonEN, HIGH); // Enable the bus voltage monitor
  analogReadResolution(14); //Set resolution to 14 bit
  delay(10); // Let the voltage settle
  myTrackerSettings.BATTV.the_data = ((uint16_t)analogRead(busVoltagePin)) * 109 * 3 * 2 / 16384; // Convert into V * 10^-2
  digitalWrite(busVoltageMonEN, LOW); // Disable the bus voltage monitor
}
