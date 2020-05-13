"""
This tool allows you to generate configuration messages for the Artemis Global Tracker
which you can then upload via Serial (USB) or send via the Rock7 Operations
Send-A-Message feature.

Based on AFU.py (Artemis Firmware Updater) which is based on gist by Stefan Lehmann:
https://gist.github.com/stlehmann/bea49796ad47b1e7f658ddde9620dff1

Also based on Srikanth Anantharam's SerialTerminal example
https://github.com/sria91/SerialTerminal

MIT license

"""
from typing import Iterator, Tuple

from PyQt5.QtCore import QSettings, QProcess, QTimer, Qt, QIODevice, pyqtSlot
from PyQt5.QtWidgets import QWidget, QLabel, QComboBox, QGridLayout, QPushButton, \
    QApplication, QLineEdit, QFileDialog, QPlainTextEdit, QCheckBox, QMessageBox
from PyQt5.QtGui import QCloseEvent, QTextCursor
from PyQt5.QtSerialPort import QSerialPort, QSerialPortInfo

import struct, pickle
from time import sleep

# Setting constants
SETTING_PORT_NAME = 'COM1'
SETTING_FILE_LOCATION = 'C:'

guiVersion = 'V1.0'

def gen_serial_ports() -> Iterator[Tuple[str, str, str]]:
    """Return all available serial ports."""
    ports = QSerialPortInfo.availablePorts()
    return ((p.description(), p.portName(), p.systemLocation()) for p in ports)

# noinspection PyArgumentList

class MainWidget(QWidget):
    """Main Widget."""

    def __init__(self, parent: QWidget = None) -> None:
        super().__init__(parent)
 
        # File location line edit
        self.msg_label = QLabel(self.tr('Configuration File (.pkl):'))
        self.fileLocation_lineedit = QLineEdit()
        self.msg_label.setBuddy(self.msg_label)
        #self.fileLocation_lineedit.setEnabled(False)
        self.fileLocation_lineedit.returnPressed.connect(
            self.on_browse_btn_pressed)

        # Browse for new file button
        self.browse_btn = QPushButton(self.tr('Browse'))
        self.browse_btn.setEnabled(True)
        self.browse_btn.pressed.connect(self.on_browse_btn_pressed)

        # Load Config Button
        self.load_config_btn = QPushButton(self.tr('Load Config'))
        self.load_config_btn.pressed.connect(self.on_load_config_btn_pressed)

        # Save Config Button
        self.save_config_btn = QPushButton(self.tr('Save Config'))
        self.save_config_btn.pressed.connect(self.on_save_config_btn_pressed)

        # Calc Config Button
        self.calc_config_btn = QPushButton(self.tr('Calculate Config'))
        self.calc_config_btn.pressed.connect(self.on_calc_config_btn_pressed)

        # Port Combobox
        self.port_label = QLabel(self.tr('COM Port:'))
        self.port_combobox = QComboBox()
        self.port_label.setBuddy(self.port_combobox)
        self.update_com_ports()

        # Refresh Button
        self.refresh_btn = QPushButton(self.tr('Refresh'))
        self.refresh_btn.pressed.connect(self.on_refresh_btn_pressed)

        # Open Port Button
        self.open_port_btn = QPushButton(self.tr('Open Port'))
        self.open_port_btn.pressed.connect(self.on_open_port_btn_pressed)

        # Close Port Button
        self.close_port_btn = QPushButton(self.tr('Close Port'))
        self.close_port_btn.pressed.connect(self.on_close_port_btn_pressed)

        # Upload Button
        self.upload_btn = QPushButton(self.tr('Upload Config'))
        self.upload_btn.pressed.connect(self.on_upload_btn_pressed)

        # Terminal Bar
        self.terminal_label = QLabel(self.tr('Serial Monitor:'))

        # Terminal Window
        self.terminal = QPlainTextEdit()

        # Messages Bar
        self.messages_label = QLabel(self.tr('Warnings / Errors:'))

        # Messages Window
        self.messages = QPlainTextEdit()

        # Config Bar
        self.config_label = QLabel(self.tr('Configuration Message:'))

        # Config Window
        self.config = QPlainTextEdit()

        # FLAGS1 Labels
        FLAGS1_header = QLabel(self.tr('FLAGS1:'))
        FLAGS1_header.setAlignment(Qt.AlignHCenter | Qt.AlignVCenter)
        label_BINARY = QLabel(self.tr('Send message in binary format'))
        label_DEST = QLabel(self.tr('Forward messages to DEST'))
        label_HIPRESS = QLabel(self.tr('Enable HIPRESS alarm messages'))
        label_LOPRESS = QLabel(self.tr('Enable LOPRESS alarm messages'))
        label_HITEMP = QLabel(self.tr('Enable HITEMP alarm messages'))
        label_LOTEMP = QLabel(self.tr('Enable LOTEMP alarm messages'))
        label_HIHUMID = QLabel(self.tr('Enable HIHUMID alarm messages'))
        label_LOHUMID = QLabel(self.tr('Enable LOHUMID alarm messages'))

        label_BINARY.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_DEST.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_HIPRESS.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_LOPRESS.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_HITEMP.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_LOTEMP.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_HIHUMID.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_LOHUMID.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        
        # FLAGS1 CheckBoxes
        self.checkbox_F1_BINARY = QCheckBox()
        self.checkbox_F1_DEST = QCheckBox()
        self.checkbox_F1_HIPRESS = QCheckBox()
        self.checkbox_F1_LOPRESS = QCheckBox()
        self.checkbox_F1_HITEMP = QCheckBox()
        self.checkbox_F1_LOTEMP = QCheckBox()
        self.checkbox_F1_HIHUMID = QCheckBox()
        self.checkbox_F1_LOHUMID = QCheckBox()

        # FLAGS2 Labels
        FLAGS2_header = QLabel(self.tr('FLAGS2:'))
        FLAGS2_header.setAlignment(Qt.AlignHCenter | Qt.AlignVCenter)
        label_GEOFENCE = QLabel(self.tr('Enable GeoFence alarm messages'))
        label_INSIDE = QLabel(self.tr('Alarm when inside the GeoFence'))
        label_LOWBATT = QLabel(self.tr('Enable LOWBATT alarm messages'))
        label_RING = QLabel(self.tr('Monitor Ring Channel continuously'))

        label_GEOFENCE.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_INSIDE.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_LOWBATT.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_RING.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

        # FLAGS2 CheckBoxes
        self.checkbox_F2_GEOFENCE = QCheckBox()
        self.checkbox_F2_INSIDE = QCheckBox()
        self.checkbox_F2_LOWBATT = QCheckBox()
        self.checkbox_F2_RING = QCheckBox()

        # USERFUNC Labels
        USERFUNC_header = QLabel(self.tr('USERFUNCs:'))
        USERFUNC_header.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        label_USERFUNC1 = QLabel(self.tr('Execute USERFUNC1'))
        label_USERFUNC2 = QLabel(self.tr('Execute USERFUNC2'))
        label_USERFUNC3 = QLabel(self.tr('Execute USERFUNC3'))
        label_USERFUNC4 = QLabel(self.tr('Execute USERFUNC4'))
        label_USERFUNC5 = QLabel(self.tr('Execute USERFUNC5'))
        label_USERFUNC6 = QLabel(self.tr('Execute USERFUNC6'))
        label_USERFUNC7 = QLabel(self.tr('Execute USERFUNC7'))
        label_USERFUNC8 = QLabel(self.tr('Execute USERFUNC8'))

        # USERFUNC CheckBoxes
        self.checkbox_USERFUNC1 = QCheckBox()
        self.checkbox_USERFUNC2 = QCheckBox()
        self.checkbox_USERFUNC3 = QCheckBox()
        self.checkbox_USERFUNC4 = QCheckBox()
        self.checkbox_USERFUNC5 = QCheckBox()
        self.checkbox_USERFUNC6 = QCheckBox()
        self.checkbox_USERFUNC7 = QCheckBox()
        self.checkbox_USERFUNC8 = QCheckBox()

        # USERFUNC5-8 Value
        self.USERFUNC5_val = QLineEdit()
        self.USERFUNC6_val = QLineEdit()
        self.USERFUNC7_val = QLineEdit()
        self.USERFUNC8_val = QLineEdit()

        # MOFIELDS Labels

        MOFIELDS_header = QLabel(self.tr('MOFIELDS:'))
        SWVER_label = QLabel(self.tr('SWVER'))
        SOURCE_label = QLabel(self.tr('SOURCE'))
        BATTV_label = QLabel(self.tr('BATTV'))
        PRESS_label = QLabel(self.tr('PRESS'))
        TEMP_label = QLabel(self.tr('TEMP'))
        HUMID_label = QLabel(self.tr('HUMID'))
        YEAR_label = QLabel(self.tr('YEAR'))
        MONTH_label = QLabel(self.tr('MONTH'))
        DAY_label = QLabel(self.tr('DAY'))
        HOUR_label = QLabel(self.tr('HOUR'))
        MIN_label = QLabel(self.tr('MIN'))
        SEC_label = QLabel(self.tr('SEC'))
        MILLIS_label = QLabel(self.tr('MILLIS'))
        DATETIME_label = QLabel(self.tr('DATETIME'))
        LAT_label = QLabel(self.tr('LAT'))
        LON_label = QLabel(self.tr('LON'))
        ALT_label = QLabel(self.tr('ALT'))
        SPEED_label = QLabel(self.tr('SPEED'))
        HEAD_label = QLabel(self.tr('HEAD'))
        SATS_label = QLabel(self.tr('SATS'))

        MOFIELDS_header.setAlignment(Qt.AlignHCenter | Qt.AlignVCenter)
        SWVER_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        SOURCE_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        BATTV_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        PRESS_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        TEMP_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        HUMID_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        YEAR_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        MONTH_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        DAY_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        HOUR_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        MIN_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        SEC_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        MILLIS_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        DATETIME_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        LAT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        LON_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        ALT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        SPEED_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        HEAD_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        SATS_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

        PDOP_label = QLabel(self.tr('PDOP'))
        FIX_label = QLabel(self.tr('FIX'))
        GEOFSTAT_label = QLabel(self.tr('GEOFSTAT'))
        USERVAL1_label = QLabel(self.tr('USERVAL1'))
        USERVAL2_label = QLabel(self.tr('USERVAL2'))
        USERVAL3_label = QLabel(self.tr('USERVAL3'))
        USERVAL4_label = QLabel(self.tr('USERVAL4'))
        USERVAL5_label = QLabel(self.tr('USERVAL5'))
        USERVAL6_label = QLabel(self.tr('USERVAL6'))
        USERVAL7_label = QLabel(self.tr('USERVAL7'))
        USERVAL8_label = QLabel(self.tr('USERVAL8'))
        MOFIELDS_label = QLabel(self.tr('MOFIELDS'))
        FLAGS1_label = QLabel(self.tr('FLAGS1'))
        FLAGS2_label = QLabel(self.tr('FLAGS2'))
        DEST_label = QLabel(self.tr('DEST'))
        HIPRESS_label = QLabel(self.tr('HIPRESS'))
        LOPRESS_label = QLabel(self.tr('LOPRESS'))
        HITEMP_label = QLabel(self.tr('HITEMP'))
        LOTEMP_label = QLabel(self.tr('LOTEMP'))
        HIHUMID_label = QLabel(self.tr('HIHUMID'))

        PDOP_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        FIX_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOFSTAT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        USERVAL1_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        USERVAL2_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        USERVAL3_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        USERVAL4_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        USERVAL5_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        USERVAL6_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        USERVAL7_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        USERVAL8_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        MOFIELDS_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        FLAGS1_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        FLAGS2_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        DEST_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        HIPRESS_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        LOPRESS_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        HITEMP_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        LOTEMP_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        HIHUMID_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

        LOHUMID_label = QLabel(self.tr('LOHUMID'))
        GEOFNUM_label = QLabel(self.tr('GEOFNUM'))
        GEOF1LAT_label = QLabel(self.tr('GEOF1LAT'))
        GEOF1LON_label = QLabel(self.tr('GEOF1LON'))
        GEOF1RAD_label = QLabel(self.tr('GEOF1RAD'))
        GEOF2LAT_label = QLabel(self.tr('GEOF2LAT'))
        GEOF2LON_label = QLabel(self.tr('GEOF2LON'))
        GEOF2RAD_label = QLabel(self.tr('GEOF2RAD'))
        GEOF3LAT_label = QLabel(self.tr('GEOF3LAT'))
        GEOF3LON_label = QLabel(self.tr('GEOF3LON'))
        GEOF3RAD_label = QLabel(self.tr('GEOF3RAD'))
        GEOF4LAT_label = QLabel(self.tr('GEOF4LAT'))
        GEOF4LON_label = QLabel(self.tr('GEOF4LON'))
        GEOF4RAD_label = QLabel(self.tr('GEOF4RAD'))
        WAKEINT_label = QLabel(self.tr('WAKEINT'))
        ALARMINT_label = QLabel(self.tr('ALARMINT'))
        TXINT_label = QLabel(self.tr('TXINT'))
        LOWBATT_label = QLabel(self.tr('LOWBATT'))
        DYNMODEL_label = QLabel(self.tr('DYNMODEL'))

        LOHUMID_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOFNUM_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF1LAT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF1LON_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF1RAD_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF2LAT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF2LON_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF2RAD_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF3LAT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF3LON_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF3RAD_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF4LAT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF4LON_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF4RAD_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        WAKEINT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        ALARMINT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        TXINT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        LOWBATT_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        DYNMODEL_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        
        # MOFIELDS CheckBoxes

        self.checkbox_SWVER = QCheckBox()
        self.checkbox_SOURCE = QCheckBox()
        self.checkbox_BATTV = QCheckBox()
        self.checkbox_PRESS = QCheckBox()
        self.checkbox_TEMP = QCheckBox()
        self.checkbox_HUMID = QCheckBox()
        self.checkbox_YEAR = QCheckBox()
        self.checkbox_MONTH = QCheckBox()
        self.checkbox_DAY = QCheckBox()
        self.checkbox_HOUR = QCheckBox()
        self.checkbox_MIN = QCheckBox()
        self.checkbox_SEC = QCheckBox()
        self.checkbox_MILLIS = QCheckBox()
        self.checkbox_DATETIME = QCheckBox()
        self.checkbox_LAT = QCheckBox()
        self.checkbox_LON = QCheckBox()
        self.checkbox_ALT = QCheckBox()
        self.checkbox_SPEED = QCheckBox()
        self.checkbox_HEAD = QCheckBox()
        self.checkbox_SATS = QCheckBox()
       
        self.checkbox_PDOP = QCheckBox()
        self.checkbox_FIX = QCheckBox()
        self.checkbox_GEOFSTAT = QCheckBox()
        self.checkbox_USERVAL1 = QCheckBox()
        self.checkbox_USERVAL2 = QCheckBox()
        self.checkbox_USERVAL3 = QCheckBox()
        self.checkbox_USERVAL4 = QCheckBox()
        self.checkbox_USERVAL5 = QCheckBox()
        self.checkbox_USERVAL6 = QCheckBox()
        self.checkbox_USERVAL7 = QCheckBox()
        self.checkbox_USERVAL8 = QCheckBox()
        self.checkbox_MOFIELDS = QCheckBox()
        self.checkbox_FLAGS1 = QCheckBox()
        self.checkbox_FLAGS2 = QCheckBox()
        self.checkbox_DEST = QCheckBox()
        self.checkbox_HIPRESS = QCheckBox()
        self.checkbox_LOPRESS = QCheckBox()
        self.checkbox_HITEMP = QCheckBox()
        self.checkbox_LOTEMP = QCheckBox()
        self.checkbox_HIHUMID = QCheckBox()
        
        self.checkbox_LOHUMID = QCheckBox()
        self.checkbox_GEOFNUM = QCheckBox()
        self.checkbox_GEOF1LAT = QCheckBox()
        self.checkbox_GEOF1LON = QCheckBox()
        self.checkbox_GEOF1RAD = QCheckBox()
        self.checkbox_GEOF2LAT = QCheckBox()
        self.checkbox_GEOF2LON = QCheckBox()
        self.checkbox_GEOF2RAD = QCheckBox()
        self.checkbox_GEOF3LAT = QCheckBox()
        self.checkbox_GEOF3LON = QCheckBox()
        self.checkbox_GEOF3RAD = QCheckBox()
        self.checkbox_GEOF4LAT = QCheckBox()
        self.checkbox_GEOF4LON = QCheckBox()
        self.checkbox_GEOF4RAD = QCheckBox()
        self.checkbox_WAKEINT = QCheckBox()
        self.checkbox_ALARMINT = QCheckBox()
        self.checkbox_TXINT = QCheckBox()
        self.checkbox_LOWBATT = QCheckBox()
        self.checkbox_DYNMODEL = QCheckBox()
        
        # Field Value Labels

        Values_label = QLabel(self.tr('Field Values:'))
        Include_label = QLabel(self.tr('Include'))
        FLAGS1_val_label = QLabel(self.tr('FLAGS1'))
        FLAGS2_val_label = QLabel(self.tr('FLAGS2'))
        MOFIELDS_val_label = QLabel(self.tr('MOFIELDS'))
        SOURCE_val_label = QLabel(self.tr('SOURCE'))
        DEST_val_label = QLabel(self.tr('DEST'))
        HIPRESS_val_label = QLabel(self.tr('HIPRESS'))
        LOPRESS_val_label = QLabel(self.tr('LOPRESS'))
        HITEMP_val_label = QLabel(self.tr('HITEMP'))
        LOTEMP_val_label = QLabel(self.tr('LOTEMP'))
        HIHUMID_val_label = QLabel(self.tr('HIHUMID'))
        LOHUMID_val_label = QLabel(self.tr('LOHUMID'))
        GEOFNUM_val_label = QLabel(self.tr('GEOFNUM'))
        GEOF1LAT_val_label = QLabel(self.tr('GEOF1LAT'))
        GEOF1LON_val_label = QLabel(self.tr('GEOF1LON'))
        GEOF1RAD_val_label = QLabel(self.tr('GEOF1RAD'))
        GEOF2LAT_val_label = QLabel(self.tr('GEOF2LAT'))
        GEOF2LON_val_label = QLabel(self.tr('GEOF2LON'))
        GEOF2RAD_val_label = QLabel(self.tr('GEOF2RAD'))
        GEOF3LAT_val_label = QLabel(self.tr('GEOF3LAT'))
        GEOF3LON_val_label = QLabel(self.tr('GEOF3LON'))
        GEOF3RAD_val_label = QLabel(self.tr('GEOF3RAD'))
        GEOF4LAT_val_label = QLabel(self.tr('GEOF4LAT'))
        GEOF4LON_val_label = QLabel(self.tr('GEOF4LON'))
        GEOF4RAD_val_label = QLabel(self.tr('GEOF4RAD'))
        WAKEINT_val_label = QLabel(self.tr('WAKEINT'))
        ALARMINT_val_label = QLabel(self.tr('ALARMINT'))
        TXINT_val_label = QLabel(self.tr('TXINT'))
        LOWBATT_val_label = QLabel(self.tr('LOWBATT'))
        DYNMODEL_val_label = QLabel(self.tr('DYNMODEL'))

        Values_label.setAlignment(Qt.AlignHCenter | Qt.AlignVCenter)
        Include_label.setAlignment(Qt.AlignHCenter | Qt.AlignVCenter)
        FLAGS1_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        FLAGS2_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        MOFIELDS_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        SOURCE_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        DEST_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        HIPRESS_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        LOPRESS_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        HITEMP_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        LOTEMP_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        HIHUMID_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        LOHUMID_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOFNUM_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF1LAT_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF1LON_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF1RAD_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF2LAT_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF2LON_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF2RAD_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF3LAT_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF3LON_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF3RAD_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF4LAT_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF4LON_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        GEOF4RAD_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        WAKEINT_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        ALARMINT_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        TXINT_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        LOWBATT_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        DYNMODEL_val_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

        # Field Value Checkboxes

        self.checkbox_val_FLAGS1 = QCheckBox()
        self.checkbox_val_FLAGS2 = QCheckBox()
        self.checkbox_val_MOFIELDS = QCheckBox()
        self.checkbox_val_SOURCE = QCheckBox()
        self.checkbox_val_DEST = QCheckBox()
        self.checkbox_val_HIPRESS = QCheckBox()
        self.checkbox_val_LOPRESS = QCheckBox()
        self.checkbox_val_HITEMP = QCheckBox()
        self.checkbox_val_LOTEMP = QCheckBox()
        self.checkbox_val_HIHUMID = QCheckBox()
        self.checkbox_val_LOHUMID = QCheckBox()      
        self.checkbox_val_GEOFNUM = QCheckBox()
        self.checkbox_val_GEOF1LAT = QCheckBox()
        self.checkbox_val_GEOF1LON = QCheckBox()
        self.checkbox_val_GEOF1RAD = QCheckBox()
        self.checkbox_val_GEOF2LAT = QCheckBox()
        self.checkbox_val_GEOF2LON = QCheckBox()
        self.checkbox_val_GEOF2RAD = QCheckBox()
        self.checkbox_val_GEOF3LAT = QCheckBox()
        self.checkbox_val_GEOF3LON = QCheckBox()
        self.checkbox_val_GEOF3RAD = QCheckBox()
        self.checkbox_val_GEOF4LAT = QCheckBox()
        self.checkbox_val_GEOF4LON = QCheckBox()
        self.checkbox_val_GEOF4RAD = QCheckBox()
        self.checkbox_val_WAKEINT = QCheckBox()
        self.checkbox_val_ALARMINT = QCheckBox()
        self.checkbox_val_TXINT = QCheckBox()
        self.checkbox_val_LOWBATT = QCheckBox()
        self.checkbox_val_DYNMODEL = QCheckBox()

        # Field Value Values

        self.val_SOURCE = QLineEdit()
        self.val_DEST = QLineEdit()
        self.val_HIPRESS = QLineEdit()
        self.val_LOPRESS = QLineEdit()
        self.val_HITEMP = QLineEdit()
        self.val_LOTEMP = QLineEdit()
        self.val_HIHUMID = QLineEdit()
        self.val_LOHUMID = QLineEdit()      
        self.val_GEOFNUM = QLineEdit()
        self.val_GEOF1LAT = QLineEdit()
        self.val_GEOF1LON = QLineEdit()
        self.val_GEOF1RAD = QLineEdit()
        self.val_GEOF2LAT = QLineEdit()
        self.val_GEOF2LON = QLineEdit()
        self.val_GEOF2RAD = QLineEdit()
        self.val_GEOF3LAT = QLineEdit()
        self.val_GEOF3LON = QLineEdit()
        self.val_GEOF3RAD = QLineEdit()
        self.val_GEOF4LAT = QLineEdit()
        self.val_GEOF4LON = QLineEdit()
        self.val_GEOF4RAD = QLineEdit()
        self.val_WAKEINT = QLineEdit()
        self.val_ALARMINT = QLineEdit()
        self.val_TXINT = QLineEdit()
        self.val_LOWBATT = QLineEdit()
        self.val_DYNMODEL = QLineEdit()

        # Field Units
        
        FLAGS1_units = QLabel(self.tr(''))
        FLAGS2_units = QLabel(self.tr(''))
        MOFIELDS_units = QLabel(self.tr(''))
        SOURCE_units = QLabel(self.tr('(Serial Only)'))
        DEST_units = QLabel(self.tr(''))
        HIPRESS_units = QLabel(self.tr('mbar'))
        LOPRESS_units = QLabel(self.tr('mbar'))
        HITEMP_units = QLabel(self.tr('C'))
        LOTEMP_units = QLabel(self.tr('C'))
        HIHUMID_units = QLabel(self.tr('%RH'))
        LOHUMID_units = QLabel(self.tr('%RH'))
        GEOFNUM_units = QLabel(self.tr('Num+Conf: 00 to 44'))
        GEOF1LAT_units = QLabel(self.tr('Degrees'))
        GEOF1LON_units = QLabel(self.tr('Degrees'))
        GEOF1RAD_units = QLabel(self.tr('m'))
        GEOF2LAT_units = QLabel(self.tr('Degrees'))
        GEOF2LON_units = QLabel(self.tr('Degrees'))
        GEOF2RAD_units = QLabel(self.tr('m'))
        GEOF3LAT_units = QLabel(self.tr('Degrees'))
        GEOF3LON_units = QLabel(self.tr('Degrees'))
        GEOF3RAD_units = QLabel(self.tr('m'))
        GEOF4LAT_units = QLabel(self.tr('Degrees'))
        GEOF4LON_units = QLabel(self.tr('Degrees'))
        GEOF4RAD_units = QLabel(self.tr('m'))
        WAKEINT_units = QLabel(self.tr('seconds'))
        ALARMINT_units = QLabel(self.tr('minutes'))
        TXINT_units = QLabel(self.tr('minutes'))
        LOWBATT_units = QLabel(self.tr('V'))
        DYNMODEL_units = QLabel(self.tr('0,2-10'))
        
        FLAGS1_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        FLAGS2_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        MOFIELDS_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        SOURCE_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        DEST_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        HIPRESS_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        LOPRESS_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        HITEMP_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        LOTEMP_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        HIHUMID_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        LOHUMID_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOFNUM_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF1LAT_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF1LON_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF1RAD_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF2LAT_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF2LON_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF2RAD_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF3LAT_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF3LON_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF3RAD_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF4LAT_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF4LON_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        GEOF4RAD_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        WAKEINT_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        ALARMINT_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        TXINT_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        LOWBATT_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        DYNMODEL_units.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        
        # Arrange Layout
        
        layout = QGridLayout()
        
        layout.addWidget(self.msg_label, 0, 0)
        layout.addWidget(self.fileLocation_lineedit, 0, 1)
        layout.addWidget(self.browse_btn, 0, 2)
        layout.addWidget(self.load_config_btn, 1, 2)
        layout.addWidget(self.save_config_btn, 2, 2)
        layout.addWidget(self.calc_config_btn, 3, 2)
        layout.addWidget(self.port_label, 5, 0)
        layout.addWidget(self.port_combobox, 5, 1)
        layout.addWidget(self.refresh_btn, 5, 2)
        layout.addWidget(self.open_port_btn, 6, 2)
        layout.addWidget(self.upload_btn, 7, 2)
        layout.addWidget(self.close_port_btn, 8, 2)
        layout.addWidget(self.terminal_label, 8, 0)
        layout.addWidget(self.terminal, 9, 0, 9, 3)
        layout.addWidget(self.messages_label, 18, 0)
        layout.addWidget(self.messages, 19, 0, 5, 3)
        layout.addWidget(self.config_label, 24, 0)
        layout.addWidget(self.config, 25, 0, 5, 3)

        layout.addWidget(FLAGS1_header, 0, 4)
        layout.addWidget(label_BINARY, 1, 4)
        layout.addWidget(label_DEST, 2, 4)
        layout.addWidget(label_HIPRESS, 3, 4)
        layout.addWidget(label_LOPRESS, 4, 4)
        layout.addWidget(label_HITEMP, 5, 4)
        layout.addWidget(label_LOTEMP, 6, 4)
        layout.addWidget(label_HIHUMID, 7, 4)
        layout.addWidget(label_LOHUMID, 8, 4)

        layout.addWidget(self.checkbox_F1_BINARY, 1, 5)
        layout.addWidget(self.checkbox_F1_DEST, 2, 5)
        layout.addWidget(self.checkbox_F1_HIPRESS, 3, 5)
        layout.addWidget(self.checkbox_F1_LOPRESS, 4, 5)
        layout.addWidget(self.checkbox_F1_HITEMP, 5, 5)
        layout.addWidget(self.checkbox_F1_LOTEMP, 6, 5)
        layout.addWidget(self.checkbox_F1_HIHUMID, 7, 5)
        layout.addWidget(self.checkbox_F1_LOHUMID, 8, 5)
        
        layout.addWidget(FLAGS2_header, 0, 6)
        layout.addWidget(label_GEOFENCE, 1, 6)
        layout.addWidget(label_INSIDE, 2, 6)
        layout.addWidget(label_LOWBATT, 3, 6)
        layout.addWidget(label_RING, 4, 6)

        layout.addWidget(self.checkbox_F2_GEOFENCE, 1, 7)
        layout.addWidget(self.checkbox_F2_INSIDE, 2, 7)
        layout.addWidget(self.checkbox_F2_LOWBATT, 3, 7)
        layout.addWidget(self.checkbox_F2_RING, 4, 7)

        layout.addWidget(USERFUNC_header, 0, 8)
        layout.addWidget(label_USERFUNC1, 1, 8)
        layout.addWidget(label_USERFUNC2, 2, 8)
        layout.addWidget(label_USERFUNC3, 3, 8)
        layout.addWidget(label_USERFUNC4, 4, 8)
        layout.addWidget(label_USERFUNC5, 5, 8)
        layout.addWidget(label_USERFUNC6, 6, 8)
        layout.addWidget(label_USERFUNC7, 7, 8)
        layout.addWidget(label_USERFUNC8, 8, 8)

        layout.addWidget(self.checkbox_USERFUNC1, 1, 9)
        layout.addWidget(self.checkbox_USERFUNC2, 2, 9)
        layout.addWidget(self.checkbox_USERFUNC3, 3, 9)
        layout.addWidget(self.checkbox_USERFUNC4, 4, 9)
        layout.addWidget(self.checkbox_USERFUNC5, 5, 9)
        layout.addWidget(self.checkbox_USERFUNC6, 6, 9)
        layout.addWidget(self.checkbox_USERFUNC7, 7, 9)
        layout.addWidget(self.checkbox_USERFUNC8, 8, 9)

        layout.addWidget(self.USERFUNC5_val, 5,10)
        layout.addWidget(self.USERFUNC6_val, 6,10)
        layout.addWidget(self.USERFUNC7_val, 7,10)
        layout.addWidget(self.USERFUNC8_val, 8,10)

        layout.addWidget(MOFIELDS_header, 9, 6)

        layout.addWidget(SWVER_label, 10, 4)
        layout.addWidget(SOURCE_label, 11, 4)
        layout.addWidget(BATTV_label, 12, 4)
        layout.addWidget(PRESS_label, 13, 4)
        layout.addWidget(TEMP_label, 14, 4)
        layout.addWidget(HUMID_label, 15, 4)
        layout.addWidget(YEAR_label, 16, 4)
        layout.addWidget(MONTH_label, 17, 4)
        layout.addWidget(DAY_label, 18, 4)
        layout.addWidget(HOUR_label, 19, 4)
        layout.addWidget(MIN_label, 20, 4)
        layout.addWidget(SEC_label, 21, 4)
        layout.addWidget(MILLIS_label, 22, 4)
        layout.addWidget(DATETIME_label, 23, 4)
        layout.addWidget(LAT_label, 24, 4)
        layout.addWidget(LON_label, 25, 4)
        layout.addWidget(ALT_label, 26, 4)
        layout.addWidget(SPEED_label, 27, 4)
        layout.addWidget(HEAD_label, 28, 4)
        layout.addWidget(SATS_label, 29, 4)

        layout.addWidget(self.checkbox_SWVER, 10, 5)
        layout.addWidget(self.checkbox_SOURCE, 11, 5)
        layout.addWidget(self.checkbox_BATTV, 12, 5)
        layout.addWidget(self.checkbox_PRESS, 13, 5)
        layout.addWidget(self.checkbox_TEMP, 14, 5)
        layout.addWidget(self.checkbox_HUMID, 15, 5)
        layout.addWidget(self.checkbox_YEAR, 16, 5)
        layout.addWidget(self.checkbox_MONTH,17 , 5)
        layout.addWidget(self.checkbox_DAY, 18, 5)
        layout.addWidget(self.checkbox_HOUR, 19, 5)
        layout.addWidget(self.checkbox_MIN, 20, 5)
        layout.addWidget(self.checkbox_SEC, 21, 5)
        layout.addWidget(self.checkbox_MILLIS, 22, 5)
        layout.addWidget(self.checkbox_DATETIME, 23, 5)
        layout.addWidget(self.checkbox_LAT, 24, 5)
        layout.addWidget(self.checkbox_LON, 25, 5)
        layout.addWidget(self.checkbox_ALT, 26, 5)
        layout.addWidget(self.checkbox_SPEED, 27, 5)
        layout.addWidget(self.checkbox_HEAD, 28, 5)
        layout.addWidget(self.checkbox_SATS, 29, 5)

        layout.addWidget(PDOP_label, 10, 6)
        layout.addWidget(FIX_label, 11, 6)
        layout.addWidget(GEOFSTAT_label, 12, 6)
        layout.addWidget(USERVAL1_label, 13, 6)
        layout.addWidget(USERVAL2_label, 14, 6)
        layout.addWidget(USERVAL3_label, 15, 6)
        layout.addWidget(USERVAL4_label, 16, 6)
        layout.addWidget(USERVAL5_label, 17, 6)
        layout.addWidget(USERVAL6_label, 18, 6)
        layout.addWidget(USERVAL7_label, 19, 6)
        layout.addWidget(USERVAL8_label, 20, 6)
        layout.addWidget(MOFIELDS_label, 21, 6)
        layout.addWidget(FLAGS1_label, 22, 6)
        layout.addWidget(FLAGS2_label, 23, 6)
        layout.addWidget(DEST_label, 24, 6)
        layout.addWidget(HIPRESS_label, 25, 6)
        layout.addWidget(LOPRESS_label, 26, 6)
        layout.addWidget(HITEMP_label, 27, 6)
        layout.addWidget(LOTEMP_label, 28, 6)
        layout.addWidget(HIHUMID_label, 29, 6)

        layout.addWidget(self.checkbox_PDOP, 10, 7)
        layout.addWidget(self.checkbox_FIX, 11, 7)
        layout.addWidget(self.checkbox_GEOFSTAT, 12, 7)
        layout.addWidget(self.checkbox_USERVAL1, 13, 7)
        layout.addWidget(self.checkbox_USERVAL2, 14, 7)
        layout.addWidget(self.checkbox_USERVAL3, 15, 7)
        layout.addWidget(self.checkbox_USERVAL4, 16, 7)
        layout.addWidget(self.checkbox_USERVAL5, 17, 7)
        layout.addWidget(self.checkbox_USERVAL6, 18, 7)
        layout.addWidget(self.checkbox_USERVAL7, 19, 7)
        layout.addWidget(self.checkbox_USERVAL8, 20, 7)
        layout.addWidget(self.checkbox_MOFIELDS, 21, 7)
        layout.addWidget(self.checkbox_FLAGS1, 22, 7)
        layout.addWidget(self.checkbox_FLAGS2, 23, 7)
        layout.addWidget(self.checkbox_DEST, 24, 7)
        layout.addWidget(self.checkbox_HIPRESS, 25, 7)
        layout.addWidget(self.checkbox_LOPRESS, 26, 7)
        layout.addWidget(self.checkbox_HITEMP, 27, 7)
        layout.addWidget(self.checkbox_LOTEMP, 28, 7)
        layout.addWidget(self.checkbox_HIHUMID, 29, 7)
        
        layout.addWidget(LOHUMID_label, 10, 8)
        layout.addWidget(GEOFNUM_label, 11, 8)
        layout.addWidget(GEOF1LAT_label, 12, 8)
        layout.addWidget(GEOF1LON_label, 13, 8)
        layout.addWidget(GEOF1RAD_label, 14, 8)
        layout.addWidget(GEOF2LAT_label, 15, 8)
        layout.addWidget(GEOF2LON_label, 16, 8)
        layout.addWidget(GEOF2RAD_label, 17, 8)
        layout.addWidget(GEOF3LAT_label, 18, 8)
        layout.addWidget(GEOF3LON_label, 19, 8)
        layout.addWidget(GEOF3RAD_label, 20, 8)
        layout.addWidget(GEOF4LAT_label, 21, 8)
        layout.addWidget(GEOF4LON_label, 22, 8)
        layout.addWidget(GEOF4RAD_label, 23, 8)
        layout.addWidget(WAKEINT_label, 24, 8)
        layout.addWidget(ALARMINT_label, 25, 8)
        layout.addWidget(TXINT_label, 26, 8)
        layout.addWidget(LOWBATT_label, 27, 8)
        layout.addWidget(DYNMODEL_label, 28, 8)

        layout.addWidget(self.checkbox_LOHUMID, 10, 9)
        layout.addWidget(self.checkbox_GEOFNUM, 11, 9)
        layout.addWidget(self.checkbox_GEOF1LAT, 12, 9)
        layout.addWidget(self.checkbox_GEOF1LON, 13, 9)
        layout.addWidget(self.checkbox_GEOF1RAD, 14, 9)
        layout.addWidget(self.checkbox_GEOF2LAT, 15, 9)
        layout.addWidget(self.checkbox_GEOF2LON, 16, 9)
        layout.addWidget(self.checkbox_GEOF2RAD, 17, 9)
        layout.addWidget(self.checkbox_GEOF3LAT, 18, 9)
        layout.addWidget(self.checkbox_GEOF3LON, 19, 9)
        layout.addWidget(self.checkbox_GEOF3RAD, 20, 9)
        layout.addWidget(self.checkbox_GEOF4LAT, 21, 9)
        layout.addWidget(self.checkbox_GEOF4LON, 22, 9)
        layout.addWidget(self.checkbox_GEOF4RAD, 23, 9)
        layout.addWidget(self.checkbox_WAKEINT, 24, 9)
        layout.addWidget(self.checkbox_ALARMINT, 25, 9)
        layout.addWidget(self.checkbox_TXINT, 26, 9)
        layout.addWidget(self.checkbox_LOWBATT, 27, 9)
        layout.addWidget(self.checkbox_DYNMODEL, 28, 9)
                         
        layout.addWidget(Values_label, 0, 11)
        layout.addWidget(FLAGS1_val_label, 1, 11)
        layout.addWidget(FLAGS2_val_label, 2, 11)
        layout.addWidget(MOFIELDS_val_label, 3, 11)
        layout.addWidget(SOURCE_val_label, 4, 11)
        layout.addWidget(DEST_val_label, 5, 11)
        layout.addWidget(HIPRESS_val_label, 6, 11)
        layout.addWidget(LOPRESS_val_label, 7, 11)
        layout.addWidget(HITEMP_val_label, 8, 11)
        layout.addWidget(LOTEMP_val_label, 9, 11)
        layout.addWidget(HIHUMID_val_label, 10, 11)
        layout.addWidget(LOHUMID_val_label, 11, 11)
        layout.addWidget(GEOFNUM_val_label, 12, 11)
        layout.addWidget(GEOF1LAT_val_label, 13, 11)
        layout.addWidget(GEOF1LON_val_label, 14, 11)
        layout.addWidget(GEOF1RAD_val_label, 15, 11)
        layout.addWidget(GEOF2LAT_val_label, 16, 11)
        layout.addWidget(GEOF2LON_val_label, 17, 11)
        layout.addWidget(GEOF2RAD_val_label, 18, 11)
        layout.addWidget(GEOF3LAT_val_label, 19, 11)
        layout.addWidget(GEOF3LON_val_label, 20, 11)
        layout.addWidget(GEOF3RAD_val_label, 21, 11)
        layout.addWidget(GEOF4LAT_val_label, 22, 11)
        layout.addWidget(GEOF4LON_val_label, 23, 11)
        layout.addWidget(GEOF4RAD_val_label, 24, 11)
        layout.addWidget(WAKEINT_val_label, 25, 11)
        layout.addWidget(ALARMINT_val_label, 26, 11)
        layout.addWidget(TXINT_val_label, 27, 11)
        layout.addWidget(LOWBATT_val_label, 28, 11)
        layout.addWidget(DYNMODEL_val_label, 29, 11)

        layout.addWidget(Include_label, 0, 12)
        layout.addWidget(self.checkbox_val_FLAGS1, 1, 12)
        layout.addWidget(self.checkbox_val_FLAGS2, 2, 12)
        layout.addWidget(self.checkbox_val_MOFIELDS, 3, 12)
        layout.addWidget(self.checkbox_val_SOURCE, 4, 12)
        layout.addWidget(self.checkbox_val_DEST, 5, 12)
        layout.addWidget(self.checkbox_val_HIPRESS, 6, 12)
        layout.addWidget(self.checkbox_val_LOPRESS, 7, 12)
        layout.addWidget(self.checkbox_val_HITEMP, 8, 12)
        layout.addWidget(self.checkbox_val_LOTEMP, 9, 12)
        layout.addWidget(self.checkbox_val_HIHUMID, 10, 12)
        layout.addWidget(self.checkbox_val_LOHUMID, 11, 12)
        layout.addWidget(self.checkbox_val_GEOFNUM, 12, 12)
        layout.addWidget(self.checkbox_val_GEOF1LAT, 13, 12)
        layout.addWidget(self.checkbox_val_GEOF1LON, 14, 12)
        layout.addWidget(self.checkbox_val_GEOF1RAD, 15, 12)
        layout.addWidget(self.checkbox_val_GEOF2LAT, 16, 12)
        layout.addWidget(self.checkbox_val_GEOF2LON, 17, 12)
        layout.addWidget(self.checkbox_val_GEOF2RAD, 18, 12)
        layout.addWidget(self.checkbox_val_GEOF3LAT, 19, 12)
        layout.addWidget(self.checkbox_val_GEOF3LON, 20, 12)
        layout.addWidget(self.checkbox_val_GEOF3RAD, 21, 12)
        layout.addWidget(self.checkbox_val_GEOF4LAT, 22, 12)
        layout.addWidget(self.checkbox_val_GEOF4LON, 23, 12)
        layout.addWidget(self.checkbox_val_GEOF4RAD, 24, 12)
        layout.addWidget(self.checkbox_val_WAKEINT, 25, 12)
        layout.addWidget(self.checkbox_val_ALARMINT, 26, 12)
        layout.addWidget(self.checkbox_val_TXINT, 27, 12)
        layout.addWidget(self.checkbox_val_LOWBATT, 28, 12)
        layout.addWidget(self.checkbox_val_DYNMODEL, 29, 12)

        layout.addWidget(self.val_SOURCE, 4, 13)
        layout.addWidget(self.val_DEST, 5, 13)
        layout.addWidget(self.val_HIPRESS, 6, 13)
        layout.addWidget(self.val_LOPRESS, 7, 13)
        layout.addWidget(self.val_HITEMP, 8, 13)
        layout.addWidget(self.val_LOTEMP, 9, 13)
        layout.addWidget(self.val_HIHUMID, 10, 13)
        layout.addWidget(self.val_LOHUMID, 11, 13)
        layout.addWidget(self.val_GEOFNUM, 12, 13)
        layout.addWidget(self.val_GEOF1LAT, 13, 13)
        layout.addWidget(self.val_GEOF1LON, 14, 13)
        layout.addWidget(self.val_GEOF1RAD, 15, 13)
        layout.addWidget(self.val_GEOF2LAT, 16, 13)
        layout.addWidget(self.val_GEOF2LON, 17, 13)
        layout.addWidget(self.val_GEOF2RAD, 18, 13)
        layout.addWidget(self.val_GEOF3LAT, 19, 13)
        layout.addWidget(self.val_GEOF3LON, 20, 13)
        layout.addWidget(self.val_GEOF3RAD, 21, 13)
        layout.addWidget(self.val_GEOF4LAT, 22, 13)
        layout.addWidget(self.val_GEOF4LON, 23, 13)
        layout.addWidget(self.val_GEOF4RAD, 24, 13)
        layout.addWidget(self.val_WAKEINT, 25, 13)
        layout.addWidget(self.val_ALARMINT, 26, 13)
        layout.addWidget(self.val_TXINT, 27, 13)
        layout.addWidget(self.val_LOWBATT, 28, 13)
        layout.addWidget(self.val_DYNMODEL, 29, 13)

        layout.addWidget(FLAGS1_units, 1, 14)
        layout.addWidget(FLAGS2_units, 2, 14)
        layout.addWidget(MOFIELDS_units, 3, 14)
        layout.addWidget(SOURCE_units, 4, 14)
        layout.addWidget(DEST_units, 5, 14)
        layout.addWidget(HIPRESS_units, 6, 14)
        layout.addWidget(LOPRESS_units, 7, 14)
        layout.addWidget(HITEMP_units, 8, 14)
        layout.addWidget(LOTEMP_units, 9, 14)
        layout.addWidget(HIHUMID_units, 10, 14)
        layout.addWidget(LOHUMID_units, 11, 14)
        layout.addWidget(GEOFNUM_units, 12, 14)
        layout.addWidget(GEOF1LAT_units, 13, 14)
        layout.addWidget(GEOF1LON_units, 14, 14)
        layout.addWidget(GEOF1RAD_units, 15, 14)
        layout.addWidget(GEOF2LAT_units, 16, 14)
        layout.addWidget(GEOF2LON_units, 17, 14)
        layout.addWidget(GEOF2RAD_units, 18, 14)
        layout.addWidget(GEOF3LAT_units, 19, 14)
        layout.addWidget(GEOF3LON_units, 20, 14)
        layout.addWidget(GEOF3RAD_units, 21, 14)
        layout.addWidget(GEOF4LAT_units, 22, 14)
        layout.addWidget(GEOF4LON_units, 23, 14)
        layout.addWidget(GEOF4RAD_units, 24, 14)
        layout.addWidget(WAKEINT_units, 25, 14)
        layout.addWidget(ALARMINT_units, 26, 14)
        layout.addWidget(TXINT_units, 27, 14)
        layout.addWidget(LOWBATT_units, 28, 14)
        layout.addWidget(DYNMODEL_units, 29, 14)

        self.setLayout(layout)

        self.load_settings()

        # Make the text edit windows read-only
        self.terminal.setReadOnly(True)
        self.messages.setReadOnly(True)
        self.config.setReadOnly(True)

    def load_settings(self) -> None:
        """Load Qsettings on startup."""
        self.settings = QSettings()
        
        port_name = self.settings.value(SETTING_PORT_NAME)
        if port_name is not None:
            index = self.port_combobox.findData(port_name)
            if index > -1:
                self.port_combobox.setCurrentIndex(index)

        msg = self.settings.value(SETTING_FILE_LOCATION)
        if msg is not None:
            self.fileLocation_lineedit.setText(msg)

    def save_settings(self) -> None:
        """Save Qsettings on shutdown."""
        self.settings = QSettings()
        self.settings.setValue(SETTING_PORT_NAME, self.port)
        self.settings.setValue(SETTING_FILE_LOCATION,
                          self.fileLocation_lineedit.text())

    def on_browse_btn_pressed(self) -> None:
        """Open dialog to select bin file."""
        options = QFileDialog.Options()
        fileName, _ = QFileDialog.getOpenFileName(
            None,
            "Select Configuration File",
            "",
            "Configuration Files (*.pkl);;All Files (*)",
            options=options)
        if fileName:
            self.fileLocation_lineedit.setText(fileName)
     
    def on_refresh_btn_pressed(self) -> None:
        """Refresh the list of serial (COM) ports"""
        self.update_com_ports()

    def on_upload_btn_pressed(self) -> None:
        """Upload the configuration message to the Tracker"""

        self.messages.clear() # Clear the message window

        portAvailable = False
        for desc, name, sys in gen_serial_ports():
            if (sys == self.port):
                portAvailable = True
        if (portAvailable == False):
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: Port No Longer Available!")
            self.messages.ensureCursorVisible()
            return
        
        try:
            if self.ser.isOpen():
                pass
        except:
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: Port Is Not Open!")
            self.messages.ensureCursorVisible()
            return

        if (self.config.toPlainText() == ''):
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Warning: Nothing To Do! Configuration Message Is Empty! Did you forget to click Calculate Config?")
            self.messages.ensureCursorVisible()
            return

        self.messages.moveCursor(QTextCursor.End)
        self.messages.ensureCursorVisible()
        self.messages.appendPlainText("Sending configuration message...")
        self.messages.ensureCursorVisible()

        self.ser.write(bytes(self.config.toPlainText(),'utf-8')) # Send the config message

        self.messages.moveCursor(QTextCursor.End)
        self.messages.ensureCursorVisible()
        self.messages.appendPlainText("Configuration message sent.")
        self.messages.ensureCursorVisible()

    def update_com_ports(self) -> None:
        """Update COM Port list in GUI."""
        self.port_combobox.clear()
        for desc, name, sys in gen_serial_ports():
            longname = desc + " (" + name + ")"
            self.port_combobox.addItem(longname, sys)

    @property
    def port(self) -> str:
        """Return the current serial port."""
        return self.port_combobox.currentData()

    def closeEvent(self, event: QCloseEvent) -> None:
        """Handle Close event of the Widget."""
        try:
            self.save_settings()
        except:
            pass

        try:
            self.ser.close()
        except:
            pass
        
        event.accept()

    def on_open_port_btn_pressed(self) -> None:
        """Check if port is available and open it"""

        self.messages.clear() # Clear the message window
        self.terminal.clear() # Clear the serial terminal window
        
        portAvailable = False
        for desc, name, sys in gen_serial_ports():
            if (sys == self.port):
                portAvailable = True
        if (portAvailable == False):
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: Port No Longer Available!")
            self.messages.ensureCursorVisible()
            return

        try:
            if self.ser.isOpen():
                self.messages.moveCursor(QTextCursor.End)
                self.messages.ensureCursorVisible()
                self.messages.appendPlainText("Error: Port Is Already Open!")
                self.messages.ensureCursorVisible()
                return
        except:
            pass
        
        try:
            self.ser = QSerialPort()
            self.ser.setPortName(self.port)
            self.ser.setBaudRate(QSerialPort.Baud115200)
            self.ser.open(QIODevice.ReadWrite)
            try:
                self.ser.setRequestToSend(False)
            except:
                pass
        except:
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: Could Not Open The Port!")
            self.messages.ensureCursorVisible()
            return

        self.ser.readyRead.connect(self.receive) # Connect the receiver
        
        try:
            sleep(0.5) # Pull RTS low for 0.5s to reset the Artemis
            self.ser.setRequestToSend(True)
        except:
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: Could Not Toggle The RTS Pin!")
            self.messages.ensureCursorVisible()
            #return
        
        self.messages.moveCursor(QTextCursor.End)
        self.messages.ensureCursorVisible()
        self.messages.appendPlainText("Port is now open.")
        self.messages.ensureCursorVisible()

    @pyqtSlot()
    def receive(self) -> None:
        try:
            while self.ser.canReadLine():
                text = self.ser.readLine().data().decode()
                self.terminal.moveCursor(QTextCursor.End)
                self.terminal.ensureCursorVisible()
                self.terminal.insertPlainText(text)
                self.terminal.ensureCursorVisible()
        except:
            pass

    def on_close_port_btn_pressed(self) -> None:
        """Close the port"""

        self.messages.clear() # Clear the message window
        
        portAvailable = False
        for desc, name, sys in gen_serial_ports():
            if (sys == self.port):
                portAvailable = True
        if (portAvailable == False):
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: Port No Longer Available!")
            self.messages.ensureCursorVisible()
            return

        try:
            if not self.ser.isOpen():
                self.messages.moveCursor(QTextCursor.End)
                self.messages.ensureCursorVisible()
                self.messages.appendPlainText("Error: Port Is Already Closed!")
                self.messages.ensureCursorVisible()
                return
        except:
            pass
        
        try:
            self.ser.close()
        except:
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: Could Not Close The Port!")
            self.messages.ensureCursorVisible()
            return

        self.messages.moveCursor(QTextCursor.End)
        self.messages.ensureCursorVisible()
        self.messages.appendPlainText("Port is now closed.")
        self.messages.ensureCursorVisible()

    def on_load_config_btn_pressed(self) -> None:
        """Load a configuration for a pickle (.pkl) file"""
        fileExists = False
        try:
            f = open(self.fileLocation_lineedit.text())
            fileExists = True
            f.close()
        except IOError:
            fileExists = False
        if (fileExists == False):
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: File Not Found!")
            self.messages.ensureCursorVisible()
            return

        try:
            fp = open(self.fileLocation_lineedit.text(),"rb")
            self.the_settings = pickle.load(fp)
            fp.close()
        except:
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: Load Config Failed!")
            self.messages.ensureCursorVisible()
            return

        self.checkbox_F1_BINARY.setChecked(self.the_settings["F1_BINARY"])
        self.checkbox_F1_DEST.setChecked(self.the_settings["F1_DEST"])
        self.checkbox_F1_HIPRESS.setChecked(self.the_settings["F1_HIPRESS"])
        self.checkbox_F1_LOPRESS.setChecked(self.the_settings["F1_LOPRESS"])
        self.checkbox_F1_HITEMP.setChecked(self.the_settings["F1_HITEMP"])
        self.checkbox_F1_LOTEMP.setChecked(self.the_settings["F1_LOTEMP"])
        self.checkbox_F1_HIHUMID.setChecked(self.the_settings["F1_HIHUMID"])
        self.checkbox_F1_LOHUMID.setChecked(self.the_settings["F1_LOHUMID"])
        self.checkbox_F2_GEOFENCE.setChecked(self.the_settings["F2_GEOFENCE"])
        self.checkbox_F2_INSIDE.setChecked(self.the_settings["F2_INSIDE"])
        self.checkbox_F2_LOWBATT.setChecked(self.the_settings["F2_LOWBATT"])
        self.checkbox_F2_RING.setChecked(self.the_settings["F2_RING"])
        self.checkbox_USERFUNC1.setChecked(self.the_settings["USERFUNC1"])
        self.checkbox_USERFUNC2.setChecked(self.the_settings["USERFUNC2"])
        self.checkbox_USERFUNC3.setChecked(self.the_settings["USERFUNC3"])
        self.checkbox_USERFUNC4.setChecked(self.the_settings["USERFUNC4"])
        self.checkbox_USERFUNC5.setChecked(self.the_settings["USERFUNC5"])
        self.checkbox_USERFUNC6.setChecked(self.the_settings["USERFUNC6"])
        self.checkbox_USERFUNC7.setChecked(self.the_settings["USERFUNC7"])
        self.checkbox_USERFUNC8.setChecked(self.the_settings["USERFUNC8"])
        self.checkbox_SWVER.setChecked(self.the_settings["SWVER"])
        self.checkbox_SOURCE.setChecked(self.the_settings["SOURCE"])
        self.checkbox_BATTV.setChecked(self.the_settings["BATTV"])
        self.checkbox_PRESS.setChecked(self.the_settings["PRESS"])
        self.checkbox_TEMP.setChecked(self.the_settings["TEMP"])
        self.checkbox_HUMID.setChecked(self.the_settings["HUMID"])
        self.checkbox_YEAR.setChecked(self.the_settings["YEAR"])
        self.checkbox_MONTH.setChecked(self.the_settings["MONTH"])
        self.checkbox_DAY.setChecked(self.the_settings["DAY"])
        self.checkbox_HOUR.setChecked(self.the_settings["HOUR"])
        self.checkbox_MIN.setChecked(self.the_settings["MIN"])
        self.checkbox_SEC.setChecked(self.the_settings["SEC"])
        self.checkbox_MILLIS.setChecked(self.the_settings["MILLIS"])
        self.checkbox_DATETIME.setChecked(self.the_settings["DATETIME"])
        self.checkbox_LAT.setChecked(self.the_settings["LAT"])
        self.checkbox_LON.setChecked(self.the_settings["LON"])
        self.checkbox_ALT.setChecked(self.the_settings["ALT"])
        self.checkbox_SPEED.setChecked(self.the_settings["SPEED"])
        self.checkbox_HEAD.setChecked(self.the_settings["HEAD"])
        self.checkbox_SATS.setChecked(self.the_settings["SATS"])
        self.checkbox_PDOP.setChecked(self.the_settings["PDOP"])
        self.checkbox_FIX.setChecked(self.the_settings["FIX"])
        self.checkbox_GEOFSTAT.setChecked(self.the_settings["GEOFSTAT"])
        self.checkbox_USERVAL1.setChecked(self.the_settings["USERVAL1"])
        self.checkbox_USERVAL2.setChecked(self.the_settings["USERVAL2"])
        self.checkbox_USERVAL3.setChecked(self.the_settings["USERVAL3"])
        self.checkbox_USERVAL4.setChecked(self.the_settings["USERVAL4"])
        self.checkbox_USERVAL5.setChecked(self.the_settings["USERVAL5"])
        self.checkbox_USERVAL6.setChecked(self.the_settings["USERVAL6"])
        self.checkbox_USERVAL7.setChecked(self.the_settings["USERVAL7"])
        self.checkbox_USERVAL8.setChecked(self.the_settings["USERVAL8"])
        self.checkbox_MOFIELDS.setChecked(self.the_settings["MOFIELDS"])
        self.checkbox_FLAGS1.setChecked(self.the_settings["FLAGS1"])
        self.checkbox_FLAGS2.setChecked(self.the_settings["FLAGS2"])
        self.checkbox_DEST.setChecked(self.the_settings["DEST"])
        self.checkbox_HIPRESS.setChecked(self.the_settings["HIPRESS"])
        self.checkbox_LOPRESS.setChecked(self.the_settings["LOPRESS"])
        self.checkbox_HITEMP.setChecked(self.the_settings["HITEMP"])
        self.checkbox_LOTEMP.setChecked(self.the_settings["LOTEMP"])
        self.checkbox_HIHUMID.setChecked(self.the_settings["HIHUMID"])
        self.checkbox_LOHUMID.setChecked(self.the_settings["LOHUMID"])
        self.checkbox_GEOFNUM.setChecked(self.the_settings["GEOFNUM"])
        self.checkbox_GEOF1LAT.setChecked(self.the_settings["GEOF1LAT"])
        self.checkbox_GEOF1LON.setChecked(self.the_settings["GEOF1LON"])
        self.checkbox_GEOF1RAD.setChecked(self.the_settings["GEOF1RAD"])
        self.checkbox_GEOF2LAT.setChecked(self.the_settings["GEOF2LAT"])
        self.checkbox_GEOF2LON.setChecked(self.the_settings["GEOF2LON"])
        self.checkbox_GEOF2RAD.setChecked(self.the_settings["GEOF2RAD"])
        self.checkbox_GEOF3LAT.setChecked(self.the_settings["GEOF3LAT"])
        self.checkbox_GEOF3LON.setChecked(self.the_settings["GEOF3LON"])
        self.checkbox_GEOF3RAD.setChecked(self.the_settings["GEOF3RAD"])
        self.checkbox_GEOF4LAT.setChecked(self.the_settings["GEOF4LAT"])
        self.checkbox_GEOF4LON.setChecked(self.the_settings["GEOF4LON"])
        self.checkbox_GEOF4RAD.setChecked(self.the_settings["GEOF4RAD"])
        self.checkbox_WAKEINT.setChecked(self.the_settings["WAKEINT"])
        self.checkbox_ALARMINT.setChecked(self.the_settings["ALARMINT"])
        self.checkbox_TXINT.setChecked(self.the_settings["TXINT"])
        self.checkbox_LOWBATT.setChecked(self.the_settings["LOWBATT"])
        self.checkbox_DYNMODEL.setChecked(self.the_settings["DYNMODEL"])
        self.checkbox_val_FLAGS1.setChecked(self.the_settings["val_FLAGS1"])
        self.checkbox_val_FLAGS2.setChecked(self.the_settings["val_FLAGS2"])
        self.checkbox_val_MOFIELDS.setChecked(self.the_settings["val_MOFIELDS"])
        self.checkbox_val_SOURCE.setChecked(self.the_settings["val_SOURCE"])
        self.checkbox_val_DEST.setChecked(self.the_settings["val_DEST"])
        self.checkbox_val_HIPRESS.setChecked(self.the_settings["val_HIPRESS"])
        self.checkbox_val_LOPRESS.setChecked(self.the_settings["val_LOPRESS"])
        self.checkbox_val_HITEMP.setChecked(self.the_settings["val_HITEMP"])
        self.checkbox_val_LOTEMP.setChecked(self.the_settings["val_LOTEMP"])
        self.checkbox_val_HIHUMID.setChecked(self.the_settings["val_HIHUMID"])
        self.checkbox_val_LOHUMID.setChecked(self.the_settings["val_LOHUMID"])
        self.checkbox_val_GEOFNUM.setChecked(self.the_settings["val_GEOFNUM"])
        self.checkbox_val_GEOF1LAT.setChecked(self.the_settings["val_GEOF1LAT"])
        self.checkbox_val_GEOF1LON.setChecked(self.the_settings["val_GEOF1LON"])
        self.checkbox_val_GEOF1RAD.setChecked(self.the_settings["val_GEOF1RAD"])
        self.checkbox_val_GEOF2LAT.setChecked(self.the_settings["val_GEOF2LAT"])
        self.checkbox_val_GEOF2LON.setChecked(self.the_settings["val_GEOF2LON"])
        self.checkbox_val_GEOF2RAD.setChecked(self.the_settings["val_GEOF2RAD"])
        self.checkbox_val_GEOF3LAT.setChecked(self.the_settings["val_GEOF3LAT"])
        self.checkbox_val_GEOF3LON.setChecked(self.the_settings["val_GEOF3LON"])
        self.checkbox_val_GEOF3RAD.setChecked(self.the_settings["val_GEOF3RAD"])
        self.checkbox_val_GEOF4LAT.setChecked(self.the_settings["val_GEOF4LAT"])
        self.checkbox_val_GEOF4LON.setChecked(self.the_settings["val_GEOF4LON"])
        self.checkbox_val_GEOF4RAD.setChecked(self.the_settings["val_GEOF4RAD"])
        self.checkbox_val_WAKEINT.setChecked(self.the_settings["val_WAKEINT"])
        self.checkbox_val_ALARMINT.setChecked(self.the_settings["val_ALARMINT"])
        self.checkbox_val_TXINT.setChecked(self.the_settings["val_TXINT"])
        self.checkbox_val_LOWBATT.setChecked(self.the_settings["val_LOWBATT"])
        self.checkbox_val_DYNMODEL.setChecked(self.the_settings["val_DYNMODEL"])
        self.val_SOURCE.setText(self.the_settings["value_SOURCE"])
        self.val_DEST.setText(self.the_settings["value_DEST"])
        self.val_HIPRESS.setText(self.the_settings["value_HIPRESS"])
        self.val_LOPRESS.setText(self.the_settings["value_LOPRESS"])
        self.val_HITEMP.setText(self.the_settings["value_HITEMP"])
        self.val_LOTEMP.setText(self.the_settings["value_LOTEMP"])
        self.val_HIHUMID.setText(self.the_settings["value_HIHUMID"])
        self.val_LOHUMID.setText(self.the_settings["value_LOHUMID"])
        self.val_GEOFNUM.setText(self.the_settings["value_GEOFNUM"])
        self.val_GEOF1LAT.setText(self.the_settings["value_GEOF1LAT"])
        self.val_GEOF1LON.setText(self.the_settings["value_GEOF1LON"])
        self.val_GEOF1RAD.setText(self.the_settings["value_GEOF1RAD"])
        self.val_GEOF2LAT.setText(self.the_settings["value_GEOF2LAT"])
        self.val_GEOF2LON.setText(self.the_settings["value_GEOF2LON"])
        self.val_GEOF2RAD.setText(self.the_settings["value_GEOF2RAD"])
        self.val_GEOF3LAT.setText(self.the_settings["value_GEOF3LAT"])
        self.val_GEOF3LON.setText(self.the_settings["value_GEOF3LON"])
        self.val_GEOF3RAD.setText(self.the_settings["value_GEOF3RAD"])
        self.val_GEOF4LAT.setText(self.the_settings["value_GEOF4LAT"])
        self.val_GEOF4LON.setText(self.the_settings["value_GEOF4LON"])
        self.val_GEOF4RAD.setText(self.the_settings["value_GEOF4RAD"])
        self.val_WAKEINT.setText(self.the_settings["value_WAKEINT"])
        self.val_ALARMINT.setText(self.the_settings["value_ALARMINT"])
        self.val_TXINT.setText(self.the_settings["value_TXINT"])
        self.val_LOWBATT.setText(self.the_settings["value_LOWBATT"])
        self.val_DYNMODEL.setText(self.the_settings["value_DYNMODEL"])
        
        self.messages.moveCursor(QTextCursor.End)
        self.messages.ensureCursorVisible()
        self.messages.appendPlainText("Configuration loaded.")
        self.messages.ensureCursorVisible()

    def on_save_config_btn_pressed(self) -> None:
        """Save the configuration to a pickle (.pkl) file"""
        fileExists = False
        try:
            f = open(self.fileLocation_lineedit.text())
            fileExists = True
            f.close()
        except IOError:
            fileExists = False
        if (fileExists == True):
            buttonReply = QMessageBox.question(self, '', "The file already exists. Do you want to replace it?", QMessageBox.Yes | QMessageBox.Cancel, QMessageBox.Cancel)
            if buttonReply == QMessageBox.Cancel:
                self.messages.moveCursor(QTextCursor.End)
                self.messages.ensureCursorVisible()
                self.messages.appendPlainText("File save cancelled!")
                self.messages.ensureCursorVisible()
                return
        self.the_settings = {
            "F1_BINARY": self.checkbox_F1_BINARY.isChecked(),
            "F1_DEST": self.checkbox_F1_DEST.isChecked(),
            "F1_HIPRESS": self.checkbox_F1_HIPRESS.isChecked(),
            "F1_LOPRESS": self.checkbox_F1_LOPRESS.isChecked(),
            "F1_HITEMP": self.checkbox_F1_HITEMP.isChecked(),
            "F1_LOTEMP": self.checkbox_F1_LOTEMP.isChecked(),
            "F1_HIHUMID": self.checkbox_F1_HIHUMID.isChecked(),
            "F1_LOHUMID": self.checkbox_F1_LOHUMID.isChecked(),
            "F2_GEOFENCE": self.checkbox_F2_GEOFENCE.isChecked(),
            "F2_INSIDE": self.checkbox_F2_INSIDE.isChecked(),
            "F2_LOWBATT": self.checkbox_F2_LOWBATT.isChecked(),
            "F2_RING": self.checkbox_F2_RING.isChecked(),
            "USERFUNC1": self.checkbox_USERFUNC1.isChecked(),
            "USERFUNC2": self.checkbox_USERFUNC2.isChecked(),
            "USERFUNC3": self.checkbox_USERFUNC3.isChecked(),
            "USERFUNC4": self.checkbox_USERFUNC4.isChecked(),
            "USERFUNC5": self.checkbox_USERFUNC5.isChecked(),
            "USERFUNC6": self.checkbox_USERFUNC6.isChecked(),
            "USERFUNC7": self.checkbox_USERFUNC7.isChecked(),
            "USERFUNC8": self.checkbox_USERFUNC8.isChecked(),
            "SWVER": self.checkbox_SWVER.isChecked(),
            "SOURCE": self.checkbox_SOURCE.isChecked(),
            "BATTV": self.checkbox_BATTV.isChecked(),
            "PRESS": self.checkbox_PRESS.isChecked(),
            "TEMP": self.checkbox_TEMP.isChecked(),
            "HUMID": self.checkbox_HUMID.isChecked(),
            "YEAR": self.checkbox_YEAR.isChecked(),
            "MONTH": self.checkbox_MONTH.isChecked(),
            "DAY": self.checkbox_DAY.isChecked(),
            "HOUR": self.checkbox_HOUR.isChecked(),
            "MIN": self.checkbox_MIN.isChecked(),
            "SEC": self.checkbox_SEC.isChecked(),
            "MILLIS": self.checkbox_MILLIS.isChecked(),
            "DATETIME": self.checkbox_DATETIME.isChecked(),
            "LAT": self.checkbox_LAT.isChecked(),
            "LON": self.checkbox_LON.isChecked(),
            "ALT": self.checkbox_ALT.isChecked(),
            "SPEED": self.checkbox_SPEED.isChecked(),
            "HEAD": self.checkbox_HEAD.isChecked(),
            "SATS": self.checkbox_SATS.isChecked(),
            "PDOP": self.checkbox_PDOP.isChecked(),
            "FIX": self.checkbox_FIX.isChecked(),
            "GEOFSTAT": self.checkbox_GEOFSTAT.isChecked(),
            "USERVAL1": self.checkbox_USERVAL1.isChecked(),
            "USERVAL2": self.checkbox_USERVAL2.isChecked(),
            "USERVAL3": self.checkbox_USERVAL3.isChecked(),
            "USERVAL4": self.checkbox_USERVAL4.isChecked(),
            "USERVAL5": self.checkbox_USERVAL5.isChecked(),
            "USERVAL6": self.checkbox_USERVAL6.isChecked(),
            "USERVAL7": self.checkbox_USERVAL7.isChecked(),
            "USERVAL8": self.checkbox_USERVAL8.isChecked(),
            "MOFIELDS": self.checkbox_MOFIELDS.isChecked(),
            "FLAGS1": self.checkbox_FLAGS1.isChecked(),
            "FLAGS2": self.checkbox_FLAGS2.isChecked(),
            "DEST": self.checkbox_DEST.isChecked(),
            "HIPRESS": self.checkbox_HIPRESS.isChecked(),
            "LOPRESS": self.checkbox_LOPRESS.isChecked(),
            "HITEMP": self.checkbox_HITEMP.isChecked(),
            "LOTEMP": self.checkbox_LOTEMP.isChecked(),
            "HIHUMID": self.checkbox_HIHUMID.isChecked(),
            "LOHUMID": self.checkbox_LOHUMID.isChecked(),
            "GEOFNUM": self.checkbox_GEOFNUM.isChecked(),
            "GEOF1LAT": self.checkbox_GEOF1LAT.isChecked(),
            "GEOF1LON": self.checkbox_GEOF1LON.isChecked(),
            "GEOF1RAD": self.checkbox_GEOF1RAD.isChecked(),
            "GEOF2LAT": self.checkbox_GEOF2LAT.isChecked(),
            "GEOF2LON": self.checkbox_GEOF2LON.isChecked(),
            "GEOF2RAD": self.checkbox_GEOF2RAD.isChecked(),
            "GEOF3LAT": self.checkbox_GEOF3LAT.isChecked(),
            "GEOF3LON": self.checkbox_GEOF3LON.isChecked(),
            "GEOF3RAD": self.checkbox_GEOF3RAD.isChecked(),
            "GEOF4LAT": self.checkbox_GEOF4LAT.isChecked(),
            "GEOF4LON": self.checkbox_GEOF4LON.isChecked(),
            "GEOF4RAD": self.checkbox_GEOF4RAD.isChecked(),
            "WAKEINT": self.checkbox_WAKEINT.isChecked(),
            "ALARMINT": self.checkbox_ALARMINT.isChecked(),
            "TXINT": self.checkbox_TXINT.isChecked(),
            "LOWBATT": self.checkbox_LOWBATT.isChecked(),
            "DYNMODEL": self.checkbox_DYNMODEL.isChecked(),
            "val_FLAGS1": self.checkbox_val_FLAGS1.isChecked(),
            "val_FLAGS2": self.checkbox_val_FLAGS2.isChecked(),
            "val_MOFIELDS": self.checkbox_val_MOFIELDS.isChecked(),
            "val_SOURCE": self.checkbox_val_SOURCE.isChecked(),
            "val_DEST": self.checkbox_val_DEST.isChecked(),
            "val_HIPRESS": self.checkbox_val_HIPRESS.isChecked(),
            "val_LOPRESS": self.checkbox_val_LOPRESS.isChecked(),
            "val_HITEMP": self.checkbox_val_HITEMP.isChecked(),
            "val_LOTEMP": self.checkbox_val_LOTEMP.isChecked(),
            "val_HIHUMID": self.checkbox_val_HIHUMID.isChecked(),
            "val_LOHUMID": self.checkbox_val_LOHUMID.isChecked(),
            "val_GEOFNUM": self.checkbox_val_GEOFNUM.isChecked(),
            "val_GEOF1LAT": self.checkbox_val_GEOF1LAT.isChecked(),
            "val_GEOF1LON": self.checkbox_val_GEOF1LON.isChecked(),
            "val_GEOF1RAD": self.checkbox_val_GEOF1RAD.isChecked(),
            "val_GEOF2LAT": self.checkbox_val_GEOF2LAT.isChecked(),
            "val_GEOF2LON": self.checkbox_val_GEOF2LON.isChecked(),
            "val_GEOF2RAD": self.checkbox_val_GEOF2RAD.isChecked(),
            "val_GEOF3LAT": self.checkbox_val_GEOF3LAT.isChecked(),
            "val_GEOF3LON": self.checkbox_val_GEOF3LON.isChecked(),
            "val_GEOF3RAD": self.checkbox_val_GEOF3RAD.isChecked(),
            "val_GEOF4LAT": self.checkbox_val_GEOF4LAT.isChecked(),
            "val_GEOF4LON": self.checkbox_val_GEOF4LON.isChecked(),
            "val_GEOF4RAD": self.checkbox_val_GEOF4RAD.isChecked(),
            "val_WAKEINT": self.checkbox_val_WAKEINT.isChecked(),
            "val_ALARMINT": self.checkbox_val_ALARMINT.isChecked(),
            "val_TXINT": self.checkbox_val_TXINT.isChecked(),
            "val_LOWBATT": self.checkbox_val_LOWBATT.isChecked(),
            "val_DYNMODEL": self.checkbox_val_DYNMODEL.isChecked(),
            "value_SOURCE": self.val_SOURCE.text(),
            "value_DEST": self.val_DEST.text(),
            "value_HIPRESS": self.val_HIPRESS.text(),
            "value_LOPRESS": self.val_LOPRESS.text(),
            "value_HITEMP": self.val_HITEMP.text(),
            "value_LOTEMP": self.val_LOTEMP.text(),
            "value_HIHUMID": self.val_HIHUMID.text(),
            "value_LOHUMID": self.val_LOHUMID.text(),
            "value_GEOFNUM": self.val_GEOFNUM.text(),
            "value_GEOF1LAT": self.val_GEOF1LAT.text(),
            "value_GEOF1LON": self.val_GEOF1LON.text(),
            "value_GEOF1RAD": self.val_GEOF1RAD.text(),
            "value_GEOF2LAT": self.val_GEOF2LAT.text(),
            "value_GEOF2LON": self.val_GEOF2LON.text(),
            "value_GEOF2RAD": self.val_GEOF2RAD.text(),
            "value_GEOF3LAT": self.val_GEOF3LAT.text(),
            "value_GEOF3LON": self.val_GEOF3LON.text(),
            "value_GEOF3RAD": self.val_GEOF3RAD.text(),
            "value_GEOF4LAT": self.val_GEOF4LAT.text(),
            "value_GEOF4LON": self.val_GEOF4LON.text(),
            "value_GEOF4RAD": self.val_GEOF4RAD.text(),
            "value_WAKEINT": self.val_WAKEINT.text(),
            "value_ALARMINT": self.val_ALARMINT.text(),
            "value_TXINT": self.val_TXINT.text(),
            "value_LOWBATT": self.val_LOWBATT.text(),
            "value_DYNMODEL": self.val_DYNMODEL.text()
        }
        try:
            fp = open(self.fileLocation_lineedit.text(),"wb")
            pickle.dump(self.the_settings, fp)
            fp.close()
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Configuration saved.")
            self.messages.ensureCursorVisible()
        except:
            self.messages.moveCursor(QTextCursor.End)
            self.messages.ensureCursorVisible()
            self.messages.appendPlainText("Error: File Save Failed!")
            self.messages.ensureCursorVisible()

    def on_calc_config_btn_pressed(self) -> None:
        """Calculate the configuration message"""
        self.messages.clear() # Clear the message window
        config_str = "02" # Add the STX
        # FLAGS1
        flags = 0
        if self.checkbox_F1_BINARY.isChecked(): flags = flags | 0b10000000
        if self.checkbox_F1_DEST.isChecked(): flags = flags | 0b01000000
        if self.checkbox_F1_HIPRESS.isChecked(): flags = flags | 0b00100000
        if self.checkbox_F1_LOPRESS.isChecked(): flags = flags | 0b00010000
        if self.checkbox_F1_HITEMP.isChecked(): flags = flags | 0b00001000
        if self.checkbox_F1_LOTEMP.isChecked(): flags = flags | 0b00000100
        if self.checkbox_F1_HIHUMID.isChecked(): flags = flags | 0b00000010
        if self.checkbox_F1_LOHUMID.isChecked(): flags = flags | 0b00000001
        if self.checkbox_val_FLAGS1.isChecked():
            config_str = config_str + "31" + struct.pack('B', flags).hex()
        elif flags > 0:
            self.messages.moveCursor(QTextCursor.End)
            self.messages.appendPlainText("Warning: FLAGS1 has bits set but the FLAGS1 Include checkbox is not checked")
            self.messages.ensureCursorVisible()
        # FLAGS2
        flags = 0
        if self.checkbox_F2_GEOFENCE.isChecked():
            flags = flags | 0b10000000
            wake_int = 60
            alarm_int = 5
            try:
                if self.val_WAKEINT.text().isdigit():
                    wake_int = int(self.val_WAKEINT.text())
                if self.val_ALARMINT.text().isdigit():
                    alarm_int = int(self.val_ALARMINT.text())
            except:
                pass
            if ((wake_int / 60) < (alarm_int)):
                self.messages.moveCursor(QTextCursor.End)
                self.messages.appendPlainText("Warning: the WAKEINT interval should be the same as ALARMINT when geofence alerts are enabled")
                self.messages.ensureCursorVisible()                
        if self.checkbox_F2_INSIDE.isChecked(): flags = flags | 0b01000000
        if self.checkbox_F2_LOWBATT.isChecked(): flags = flags | 0b00100000
        if self.checkbox_F2_RING.isChecked():
            flags = flags | 0b00010000
            wake_int = 60
            alarm_int = 5
            tx_int = 5
            try:
                if self.val_WAKEINT.text().isdigit():
                    wake_int = int(self.val_WAKEINT.text())
                if self.val_ALARMINT.text().isdigit():
                    alarm_int = int(self.val_ALARMINT.text())
                if self.val_TXINT.text().isdigit():
                    tx_int = int(self.val_TXINT.text())
            except:
                pass
            if ((wake_int / 60) < (alarm_int)) and ((wake_int / 60) < (tx_int)):
                self.messages.moveCursor(QTextCursor.End)
                self.messages.appendPlainText("Warning: the WAKEINT interval should be the same as ALARMINT and TXINT when monitoring the ring channel")
                self.messages.ensureCursorVisible()                
        if self.checkbox_val_FLAGS2.isChecked():
            config_str = config_str + "32" + struct.pack('B', flags).hex()
        elif flags > 0:
            self.messages.moveCursor(QTextCursor.End)
            self.messages.appendPlainText("Warning: FLAGS2 has bits set but the FLAGS2 Include checkbox is not checked")
            self.messages.ensureCursorVisible()
        # MOFIELDS
        flags0 = 0
        flags1 = 0
        flags2 = 0
        maxTxtLen = 0 # Keep a running total of the maximum text message length
        if self.checkbox_F1_DEST.isChecked(): maxTxtLen += 10 # Include the RockBLOCK header
        if self.checkbox_SWVER.isChecked():
            flags0 = flags0 | 0x08000000
            maxTxtLen += 6
        if self.checkbox_SOURCE.isChecked():
            flags0 = flags0 | 0x00800000
            maxTxtLen += 8
        if self.checkbox_BATTV.isChecked():
            flags0 = flags0 | 0x00400000
            maxTxtLen += 5
        if self.checkbox_PRESS.isChecked():
            flags0 = flags0 | 0x00200000
            maxTxtLen += 5
        if self.checkbox_TEMP.isChecked():
            flags0 = flags0 | 0x00100000
            maxTxtLen += 7
        if self.checkbox_HUMID.isChecked():
            flags0 = flags0 | 0x00080000
            maxTxtLen += 7
        if self.checkbox_YEAR.isChecked():
            flags0 = flags0 | 0x00040000
            maxTxtLen += 5
        if self.checkbox_MONTH.isChecked():
            flags0 = flags0 | 0x00020000
            maxTxtLen += 3
        if self.checkbox_DAY.isChecked():
            flags0 = flags0 | 0x00010000
            maxTxtLen += 3
        if self.checkbox_HOUR.isChecked():
            flags0 = flags0 | 0x00008000
            maxTxtLen += 3
        if self.checkbox_MIN.isChecked():
            flags0 = flags0 | 0x00004000
            maxTxtLen += 3
        if self.checkbox_SEC.isChecked():
            flags0 = flags0 | 0x00002000
            maxTxtLen += 3
        if self.checkbox_MILLIS.isChecked():
            flags0 = flags0 | 0x00001000
            maxTxtLen += 4
        if self.checkbox_DATETIME.isChecked():
            flags0 = flags0 | 0x00000800
            maxTxtLen += 15
        if self.checkbox_LAT.isChecked():
            flags0 = flags0 | 0x00000400
            maxTxtLen += 12
        if self.checkbox_LON.isChecked():
            flags0 = flags0 | 0x00000200
            maxTxtLen += 13
        if self.checkbox_ALT.isChecked():
            flags0 = flags0 | 0x00000100
            maxTxtLen += 10
        if self.checkbox_SPEED.isChecked():
            flags0 = flags0 | 0x00000080
            maxTxtLen += 8
        if self.checkbox_HEAD.isChecked():
            flags0 = flags0 | 0x00000040
            maxTxtLen += 7
        if self.checkbox_SATS.isChecked():
            flags0 = flags0 | 0x00000020
            maxTxtLen += 3
        if self.checkbox_PDOP.isChecked():
            flags0 = flags0 | 0x00000010
            maxTxtLen += 7
        if self.checkbox_FIX.isChecked():
            flags0 = flags0 | 0x00000008
            maxTxtLen += 2
        if self.checkbox_GEOFSTAT.isChecked():
            flags0 = flags0 | 0x00000004
            maxTxtLen += 7
        if self.checkbox_USERVAL1.isChecked():
            flags1 = flags1 | 0x80000000
            maxTxtLen += 4
        if self.checkbox_USERVAL2.isChecked():
            flags1 = flags1 | 0x40000000
            maxTxtLen += 4
        if self.checkbox_USERVAL3.isChecked():
            flags1 = flags1 | 0x20000000
            maxTxtLen += 6
        if self.checkbox_USERVAL4.isChecked():
            flags1 = flags1 | 0x10000000
            maxTxtLen += 6
        if self.checkbox_USERVAL5.isChecked():
            flags1 = flags1 | 0x08000000
            maxTxtLen += 11
        if self.checkbox_USERVAL6.isChecked():
            flags1 = flags1 | 0x04000000
            maxTxtLen += 11
        if self.checkbox_USERVAL7.isChecked():
            flags1 = flags1 | 0x02000000
            maxTxtLen += 15
        if self.checkbox_USERVAL8.isChecked():
            flags1 = flags1 | 0x01000000
            maxTxtLen += 15
        if self.checkbox_MOFIELDS.isChecked():
            flags1 = flags1 | 0x00008000
            maxTxtLen += 25
        if self.checkbox_FLAGS1.isChecked():
            flags1 = flags1 | 0x00004000
            maxTxtLen += 3
        if self.checkbox_FLAGS2.isChecked():
            flags1 = flags1 | 0x00002000
            maxTxtLen += 3
        if self.checkbox_DEST.isChecked():
            flags1 = flags1 | 0x00001000
            maxTxtLen += 8
        if self.checkbox_HIPRESS.isChecked():
            flags1 = flags1 | 0x00000800
            maxTxtLen += 5
        if self.checkbox_LOPRESS.isChecked():
            flags1 = flags1 | 0x00000400
            maxTxtLen += 5
        if self.checkbox_HITEMP.isChecked():
            flags1 = flags1 | 0x00000200
            maxTxtLen += 7
        if self.checkbox_LOTEMP.isChecked():
            flags1 = flags1 | 0x00000100
            maxTxtLen += 7
        if self.checkbox_HIHUMID.isChecked():
            flags1 = flags1 | 0x00000080
            maxTxtLen += 7
        if self.checkbox_LOHUMID.isChecked():
            flags1 = flags1 | 0x00000040
            maxTxtLen += 7
        if self.checkbox_GEOFNUM.isChecked():
            flags1 = flags1 | 0x00000020
            maxTxtLen += 3
        if self.checkbox_GEOF1LAT.isChecked():
            flags1 = flags1 | 0x00000010
            maxTxtLen += 12
        if self.checkbox_GEOF1LON.isChecked():
            flags1 = flags1 | 0x00000008
            maxTxtLen += 13
        if self.checkbox_GEOF1RAD.isChecked():
            flags1 = flags1 | 0x00000004
            maxTxtLen += 10
        if self.checkbox_GEOF2LAT.isChecked():
            flags1 = flags1 | 0x00000002
            maxTxtLen += 12
        if self.checkbox_GEOF2LON.isChecked():
            flags1 = flags1 | 0x00000001
            maxTxtLen += 13
        if self.checkbox_GEOF2RAD.isChecked():
            flags2 = flags2 | 0x80000000
            maxTxtLen += 10
        if self.checkbox_GEOF3LAT.isChecked():
            flags2 = flags2 | 0x40000000
            maxTxtLen += 12
        if self.checkbox_GEOF3LON.isChecked():
            flags2 = flags2 | 0x20000000
            maxTxtLen += 13
        if self.checkbox_GEOF3RAD.isChecked():
            flags2 = flags2 | 0x10000000
            maxTxtLen += 10
        if self.checkbox_GEOF4LAT.isChecked():
            flags2 = flags2 | 0x08000000
            maxTxtLen += 12
        if self.checkbox_GEOF4LON.isChecked():
            flags2 = flags2 | 0x04000000
            maxTxtLen += 13
        if self.checkbox_GEOF4RAD.isChecked():
            flags2 = flags2 | 0x02000000
            maxTxtLen += 10
        if self.checkbox_WAKEINT.isChecked():
            flags2 = flags2 | 0x01000000
            maxTxtLen += 5
        if self.checkbox_ALARMINT.isChecked():
            flags2 = flags2 | 0x00800000
            maxTxtLen += 5
        if self.checkbox_TXINT.isChecked():
            flags2 = flags2 | 0x00400000
            maxTxtLen += 5
        if self.checkbox_LOWBATT.isChecked():
            flags2 = flags2 | 0x00200000
            maxTxtLen += 5
        if self.checkbox_DYNMODEL.isChecked():
            flags2 = flags2 | 0x00100000
            maxTxtLen += 3
        if (self.checkbox_F1_BINARY.isChecked() == False) and (maxTxtLen > 340):
            self.messages.moveCursor(QTextCursor.End)
            self.messages.appendPlainText("Error: The text message sent by the Tracker will exceed 340 bytes! You need to select fewer MOFIELDS.")
            self.messages.ensureCursorVisible()
        if self.checkbox_val_MOFIELDS.isChecked():
            config_str = config_str + "30" + struct.pack('<I', flags0).hex() # Little-endian hex
            config_str = config_str + struct.pack('<I', flags1).hex()
            config_str = config_str + struct.pack('<I', flags2).hex()
        elif (flags0 > 0) or (flags1 > 0) or (flags2 > 0):
            self.messages.moveCursor(QTextCursor.End)
            self.messages.appendPlainText("Warning: MOFIELDS has bits set but the MOFIELDS Include checkbox is not checked")
            self.messages.ensureCursorVisible()
        # Fields Values
        if self.checkbox_val_SOURCE.isChecked():
            if self.val_SOURCE.text().isdigit():
                try:
                    value = int(self.val_SOURCE.text())
                    if (value < 0) or (value > 9999999):
                        self.messages.appendPlainText("Error: the value for SOURCE is not valid!")
                    else:
                        config_str = config_str + "08" + struct.pack('<I', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for SOURCE is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for SOURCE is not valid!")
        if self.checkbox_val_DEST.isChecked():
            if self.val_DEST.text().isdigit():
                try:
                    value = int(self.val_DEST.text())
                    if (value < 0) or (value > 9999999):
                        self.messages.appendPlainText("Error: the value for DEST is not valid!")
                    else:
                        config_str = config_str + "33" + struct.pack('<I', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for DEST is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for DEST is not valid!")
        if self.checkbox_val_HIPRESS.isChecked():
            if self.val_HIPRESS.text().isdigit():
                try:
                    value = int(self.val_HIPRESS.text())
                    if (value < 0) or (value > 1084):
                        self.messages.appendPlainText("Error: the value for HIPRESS is not valid!")
                    else:
                        config_str = config_str + "34" + struct.pack('<H', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for HIPRESS is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for HIPRESS is not valid!")
        if self.checkbox_val_LOPRESS.isChecked():
            if self.val_LOPRESS.text().isdigit():
                try:
                    value = int(self.val_LOPRESS.text())
                    if (value < 0) or (value > 1084):
                        self.messages.appendPlainText("Error: the value for LOPRESS is not valid!")
                    else:
                        config_str = config_str + "35" + struct.pack('<H', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for LOPRESS is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for LOPRESS is not valid!")
        if self.checkbox_val_HITEMP.isChecked():
            try:
                value = float(self.val_HITEMP.text())
                if (value < -40.0) or (value > 85.0):
                    self.messages.appendPlainText("Error: the value for HITEMP is not valid!")
                else:
                    value = int(value * 100.0)
                    config_str = config_str + "36" + struct.pack('<h', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for HITEMP is not valid!")
        if self.checkbox_val_LOTEMP.isChecked():
            try:
                value = float(self.val_LOTEMP.text())
                if (value < -40.0) or (value > 85.0):
                    self.messages.appendPlainText("Error: the value for LOTEMP is not valid!")
                else:
                    value = int(value * 100.0)
                    config_str = config_str + "37" + struct.pack('<h', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for LOTEMP is not valid!")
        if self.checkbox_val_HIHUMID.isChecked():
            try:
                value = float(self.val_HIHUMID.text())
                if (value < 0.0) or (value > 100.0):
                    self.messages.appendPlainText("Error: the value for HIHUMID is not valid!")
                else:
                    value = int(value * 100.0)
                    config_str = config_str + "38" + struct.pack('<H', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for HIHUMID is not valid!")
        if self.checkbox_val_LOHUMID.isChecked():
            try:
                value = float(self.val_LOHUMID.text())
                if (value < 0.0) or (value > 100.0):
                    self.messages.appendPlainText("Error: the value for LOHUMID is not valid!")
                else:
                    value = int(value * 100.0)
                    config_str = config_str + "39" + struct.pack('<H', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for LOHUMID is not valid!")
        if self.checkbox_val_GEOFNUM.isChecked():
            numf = 0 # The number of geofences
            conf = 0 # The confidence level
            if self.val_GEOFNUM.text().isdigit():
                try:
                    value = float(self.val_GEOFNUM.text())
                    numf = int(value / 10)
                    conf = int(value % 10)
                    if (numf < 0) or (numf > 4) or (conf < 0) or (conf > 4):
                        self.messages.appendPlainText("Error: the value for GEOFNUM is not valid!")
                    else:
                        value = (numf * 16) + conf
                        config_str = config_str + "3a" + struct.pack('B', value).hex()
                except:
                    self.messages.appendPlainText("Error: the value for GEOFNUM is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for GEOFNUM is not valid!")
                return
            ## The next 20 lines are useful but are commented out as they stop you setting GEOFNUM to zero
            ## and resetting the GEOFnLAT/LON/RAD in a single message
            #if numf < 4: # De-select the LAT/LON/RAD for unused geofences
            #    self.checkbox_val_GEOF4LAT.setChecked(False)
            #    self.checkbox_val_GEOF4LON.setChecked(False)
            #    self.checkbox_val_GEOF4RAD.setChecked(False)
            #    self.messages.appendPlainText("Warning: unchecking the GEOF4 settings to match GEOFNUM")
            #if numf < 3:
            #    self.checkbox_val_GEOF3LAT.setChecked(False)
            #    self.checkbox_val_GEOF3LON.setChecked(False)
            #    self.checkbox_val_GEOF3RAD.setChecked(False)
            #    self.messages.appendPlainText("Warning: unchecking the GEOF3 settings to match GEOFNUM")
            #if numf < 2:
            #    self.checkbox_val_GEOF2LAT.setChecked(False)
            #    self.checkbox_val_GEOF2LON.setChecked(False)
            #    self.checkbox_val_GEOF2RAD.setChecked(False)
            #    self.messages.appendPlainText("Warning: unchecking the GEOF2 settings to match GEOFNUM")
            #if numf < 1:
            #    self.checkbox_val_GEOF1LAT.setChecked(False)
            #    self.checkbox_val_GEOF1LON.setChecked(False)
            #    self.checkbox_val_GEOF1RAD.setChecked(False)
            #    self.messages.appendPlainText("Warning: unchecking the GEOF1 settings to match GEOFNUM")
        if self.checkbox_val_GEOF1LAT.isChecked():
            try:
                value = float(self.val_GEOF1LAT.text())
                if (value < -90.0) or (value >= 90.0):
                    self.messages.appendPlainText("Error: the value for GEOF1LAT is not valid!")
                else:
                    value = int(value * 1E7) # Convert to degrees ^ 10-7
                    config_str = config_str + "3b" + struct.pack('<i', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF1LAT is not valid!")
        if self.checkbox_val_GEOF1LON.isChecked():
            try:
                value = float(self.val_GEOF1LON.text())
                if (value < -180.0) or (value >= 180.0):
                    self.messages.appendPlainText("Error: the value for GEOF1LON is not valid!")
                else:
                    value = int(value * 1E7) # Convert to degrees ^ 10-7
                    config_str = config_str + "3c" + struct.pack('<i', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF1LON is not valid!")
        if self.checkbox_val_GEOF1RAD.isChecked():
            try:
                value = float(self.val_GEOF1RAD.text())
                if (value < 0.0) or (value > 100000.0):
                    self.messages.appendPlainText("Error: the value for GEOF1RAD is not valid!")
                else:
                    value = int(value * 100.0) # Convert to cm
                    config_str = config_str + "3d" + struct.pack('<I', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF1RAD is not valid!")
        if self.checkbox_val_GEOF2LAT.isChecked():
            try:
                value = float(self.val_GEOF2LAT.text())
                if (value < -90.0) or (value >= 90.0):
                    self.messages.appendPlainText("Error: the value for GEOF2LAT is not valid!")
                else:
                    value = int(value * 1E7) # Convert to degrees ^ 10-7
                    config_str = config_str + "3e" + struct.pack('<i', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF2LAT is not valid!")
        if self.checkbox_val_GEOF2LON.isChecked():
            try:
                value = float(self.val_GEOF2LON.text())
                if (value < -180.0) or (value >= 180.0):
                    self.messages.appendPlainText("Error: the value for GEOF2LON is not valid!")
                else:
                    value = int(value * 1E7) # Convert to degrees ^ 10-7
                    config_str = config_str + "3f" + struct.pack('<i', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF2LON is not valid!")
        if self.checkbox_val_GEOF2RAD.isChecked():
            try:
                value = float(self.val_GEOF2RAD.text())
                if (value < 0.0) or (value > 100000.0):
                    self.messages.appendPlainText("Error: the value for GEOF2RAD is not valid!")
                else:
                    value = int(value * 100.0) # Convert to cm
                    config_str = config_str + "40" + struct.pack('<I', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF2RAD is not valid!")
        if self.checkbox_val_GEOF3LAT.isChecked():
            try:
                value = float(self.val_GEOF3LAT.text())
                if (value < -90.0) or (value >= 90.0):
                    self.messages.appendPlainText("Error: the value for GEOF3LAT is not valid!")
                else:
                    value = int(value * 1E7) # Convert to degrees ^ 10-7
                    config_str = config_str + "41" + struct.pack('<i', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF3LAT is not valid!")
        if self.checkbox_val_GEOF3LON.isChecked():
            try:
                value = float(self.val_GEOF3LON.text())
                if (value < -180.0) or (value >= 180.0):
                    self.messages.appendPlainText("Error: the value for GEOF3LON is not valid!")
                else:
                    value = int(value * 1E7) # Convert to degrees ^ 10-7
                    config_str = config_str + "42" + struct.pack('<i', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF3LON is not valid!")
        if self.checkbox_val_GEOF3RAD.isChecked():
            try:
                value = float(self.val_GEOF3RAD.text())
                if (value < 0.0) or (value > 100000.0):
                    self.messages.appendPlainText("Error: the value for GEOF3RAD is not valid!")
                else:
                    value = int(value * 100.0) # Convert to cm
                    config_str = config_str + "43" + struct.pack('<I', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF3RAD is not valid!")
        if self.checkbox_val_GEOF4LAT.isChecked():
            try:
                value = float(self.val_GEOF4LAT.text())
                if (value < -90.0) or (value >= 90.0):
                    self.messages.appendPlainText("Error: the value for GEOF4LAT is not valid!")
                else:
                    value = int(value * 1E7) # Convert to degrees ^ 10-7
                    config_str = config_str + "44" + struct.pack('<i', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF4LAT is not valid!")
        if self.checkbox_val_GEOF4LON.isChecked():
            try:
                value = float(self.val_GEOF4LON.text())
                if (value < -180.0) or (value >= 180.0):
                    self.messages.appendPlainText("Error: the value for GEOF4LON is not valid!")
                else:
                    value = int(value * 1E7) # Convert to degrees ^ 10-7
                    config_str = config_str + "45" + struct.pack('<i', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF4LON is not valid!")
        if self.checkbox_val_GEOF4RAD.isChecked():
            try:
                value = float(self.val_GEOF4RAD.text())
                if (value < 0.0) or (value > 100000.0):
                    self.messages.appendPlainText("Error: the value for GEOF4RAD is not valid!")
                else:
                    value = int(value * 100.0) # Convert to cm
                    config_str = config_str + "46" + struct.pack('<I', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for GEOF4RAD is not valid!")
        if self.checkbox_val_WAKEINT.isChecked():
            if self.val_WAKEINT.text().isdigit():
                try:
                    longvalue = int(self.val_WAKEINT.text())
                    if (longvalue < 0) or (longvalue > 86400):
                        self.messages.appendPlainText("Error: the value for WAKEINT is not valid!")
                    else:
                        config_str = config_str + "47" + struct.pack('<I', longvalue).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for WAKEINT is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for WAKEINT is not valid!")
        if self.checkbox_val_ALARMINT.isChecked():
            if self.val_ALARMINT.text().isdigit():
                try:
                    value = int(self.val_ALARMINT.text())
                    if (value < 0) or (value > 1440):
                        self.messages.appendPlainText("Error: the value for ALARMINT is not valid!")
                    else:
                        config_str = config_str + "48" + struct.pack('<H', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for ALARMINT is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for ALARMINT is not valid!")
        if self.checkbox_val_TXINT.isChecked():
            if self.val_TXINT.text().isdigit():
                try:
                    value = int(self.val_TXINT.text())
                    if (value < 0) or (value > 1440):
                        self.messages.appendPlainText("Error: the value for TXINT is not valid!")
                    else:
                        config_str = config_str + "49" + struct.pack('<H', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for TXINT is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for TXINT is not valid!")
        if self.checkbox_val_LOWBATT.isChecked():
            try:
                value = float(self.val_LOWBATT.text())
                if (value < 0.0) or (value > 9.99):
                    self.messages.appendPlainText("Error: the value for LOWBATT is not valid!")
                else:
                    value = int(value * 100.0)
                    config_str = config_str + "4a" + struct.pack('<H', value).hex() # Little-endian hex
            except:
                self.messages.appendPlainText("Error: the value for LOWBATT is not valid!")
        if self.checkbox_val_DYNMODEL.isChecked():
            if self.val_DYNMODEL.text().isdigit():
                try:
                    value = int(self.val_DYNMODEL.text())
                    if (value < 0) or (value == 1) or (value > 10):
                        self.messages.appendPlainText("Error: the value for DYNMODEL is not valid!")
                    else:
                        config_str = config_str + "4b" + struct.pack('B', value).hex()
                except:
                    self.messages.appendPlainText("Error: the value for DYNMODEL is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for DYNMODEL is not valid!")
        # USERFUNCs
        if self.checkbox_USERFUNC1.isChecked():
            config_str = config_str + "58"
        if self.checkbox_USERFUNC2.isChecked():
            config_str = config_str + "59"
        if self.checkbox_USERFUNC3.isChecked():
            config_str = config_str + "5a"
        if self.checkbox_USERFUNC4.isChecked():
            config_str = config_str + "5b"
        if self.checkbox_USERFUNC5.isChecked():
            if self.USERFUNC5_val.text().isdigit():
                try:
                    value = int(self.USERFUNC5_val.text())
                    if (value < 0) or (value > 65535):
                        self.messages.appendPlainText("Error: the value for USERFUNC5 is not valid!")
                    else:
                        config_str = config_str + "5c" + struct.pack('<H', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for USERFUNC5 is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for USERFUNC5 is not valid!")
        if self.checkbox_USERFUNC6.isChecked():
            if self.USERFUNC6_val.text().isdigit():
                try:
                    value = int(self.USERFUNC6_val.text())
                    if (value < 0) or (value > 65535):
                        self.messages.appendPlainText("Error: the value for USERFUNC6 is not valid!")
                    else:
                        config_str = config_str + "5d" + struct.pack('<H', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for USERFUNC6 is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for USERFUNC6 is not valid!")
        if self.checkbox_USERFUNC7.isChecked():
            if self.USERFUNC7_val.text().isdigit():
                try:
                    value = int(self.USERFUNC7_val.text())
                    if (value < 0) or (value > 4294967295):
                        self.messages.appendPlainText("Error: the value for USERFUNC7 is not valid!")
                    else:
                        config_str = config_str + "5e" + struct.pack('<I', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for USERFUNC7 is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for USERFUNC7 is not valid!")
        if self.checkbox_USERFUNC8.isChecked():
            if self.USERFUNC8_val.text().isdigit():
                try:
                    value = int(self.USERFUNC8_val.text())
                    if (value < 0) or (value > 4294967295):
                        self.messages.appendPlainText("Error: the value for USERFUNC8 is not valid!")
                    else:
                        config_str = config_str + "5f" + struct.pack('<I', value).hex() # Little-endian hex
                except:
                    self.messages.appendPlainText("Error: the value for USERFUNC8 is not valid!")
            else:
                self.messages.appendPlainText("Error: the value for USERFUNC8 is not valid!")
        # Add the ETX
        config_str = config_str + "03"
        # Calculate and append the checksum bytes
        csuma = 0
        csumb = 0
        for i in range(0, len(config_str), 2):
            pair = int(config_str[i:i+2], 16) # Grab a pair of hex digits as an int
            csuma = csuma + pair
            csumb = csumb + csuma
        config_str = config_str + struct.pack('B', (csuma%256)).hex() # Add the checksum bytes to the message
        config_str = config_str + struct.pack('B', (csumb%256)).hex()
        # Display the message            
        self.config.clear() # Clear the config window
        self.config.appendPlainText(config_str) # Display the config message

if __name__ == '__main__':
    from sys import exit as sysExit
    app = QApplication([])
    app.setOrganizationName('SparkX')
    app.setApplicationName('Artemis Global Tracker Configuration Tool ' + guiVersion)
    w = MainWidget()
    w.show()
    sysExit(app.exec_())
