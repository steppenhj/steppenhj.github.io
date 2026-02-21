/********************************************************************
	Rhapsody	: 10.0 
	Login		: 82106
	Component	: component_9 
	Configuration 	: DefaultConfig
	Model Element	: VehicleController
//!	Generated Date	: Sat, 21, Feb 2026  
	File Path	: component_9\DefaultConfig\VehicleController.cpp
*********************************************************************/

//## auto_generated
#include "VehicleController.h"
//## link itsUdpReceiver
#include "UdpReceiver.h"
//## package Default

//## class VehicleController
VehicleController::VehicleController(IOxfActive* theActiveContext) {
    setActiveContext(this, true);
    itsUdpReceiver = NULL;
    initStatechart();
}

VehicleController::~VehicleController() {
    cleanUpRelations();
    cancelTimeouts();
}

UdpReceiver* VehicleController::getItsUdpReceiver() const {
    return itsUdpReceiver;
}

void VehicleController::setItsUdpReceiver(UdpReceiver* p_UdpReceiver) {
    if(p_UdpReceiver != NULL)
        {
            p_UdpReceiver->_setItsVehicleController(this);
        }
    _setItsUdpReceiver(p_UdpReceiver);
}

bool VehicleController::startBehavior() {
    bool done = false;
    done = OMReactive::startBehavior();
    if(done)
        {
            startDispatching();
        }
    return done;
}

void VehicleController::initStatechart() {
    rootState_subState = OMNonState;
    rootState_active = OMNonState;
    rootState_timeout = NULL;
}

void VehicleController::cleanUpRelations() {
    if(itsUdpReceiver != NULL)
        {
            VehicleController* p_VehicleController = itsUdpReceiver->getItsVehicleController();
            if(p_VehicleController != NULL)
                {
                    itsUdpReceiver->__setItsVehicleController(NULL);
                }
            itsUdpReceiver = NULL;
        }
}

void VehicleController::cancelTimeouts() {
    cancel(rootState_timeout);
}

bool VehicleController::cancelTimeout(const IOxfTimeout* arg) {
    bool res = false;
    if(rootState_timeout == arg)
        {
            rootState_timeout = NULL;
            res = true;
        }
    return res;
}

void VehicleController::__setItsUdpReceiver(UdpReceiver* p_UdpReceiver) {
    itsUdpReceiver = p_UdpReceiver;
}

void VehicleController::_setItsUdpReceiver(UdpReceiver* p_UdpReceiver) {
    if(itsUdpReceiver != NULL)
        {
            itsUdpReceiver->__setItsVehicleController(NULL);
        }
    __setItsUdpReceiver(p_UdpReceiver);
}

void VehicleController::_clearItsUdpReceiver() {
    itsUdpReceiver = NULL;
}

void VehicleController::rootState_entDef() {
    {
        pushNullTransition();
        rootState_subState = INIT;
        rootState_active = INIT;
    }
}

IOxfReactive::TakeEventStatus VehicleController::rootState_processEvent() {
    IOxfReactive::TakeEventStatus res = eventNotConsumed;
    switch (rootState_active) {
        // State INIT
        case INIT:
        {
            if(IS_EVENT_TYPE_OF(OMNullEventId))
                {
                    popNullTransition();
                    rootState_subState = OPERATING;
                    rootState_active = OPERATING;
                    rootState_timeout = scheduleTimeout(500, NULL);
                    res = eventConsumed;
                }
            
        }
        break;
        // State OPERATING
        case OPERATING:
        {
            if(IS_EVENT_TYPE_OF(OMTimeoutEventId))
                {
                    if(getCurrentEvent() == rootState_timeout)
                        {
                            cancel(rootState_timeout);
                            rootState_subState = FAIL_SAFE;
                            rootState_active = FAIL_SAFE;
                            //#[ state FAIL_SAFE.(Entry) 
                            std::cout << "[WARNING] 통신 두절! FAIL_SAFE 진입 - 모터 정지 명령 0" << std::endl;
                            //#]
                            res = eventConsumed;
                        }
                }
            else if(IS_EVENT_TYPE_OF(evUpdateCmd_Default_id))
                {
                    OMSETPARAMS(evUpdateCmd);
                    //#[ transition 4 
                    std::cout << "[OPERATING] Motor CMD: th=" << th << ", st=" << st << std::endl;
                    //#]
                    res = eventConsumed;
                }
            
        }
        break;
        // State FAIL_SAFE
        case FAIL_SAFE:
        {
            if(IS_EVENT_TYPE_OF(evUpdateCmd_Default_id))
                {
                    OMSETPARAMS(evUpdateCmd);
                    rootState_subState = OPERATING;
                    rootState_active = OPERATING;
                    rootState_timeout = scheduleTimeout(500, NULL);
                    res = eventConsumed;
                }
            
        }
        break;
        default:
            break;
    }
    return res;
}

/*********************************************************************
	File Path	: component_9\DefaultConfig\VehicleController.cpp
*********************************************************************/
