// Artemis Global Tracker: IridiumSBD Callbacks

// IridiumSBD Callback - this code is called while the 9603N is trying to transmit
bool ISBDCallback()
{
  // Flash the LED at 4 Hz
  if ((millis() / 250) % 2 == 1) {
    digitalWrite(LED, HIGH);
  }
  else {
    digitalWrite(LED, LOW);
  }

  // Check the battery voltage now we are drawing current for the 9603
  // If voltage is low, stop Iridium send
  get_vbat(); // Read the battery (bus) voltage
  if ((myTrackerSettings.BATTV.the_data < (myTrackerSettings.LOWBATT.the_data - 200))) {
    Serial.print("*** LOW VOLTAGE (ISBDCallback) ");
    Serial.print((((float)myTrackerSettings.BATTV.the_data)/100.0),2);
    Serial.println("V ***");
    return false; // Returning false causes IridiumSBD to terminate
  }
  else {     
    return true;
  }
} // End of ISBDCallback()

#if DIAGNOSTICS
void ISBDConsoleCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}

void ISBDDiagsCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}
#endif
