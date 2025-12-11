#include "SIM800L.h"

String SIM800L::_readSerial()
{
	_timeout=0;
	while(!_serial->available() && _timeout<(TIMEOUT*100)) 
	{
		delay(10);
		yield();
		_timeout++;
	}
	if(_serial->available())
	{
		return _serial->readString();
	}
	return "";
}

void SIM800L::_clearSerial()
{
	if(_serial->available())
	{
		_serial->readString();
	}
}

SIM800L::SIM800L(void)
{
  _serialBuffer.reserve(1500);
}

bool SIM800L::begin(Stream &serial)
{
	_serial = &serial;
	delay(1000);
	yield();
	_clearSerial();

	_serial->print(F("AT\r\n"));
	_serialBuffer=_readSerial();
	if ( (_serialBuffer.indexOf("OK") )!=-1 )
	{
		yield();
		_clearSerial();
		_serial->print(F("ATE0\r\n"));
		_serialBuffer=_readSerial();
		if ((_serialBuffer.indexOf("OK"))!=-1 )
		{	
			return true;
		}
		else 
		{
			return false;
		}
	}
	else 
	{
		return false;
	}
}

bool SIM800L::begin(Stream &serial,uint8_t pin)
{
	rstpin=pin;
	pinMode(rstpin,OUTPUT);
	digitalWrite(rstpin,LOW);
	rstDeclair=true;
	begin(serial);
}

bool SIM800L::sendSMS(char* number,char* text)
{
	_clearSerial();
	_serial->print(F("AT+CMGF=1\r"));
	_serialBuffer=_readSerial();
	
	if(_serialBuffer.indexOf("OK")!=-1 )
	{
		_clearSerial();
		_serial->print(F("AT+CMGS=\""));
		_serial->print(number);
		_serial->print(F("\"\r"));
		_serialBuffer=_readSerial();
		
		if(_serialBuffer.indexOf(">")!=-1 )
		{
			_clearSerial();
			_serial->print(text);
			_serial->print("\r");
			_serial->print((char)26);
			_serialBuffer=_readSerial();
			
			if(_serialBuffer.indexOf("OK")!=-1 )
			{
				return true;
			}
			else
			{
				return false;
			}	
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

String SIM800L::readSMS(uint8_t msgIndex)
{
	_clearSerial();
	_serial->print(F("AT+CMGF=1\r"));
	_serialBuffer=_readSerial();
	
	if(_serialBuffer.indexOf("OK")!=-1 )
	{
		_clearSerial();
		_serial->print(F("AT+CMGR="));
		_serial->print(msgIndex);
		_serial->print("\r");
		_serialBuffer=_readSerial();
		
		if(_serialBuffer.indexOf("+CMGR:")!=-1 )
		{
			uint8_t smsEndIndex=_serialBuffer.indexOf("\r\n\r\nOK");
			return _serialBuffer.substring(0,smsEndIndex);
		}
		else
		{
			return "";
		}
	}
	else
	{
		return "";
	}
}

int8_t SIM800L::signalStrength()
{
	_clearSerial();
	_serial->print(F("AT+CSQ\r"));
	_serialBuffer=_readSerial();
	
	if(_serialBuffer.indexOf("OK")!=-1)
	{
		int8_t signalIndex=_serialBuffer.indexOf(":");
		_serialBuffer=_serialBuffer.substring(signalIndex+1,_serialBuffer.length());
		signalIndex=_serialBuffer.indexOf(",");
		_serialBuffer=_serialBuffer.substring(0,signalIndex);
		_serialBuffer.trim();
		return _serialBuffer.toInt();
	}
	else 
	{
		return -1;
	}
}

bool SIM800L::checkNetwork()
{
	_clearSerial();
	_serial->print(F("AT+CREG?\r"));
	_serialBuffer=_readSerial();
	
	if(_serialBuffer.indexOf("OK")!=-1)
	{
		if((_serialBuffer.indexOf(",1")!=-1)||(_serialBuffer.indexOf(",5")!=-1))
		{
			return true;
		}
		else 
		{
			return false;
		}
	}
	else 
	{
		return false;
	}
}

String SIM800L::serviceProvider()
{
	_clearSerial();
	_serial->print(F("AT+COPS?\r"));
	_serialBuffer=_readSerial();
	
	if(_serialBuffer.indexOf("OK")!=-1)
	{
		uint8_t indexOne=_serialBuffer.indexOf("\"");
		uint8_t indexTwo=_serialBuffer.indexOf("\"",indexOne+1);
		return _serialBuffer.substring(indexOne+1,indexTwo);
	}
	else 
	{
		return "";
	}
}
