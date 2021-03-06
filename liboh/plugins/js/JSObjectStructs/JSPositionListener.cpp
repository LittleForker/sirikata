
#include <v8.h>
#include "JSPresenceStruct.hpp"
#include "../JSObjectScript.hpp"
#include "../JSSerializer.hpp"
#include "../JSLogging.hpp"
#include "../JSObjects/JSVec3.hpp"
#include "../JSObjects/JSQuaternion.hpp"


namespace Sirikata {
namespace JS {


//this constructor is called when the presence associated
JSPositionListener::JSPositionListener(JSObjectScript* script)
 :  jsObjScript(script),
    sporefToListenTo(NULL),
    sporefToListenFrom(NULL),
    hasRegisteredListener(false)
{
}


JSPositionListener::~JSPositionListener()
{
    if (hasRegisteredListener)
        deregisterAsPosListener();

    if (sporefToListenTo != NULL)
        delete sporefToListenTo;
    if (sporefToListenFrom != NULL)
        delete sporefToListenFrom;

}


void JSPositionListener::destroyed()
{
    if (hasRegisteredListener)
    {
        hasRegisteredListener = false;
        deregisterAsPosListener();
    }
}

void JSPositionListener::setListenTo(const SpaceObjectReference* objToListenTo,const SpaceObjectReference* objToListenFrom)
{
    //setting listenTo
    if (sporefToListenTo != NULL)
        *sporefToListenTo = *objToListenTo;
    else
    {
        if (objToListenTo != NULL)
            sporefToListenTo  =  new SpaceObjectReference(objToListenTo->space(),objToListenTo->object());
    }

    //setting listenFrom
    if (sporefToListenFrom != NULL)
        *sporefToListenFrom = *objToListenFrom;
    else
    {
        //may frequently get this parameter to be null from inheriting
        //JSPresenceStruct class.
        if (objToListenFrom != NULL)
            sporefToListenFrom =  new SpaceObjectReference(objToListenFrom->space(),objToListenFrom->object());
    }
}


bool JSPositionListener::registerAsPosListener()
{
    if (hasRegisteredListener)
        return true;

    //initializes mLocation and mOrientation to correct starting values.
    if (sporefToListenTo == NULL)
    {
        JSLOG(error,"error in JSPositionListener.  Requesting to register as pos listener to null sporef.  Doing nothing");
        return false;
    }

    hasRegisteredListener = jsObjScript->registerPosListener(sporefToListenTo,sporefToListenFrom,this,&mLocation,&mOrientation);

    return hasRegisteredListener;
}


v8::Handle<v8::Value> JSPositionListener::sendMessage (std::string& msgToSend)
{
    //actually send the message to the entity
    if ((sporefToListenFrom == NULL) || (*sporefToListenFrom == SpaceObjectReference::null()) ||
        (sporefToListenTo   == NULL) || (*sporefToListenTo   == SpaceObjectReference::null()))
        return v8::ThrowException( v8::Exception::Error(v8::String::New("Error trying to send message.  Do not have a presence associated with this object to send message out from, or do not have sporef to send to.")));

    jsObjScript->sendMessageToEntity(sporefToListenTo,sporefToListenFrom,msgToSend);
    return v8::Undefined();
}



void JSPositionListener::deregisterAsPosListener()
{
    if (!hasRegisteredListener)
        return;

    hasRegisteredListener =false;

    if (sporefToListenTo != NULL)
        jsObjScript->deRegisterPosListener(sporefToListenTo,sporefToListenFrom,this);
}


SpaceObjectReference* JSPositionListener::getToListenTo()
{
    return sporefToListenTo;
}

SpaceObjectReference* JSPositionListener::getToListenFrom()
{
    return sporefToListenFrom;
}

//calls updateLocation on jspos, filling in mLocation, mOrientation, and mBounds
//for the newLocation,newOrientation, and newBounds of updateLocation field.
void JSPositionListener::updateOtherJSPosListener(JSPositionListener* jspos)
{
    jspos->updateLocation(mLocation,mOrientation,mBounds);
}

//from being a position listener, must define what to do when receive an updated location.
void JSPositionListener::updateLocation (const TimedMotionVector3f &newLocation, const TimedMotionQuaternion& newOrient, const BoundingSphere3f& newBounds)
{
    mLocation    = newLocation;
    mOrientation = newOrient;
    mBounds      = newBounds;

    //if I received an updated location and I am associated with an object that
    //I am listeningFrom, then I should propagate this update to visible structs
    //that may not know about updates to this presence.
    if (sporefToListenFrom != NULL)
    {
        if (*sporefToListenFrom != SpaceObjectReference::null())
            jsObjScript->checkForwardUpdate(*sporefToListenFrom,newLocation,newOrient,newBounds);
    }

}

Vector3f JSPositionListener::getPosition()
{
    return mLocation.position(jsObjScript->getHostedTime());
}
Vector3f JSPositionListener::getVelocity()
{
    return mLocation.velocity();
}

Quaternion JSPositionListener::getOrientationVelocity()
{
    return mOrientation.velocity();
}

Quaternion JSPositionListener::getOrientation()
{
    return mOrientation.position(jsObjScript->getHostedTime()).normal();
}


BoundingSphere3f JSPositionListener::getBounds()
{
    return mBounds;
}

v8::Handle<v8::Value> JSPositionListener::struct_getPosition()
{
    String errorMsg;
    if (! passErrorChecks(errorMsg,"getPosition"))
        return v8::ThrowException(v8::Exception::Error(v8::String::New(errorMsg.c_str())));


    v8::Handle<v8::Context>curContext = v8::Context::GetCurrent();
    return CreateJSResult(curContext,getPosition());
}


v8::Handle<v8::Value>JSPositionListener::struct_getVelocity()
{
    String errorMsg;
    if (! passErrorChecks(errorMsg,"getVelocity"))
        return v8::ThrowException(v8::Exception::Error(v8::String::New(errorMsg.c_str())));


    v8::Handle<v8::Context> curContext = v8::Context::GetCurrent();
    return CreateJSResult(curContext,getVelocity());
}

v8::Handle<v8::Value> JSPositionListener::struct_getOrientationVel()
{
    String errorMsg;
    if (! passErrorChecks(errorMsg,"getOrientationVel"))
        return v8::ThrowException(v8::Exception::Error(v8::String::New(errorMsg.c_str())));

    v8::Handle<v8::Context> curContext = v8::Context::GetCurrent();
    return CreateJSResult(curContext,getOrientationVelocity());
}

v8::Handle<v8::Value> JSPositionListener::struct_getOrientation()
{
    String errorMsg;
    if (! passErrorChecks(errorMsg,"getOrientation"))
        return v8::ThrowException(v8::Exception::Error(v8::String::New(errorMsg.c_str())));

    v8::Handle<v8::Context>curContext = v8::Context::GetCurrent();
    return CreateJSResult(curContext,getOrientation());
}

v8::Handle<v8::Value> JSPositionListener::struct_getScale()
{
    String errorMsg;
    if (! passErrorChecks(errorMsg,"getScale"))
        return v8::ThrowException(v8::Exception::Error(v8::String::New(errorMsg.c_str())));

    v8::Handle<v8::Context>curContext = v8::Context::GetCurrent();
    return CreateJSResult(curContext,getBounds().radius());
}

v8::Handle<v8::Value> JSPositionListener::struct_getDistance(const Vector3d& distTo)
{
    String errorMsg;
    if (! passErrorChecks(errorMsg,"getOrientation"))
        return v8::ThrowException(v8::Exception::Error(v8::String::New(errorMsg.c_str())));

    Vector3d curPos = Vector3d(getPosition());
    double distVal = (distTo - curPos).length();

    v8::Handle<v8::Context>curContext = v8::Context::GetCurrent();
    return CreateJSResult(curContext,distVal);
}


bool JSPositionListener::passErrorChecks(String& errorMsg, const String& funcIn )
{
    if (sporefToListenTo == NULL)
    {
        errorMsg =  "Error when calling " + funcIn + ".  Did not specify who to listen to.";
        return false;
    }

    if (!v8::Context::InContext())
    {
        errorMsg = "Error when calling " + funcIn + ".  Not currently within a context.";
        return false;
    }

    return true;
}



} //namespace JS
} //namespace Sirikata
