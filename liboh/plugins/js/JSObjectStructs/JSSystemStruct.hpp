#ifndef __SIRIKATA_JS_SYSTEM_STRUCT_HPP__
#define __SIRIKATA_JS_SYSTEM_STRUCT_HPP__

#include <sirikata/oh/HostedObject.hpp>
#include <v8.h>
#include "../JSPattern.hpp"
#include "../JSEntityCreateInfo.hpp"

namespace Sirikata {
namespace JS {

//need to forward-declare this so that can reference this inside
class JSObjectScript;
class JSContextStruct;
class JSEventHandlerStruct;
class JSPresenceStruct;



//Most calls in this class just go straight through into associated context to
//make a sibling call.  Split system into intermediate layer between v8-bound
//calls and jscontextstruct to make tracking of capabilities explicit, and easy
//to check without having to dig through a lot of other code.
struct JSSystemStruct
{
    JSSystemStruct(JSContextStruct* jscont, bool send, bool receive, bool prox,bool import,bool createPres, bool createEntity,bool eval);
    ~JSSystemStruct();

    static JSSystemStruct* decodeSystemStruct(v8::Handle<v8::Value> toDecode ,std::string& errorMessage);

    //regular members
    v8::Handle<v8::Value> struct_canSendMessage();
    v8::Handle<v8::Value> struct_canRecvMessage();
    v8::Handle<v8::Value> struct_canProx();
    v8::Handle<v8::Value> struct_canImport();
    
    v8::Handle<v8::Value> struct_getPosition();


    v8::Handle<v8::Value> struct_print(const String& msg);    
    v8::Handle<v8::Value> struct_sendHome(const String& toSend);
    v8::Handle<v8::Value> struct_import(const String& toImportFrom);
    v8::Handle<v8::Value> struct_require(const String& toRequireFrom);
    
    //if have the capability to create presences, create a new presence with
    //mesh newMesh and executes initFunc, which gets executed onConnected.
    //if do not have the capability, throws an error.
    v8::Handle<v8::Value> struct_createPresence(const String& newMesh, v8::Handle<v8::Function> initFunc);

    //if have the capability to create presences, create a new presence with
    //mesh newMesh and executes initFunc, which gets executed onConnected.
    //if do not have the capability, throws an error.
    v8::Handle<v8::Value> struct_createEntity(EntityCreateInfo& eci);
    
    v8::Handle<v8::Value> struct_makeEventHandlerObject(const PatternList& native_patterns,v8::Persistent<v8::Object> target_persist, v8::Persistent<v8::Function> cb_persist, v8::Persistent<v8::Object> sender_persist);

    v8::Handle<v8::Value> struct_createContext(SpaceObjectReference* canMessage, bool sendEveryone,bool recvEveryone,bool proxQueries,bool import,bool createPres,bool createEnt, bool evalable,JSPresenceStruct* presStruct);
    
    JSContextStruct* getContext();

    v8::Handle<v8::Value> struct_registerOnPresenceConnectedHandler(v8::Persistent<v8::Function> cb_persist);
    v8::Handle<v8::Value> struct_registerOnPresenceDisconnectedHandler(v8::Persistent<v8::Function> cb_persist);

    //calls eval on the system's context associated with this system.
    v8::Handle<v8::Value> struct_eval(const String& native_contents, ScriptOrigin* sOrigin);


    //create a timer that will fire in dur seconds from now, that will bind the
    //this parameter to target and that will fire the callback cb.
    v8::Handle<v8::Value> struct_createTimeout(const Duration& dur, v8::Persistent<v8::Function>& cb);

    v8::Handle<v8::Value> struct_setScript(const String& script);
    v8::Handle<v8::Value> struct_reset();

    
private:
    //associated data 
    JSContextStruct* associatedContext;
    bool canSend, canRecv, canProx,canImport,canCreatePres,canCreateEnt,canEval;
};


}//end namespace js
}//end namespace sirikata

#endif
