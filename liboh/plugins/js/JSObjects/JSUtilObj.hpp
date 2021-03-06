#ifndef __SIRIKATA_JS_UTILOBJ_HPP__
#define __SIRIKATA_JS_UTILOBJ_HPP__

#include "../JSUtil.hpp"
#include "../JSObjectScript.hpp"
#include <v8.h>

namespace Sirikata {
namespace JS {
namespace JSUtilObj{

v8::Handle<v8::Value> ScriptCreateWhen(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptCreateWhenTimeoutLT(const v8::Arguments& args);

v8::Handle<v8::Value> ScriptSqrtFunction(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptAcosFunction(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptAsinFunction(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptSinFunction(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptCosFunction(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptRandFunction(const v8::Arguments& args);

v8::Handle<v8::Value> ScriptPowFunction(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptExpFunction(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptAbsFunction(const v8::Arguments& args);


v8::Handle<v8::Value> ScriptCreateQuotedObject(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptCreateWhenWatchedItem(const v8::Arguments& args);
v8::Handle<v8::Value> ScriptCreateWhenWatchedList(const v8::Arguments& args);


} //jsutilobj
} //js
} //sirikata

#endif
