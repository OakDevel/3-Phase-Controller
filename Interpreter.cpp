
#include "Interpreter.h"
#include "Config.h"
#include "ThreePhaseController.h"
// #include "MotorControl.h"
#include "MLX90363.h"
#include "TwillBotInterface.h"

bool Interpreter::streaming = false;

void Interpreter::interpretFromMaster(u1 const * const incomingData){
	if (checkCRC(incomingData)){
		return;
	}

	if (incomingData[0] == 0x20){
		s2 torque = *((s2*)(incomingData+1));
		
		if(torque > ThreePhaseController::getMaxTorque())
			torque = ThreePhaseController::getMaxTorque();
		else if ( torque < -ThreePhaseController::getMaxTorque())
			torque = -ThreePhaseController::getMaxTorque();

		ThreePhaseController::setTorque(torque);
	}

	if (incomingData[0] == 0x10) {
		s4 go = *((s4*)(incomingData+1));


	}

	if (incomingData[0] == 0x88){
		//Start Streaming
		if (incomingData[1] == 0xF0){
			streaming = true;
		}
		//Stop Streaming
		else if (incomingData[1] == 0x0F){	
			streaming = false;
		}
	}
}

void Interpreter::sendNormalDataToMaster(){
	if (!streaming) return;
	if (!MLX90363::isMeasurementReady()) return;

	MLX90363::startTransmitting();
	while (MLX90363::isTransmitting());

	u2 * const buff = (u2 * const)TwillBotInterface::getOutgoingWriteBuffer();

	buff[0] = MLX90363::getAlpha();
	buff[1] = 0;
	buff[2] = MLX90363::getRoll();
	buff[3] = MLX90363::getErr();
	buff[4] = 0;
	
	TwillBotInterface::getOutgoingWriteBuffer()[Config::i2cBufferOutgoingSize-1] = getCRC(TwillBotInterface::getOutgoingWriteBuffer(), Config::i2cBufferOutgoingSize-1);

	TwillBotInterface::releaseNextWriteBuffer();
}

//check the message to check if it passes CRC check
bool Interpreter::checkCRC(u1 const * const mes){
	u1 sum = 0;

	for(u1 i = 0; i < Config::i2cBufferIncomingSize; i++)
		//sum = _crc8_ccitt_update(sum, mes[i]);
		sum += mes[i];

	return sum != 0xFF;
}

u1 Interpreter::getCRC(u1 const * const mes, u1 length){
	u1 sum = 0;
	
	for(u1 i = 0; i < length; i++)
		//sum = _crc8_ccitt_update(sum, mes[i]);
		sum += mes[i];

	return ~sum;
}