#include "StreamLogger.h"

StreamLogger* StreamLogger::_instance = NULL ;

StreamLogger*	StreamLogger::getInstance() {
	if ( _instance == NULL ) {
		_instance = new StreamLogger() ;
	}
	return _instance ;
}


