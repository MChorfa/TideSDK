/**
 * Appcelerator Kroll - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2008 Appcelerator, Inc. All Rights Reserved.
 */

#include "javascript_module.h"

#include <Poco/FileStream.h>
#include <Poco/Mutex.h>

namespace tide
{
namespace JSUtil
{
	static inline Logger* GetLogger()
	{
		static Logger* logger = 0;
		if (!logger)
			logger = Logger::Get("JavaScript.JSUtil");
		return logger;
	}

	static JSClassRef JSObjectClass = NULL;
	static JSClassRef JSMethodClass = NULL;
	static JSClassRef JSListClass = NULL;
	static const JSClassDefinition emptyClassDefinition =
	{
		0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	};

	// These are all JS*Class class methods.
	static void GetPropertyNamesCallback(JSContextRef, JSObjectRef, JSPropertyNameAccumulatorRef);
	static bool HasPropertyCallback(JSContextRef, JSObjectRef, JSStringRef);
	static JSValueRef GetPropertyCallback(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);
	static bool SetPropertyCallback(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSValueRef*);
	static JSValueRef CallAsFunctionCallback(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
	static void FinalizeCallback(JSObjectRef);
	static JSValueRef ToStringCallback(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
	static JSValueRef EqualsCallback(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);

	// Private functions
	static void AddSpecialPropertyNames(ValueRef, SharedStringList, bool);
	static JSValueRef GetSpecialProperty(ValueRef, const char*, JSContextRef, ValueRef);
	static bool DoSpecialSetBehavior(ValueRef target, const char* name, ValueRef newValue);
	static JSValueRef GetFunctionPrototype(JSContextRef jsContext, JSValueRef* exception);
	static JSValueRef GetArrayPrototype(JSContextRef jsContext, JSValueRef* exception);

	ValueRef ToTideValue(JSValueRef value, JSContextRef jsContext,
		JSObjectRef thisObject)
	{
		ValueRef tideValue = 0;
		JSValueRef exception = NULL;

		if (value == NULL)
		{
			GetLogger()->Error("Trying to convert NULL JSValueRef");
			return Value::Undefined;
		}

		if (JSValueIsNumber(jsContext, value))
		{
			tideValue = Value::NewDouble(JSValueToNumber(jsContext, value, &exception));
		}
		else if (JSValueIsBoolean(jsContext, value))
		{
			tideValue = Value::NewBool(JSValueToBoolean(jsContext, value));
		}
		else if (JSValueIsString(jsContext, value))
		{
			JSStringRef jsString = JSValueToStringCopy(jsContext, value, &exception);
			if (jsString)
			{
				std::string stringValue(ToChars(jsString));
				JSStringRelease(jsString);
				tideValue = Value::NewString(stringValue);
			}
		}
		else if (JSValueIsObject(jsContext, value))
		{
			JSObjectRef o = JSValueToObject(jsContext, value, &exception);
			if (o != NULL)
			{
				ValueRef* value = static_cast<ValueRef*>(JSObjectGetPrivate(o));
				if (value != NULL)
				{
					// This is a KJS-wrapped Tide value: unwrap it
					return *value;
				}
				else if (JSObjectIsFunction(jsContext, o))
				{
					// this is a pure JS method: proxy it
					KMethodRef tibm = new KKJSMethod(jsContext, o, thisObject);
					tideValue = Value::NewMethod(tibm);
				}
				else if (IsArrayLike(o, jsContext))
				{
					// this is a pure JS array: proxy it
					KListRef tibl = new KKJSList(jsContext, o);
					tideValue = Value::NewList(tibl);
				}
				else
				{
					// this is a pure JS object: proxy it
					KObjectRef tibo = new KKJSObject(jsContext, o);
					tideValue = Value::NewObject(tibo);
				}
			}
		}
		else if (JSValueIsNull(jsContext, value))
		{
			tideValue = tide::Value::Null;
		}
		else
		{
			tideValue = tide::Value::Undefined;
		}
		if (!tideValue.isNull() && exception == NULL)
		{
			return tideValue;
		}
		else if (exception != NULL)
		{
			throw ToTideValue(exception, jsContext, NULL);
		}
		else
		{
			GetLogger()->Error("Failed Tide->JS conversion with no exception!");
			throw ValueException::FromString("Conversion from Tide value to JS value failed");
		}
	}

	JSValueRef ToJSValue(ValueRef value, JSContextRef jsContext)
	{
		JSValueRef jsValue = NULL;
		if (value->IsInt())
		{
			jsValue = JSValueMakeNumber(jsContext, value->ToInt());
		}
		else if (value->IsDouble())
		{
			jsValue = JSValueMakeNumber(jsContext, value->ToDouble());
		}
		else if (value->IsBool())
		{
			jsValue = JSValueMakeBoolean(jsContext, value->ToBool());
		}
		else if (value->IsString())
		{
			JSStringRef s = JSStringCreateWithUTF8CString(value->ToString());
			jsValue = JSValueMakeString(jsContext, s);
			JSStringRelease(s);
		}
		else if (value->IsObject())
		{
			KObjectRef obj = value->ToObject();
			AutoPtr<KKJSObject> kobj = obj.cast<KKJSObject>();
			if (!kobj.isNull() && kobj->SameContextGroup(jsContext))
			{
				// this object is actually a pure JS object
				jsValue = kobj->GetJSObject();
			}
			else
			{
				// this is a Object that needs to be proxied
				jsValue = ObjectToJSValue(value, jsContext);
			}
		}
		else if (value->IsMethod())
		{
			KMethodRef meth = value->ToMethod();
			AutoPtr<KKJSMethod> kmeth = meth.cast<KKJSMethod>();
			if (!kmeth.isNull() && kmeth->SameContextGroup(jsContext))
			{
				// this object is actually a pure JS callable object
				jsValue = kmeth->GetJSObject();
			}
			else
			{
				// this is a KMethod that needs to be proxied
				jsValue = MethodToJSValue(value, jsContext);
			}
		}
		else if (value->IsList())
		{
			KListRef list = value->ToList();
			AutoPtr<KKJSList> klist = list.cast<KKJSList>();
			if (!klist.isNull() && klist->SameContextGroup(jsContext))
			{
				// this object is actually a pure JS array
				jsValue = klist->GetJSObject();
			}
			else
			{
				// this is a KList that needs to be proxied
				jsValue = ListToJSValue(value, jsContext);
			}
		}
		else if (value->IsNull())
		{
			jsValue = JSValueMakeNull(jsContext);
		}
		else if (value->IsUndefined())
		{
			jsValue = JSValueMakeUndefined(jsContext);
		}
		else
		{
			jsValue = JSValueMakeUndefined(jsContext);
		}

		return jsValue;

	}

	JSValueRef ObjectToJSValue(ValueRef objectValue, JSContextRef jsContext)
	{
		if (JSObjectClass == NULL)
		{
			JSClassDefinition jsClassDefinition = emptyClassDefinition;
			jsClassDefinition.className = "Object";
			jsClassDefinition.getPropertyNames = GetPropertyNamesCallback;
			jsClassDefinition.finalize = FinalizeCallback;
			jsClassDefinition.hasProperty = HasPropertyCallback;
			jsClassDefinition.getProperty = GetPropertyCallback;
			jsClassDefinition.setProperty = SetPropertyCallback;
			JSObjectClass = JSClassCreate(&jsClassDefinition);
		}
		return JSObjectMake(jsContext, JSObjectClass, new ValueRef(objectValue));
	}

	JSValueRef MethodToJSValue(ValueRef methodValue, JSContextRef jsContext)
	{
		if (JSMethodClass == NULL)
		{
			JSClassDefinition jsClassDefinition = emptyClassDefinition;
			jsClassDefinition.className = "Function";
			jsClassDefinition.getPropertyNames = GetPropertyNamesCallback;
			jsClassDefinition.finalize = FinalizeCallback;
			jsClassDefinition.hasProperty = HasPropertyCallback;
			jsClassDefinition.getProperty = GetPropertyCallback;
			jsClassDefinition.setProperty = SetPropertyCallback;
			jsClassDefinition.callAsFunction = CallAsFunctionCallback;
			JSMethodClass = JSClassCreate(&jsClassDefinition);
		}
		JSObjectRef jsobject = JSObjectMake(jsContext, JSMethodClass, new ValueRef(methodValue));
		JSValueRef functionPrototype = GetFunctionPrototype(jsContext, NULL);
		JSObjectSetPrototype(jsContext, jsobject, functionPrototype);
		return jsobject;
	}

	JSValueRef ListToJSValue(ValueRef listValue, JSContextRef jsContext)
	{

		if (JSListClass == NULL)
		{
			JSClassDefinition jsClassDefinition = emptyClassDefinition;
			jsClassDefinition.className = "Array";
			jsClassDefinition.getPropertyNames = GetPropertyNamesCallback;
			jsClassDefinition.finalize = FinalizeCallback;
			jsClassDefinition.hasProperty = HasPropertyCallback;
			jsClassDefinition.getProperty = GetPropertyCallback;
			jsClassDefinition.setProperty = SetPropertyCallback;
			JSListClass = JSClassCreate(&jsClassDefinition);
		}

		JSObjectRef jsobject = JSObjectMake(jsContext, JSListClass, new ValueRef(listValue));
		JSValueRef arrayPrototype = GetArrayPrototype(jsContext, NULL);
		JSObjectSetPrototype(jsContext, jsobject, arrayPrototype);
		return jsobject;
	}

	std::string ToChars(JSStringRef jsString)
	{
		size_t size = JSStringGetMaximumUTF8CStringSize(jsString);
		char* cstring = (char*) malloc(size);
		JSStringGetUTF8CString(jsString, cstring, size);
		std::string string(cstring);
		free(cstring);
		return string;
	}

	bool IsArrayLike(JSObjectRef object, JSContextRef jsContext)
	{
		bool isArrayLike = true;

		JSStringRef pop = JSStringCreateWithUTF8CString("pop");
		isArrayLike = isArrayLike && JSObjectHasProperty(jsContext, object, pop);
		JSStringRelease(pop);

		JSStringRef concat = JSStringCreateWithUTF8CString("concat");
		isArrayLike = isArrayLike && JSObjectHasProperty(jsContext, object, concat);
		JSStringRelease(concat);

		JSStringRef length = JSStringCreateWithUTF8CString("length");
		isArrayLike = isArrayLike && JSObjectHasProperty(jsContext, object, length);
		JSStringRelease(length);

		return isArrayLike;
	}

	static void FinalizeCallback(JSObjectRef jsObject)
	{
		ValueRef* value = static_cast<ValueRef*>(JSObjectGetPrivate(jsObject));
		delete value;
	}

	static bool PrototypeHasFunctionNamed(JSContextRef jsContext, JSObjectRef object,
		JSStringRef name)
	{
		JSValueRef exception = NULL;

		JSValueRef prototypeValue = JSObjectGetPrototype(jsContext, object);
		JSObjectRef prototype = JSValueToObject(jsContext, prototypeValue, &exception);

		if (exception)
			return false;

		JSValueRef propValue = JSObjectGetProperty(jsContext, prototype, name, &exception);
		if (exception)
			return false;

		if (!JSValueIsObject(jsContext, propValue))
			return false;

		JSObjectRef prop = JSValueToObject(jsContext, propValue, &exception);
		return !exception && JSObjectIsFunction(jsContext, prop);
	}

	static bool HasPropertyCallback(JSContextRef jsContext, JSObjectRef jsObject,
		JSStringRef jsProperty)
	{
		ValueRef* value = static_cast<ValueRef*>(JSObjectGetPrivate(jsObject));
		if (value == NULL)
			return false;

		// Convert the name to a std::string.
		KObjectRef object = (*value)->ToObject();
		std::string name(ToChars(jsProperty));

		// Special properties always take precedence. This is important
		// because even though the Array and Function prototypes have 
		// methods like toString -- we always want our special properties
		// to override those.
		SharedStringList specialProperties(new StringList());
		AddSpecialPropertyNames(*value, specialProperties, true);
		for (size_t i = 0; i < specialProperties->size(); i++)
		{
			if (name == *specialProperties->at(i))
				return true;
		}

		// If the JavaScript prototype for Lists (Array) or Methods (Function) has
		// a method with the same name -- opt to use the prototype's version instead.
		// This will prevent  incompatible versions of things like pop() bleeding into
		// JavaScript.
		if (((*value)->IsList() || (*value)->IsMethod()) &&
			PrototypeHasFunctionNamed(jsContext, jsObject, jsProperty))
		{
			return false;
		}

		return object->HasProperty(name.c_str());
	}

	static JSValueRef GetPropertyCallback(JSContextRef jsContext, 
		JSObjectRef jsObject, JSStringRef jsProperty, JSValueRef* jsException)
	{

		ValueRef* value = static_cast<ValueRef*>(JSObjectGetPrivate(jsObject));
		if (value == NULL)
			return JSValueMakeUndefined(jsContext);

		KObjectRef object = (*value)->ToObject();
		std::string name(ToChars(jsProperty));
		JSValueRef jsValue = NULL;
		try
		{
			ValueRef kvalue = object->Get(name.c_str());
			jsValue = GetSpecialProperty(*value, name.c_str(), jsContext, kvalue);
		}
		catch (ValueException& exception)
		{
			*jsException = ToJSValue(exception.GetValue(), jsContext);
		}
		catch (std::exception &e)
		{
			ValueRef v = Value::NewString(e.what());
			*jsException = ToJSValue(v, jsContext);
		}
		catch (...)
		{
			std::string error = "Unknown exception trying to get property: ";
			error.append(name);
			ValueRef v = Value::NewString(error);
			*jsException = ToJSValue(v, jsContext);
		}

		return jsValue;
	}

	static bool SetPropertyCallback(JSContextRef jsContext, JSObjectRef jsObject,
		JSStringRef jsProperty, JSValueRef jsValue, JSValueRef* jsException)
	{
		ValueRef* value = static_cast<ValueRef*>(JSObjectGetPrivate(jsObject));
		if (value == NULL)
			return false;

		KObjectRef object = (*value)->ToObject();
		bool success = false;
		std::string propertyName(ToChars(jsProperty));
		try
		{
			ValueRef newValue = ToTideValue(jsValue, jsContext, jsObject);

			// Arrays in particular have a special behavior when
			// you do something like set the "length" property
			if (!DoSpecialSetBehavior(*value, propertyName.c_str(), newValue))
			{
				object->Set(propertyName.c_str(), newValue);
			}
			success = true;
		}
		catch (ValueException& exception)
		{
			*jsException = ToJSValue(exception.GetValue(), jsContext);
		}
		catch (std::exception &e)
		{
			ValueRef v = Value::NewString(e.what());
			*jsException = ToJSValue(v, jsContext);
		}
		catch (...)
		{
			std::string error = "Unknown exception trying to set property: ";
			error.append(propertyName);
			ValueRef v = Value::NewString(error);
			*jsException = ToJSValue(v, jsContext);
		}

		return success;
	}

	static JSValueRef CallAsFunctionCallback(JSContextRef jsContext,
		JSObjectRef jsFunction, JSObjectRef jsThis, size_t argCount,
		const JSValueRef jsArgs[], JSValueRef* jsException)
	{
		ValueRef* value = static_cast<ValueRef*>(JSObjectGetPrivate(jsFunction));
		if (value == NULL)
			return JSValueMakeUndefined(jsContext);

		KMethodRef method = (*value)->ToMethod();
		ValueList args;
		for (size_t i = 0; i < argCount; i++)
		{
			ValueRef argVal = ToTideValue(jsArgs[i], jsContext, jsThis);
			Value::Unwrap(argVal);
			args.push_back(argVal);
		}

		JSValueRef jsValue = NULL;
		try
		{
			ValueRef kvalue = method->Call(args);
			jsValue = ToJSValue(kvalue, jsContext);
		}
		catch (ValueException& exception)
		 {
			SharedString str = exception.DisplayString();
			*jsException = ToJSValue(exception.GetValue(), jsContext);
		} 
		catch (std::exception &e)
		{
			ValueRef v = Value::NewString(e.what());
			*jsException = ToJSValue(v, jsContext);
		}
		catch (...)
		{
			ValueRef v = Value::NewString("Unknown exception during Tide method call");
			*jsException = ToJSValue(v, jsContext);
		}

		return jsValue;
	}

	static void AddSpecialPropertyNames(ValueRef value, SharedStringList props,
		bool showInvisible)
	{
		// Some attributes should be hidden unless the are requested specifically -- 
		// essentially a has_property(...) versus  get_property_list(...). An example
		// of this type of attribute is toString(). Some JavaScript code might expect
		// a "hash" object to have no methods in its property list. We don't want
		// toString() to show up in those situations.

		bool foundLength = false, foundToString = false, foundEquals = false;
		for (size_t i = 0; i < props->size(); i++)
		{
			SharedString propertyName = props->at(i);
			if (strcmp(propertyName->c_str(), "length") == 0)
				foundLength = true;
			if (strcmp(propertyName->c_str(), "toString") == 0)
				foundToString = true;
			if (strcmp(propertyName->c_str(), "equals") == 0)
				foundEquals = true;
		}

		if (!foundLength && value->IsList())
		{
			props->push_back(new std::string("length"));
		}

		if (!foundToString && showInvisible)
		{
			props->push_back(new std::string("toString"));
		}

		if (!foundEquals && showInvisible)
		{
			props->push_back(new std::string("equals"));
		}
	}

	static JSValueRef GetSpecialProperty(ValueRef value, const char* name, 
		JSContextRef jsContext, ValueRef objValue)
	{
		// Always override the length property on lists. Some languages
		// supply their own length property, which might be a method instead
		// of a number -- bad news.
		if (value->IsList() && !strcmp(name, "length"))
		{
			KListRef l = value->ToList();
			return JSValueMakeNumber(jsContext, l->Size());
		}

		// Only overload these methods if the value in our object is not a
		// method We want the user to be able to supply their own versions,
		// but we don't want JavaScript code to freak out in situations where
		// Tide objects use attributes with the same name that aren't methods.
		if (!objValue->IsMethod())
		{
			if (!strcmp(name, "toString"))
			{
				JSStringRef s = JSStringCreateWithUTF8CString("toString");
				JSValueRef toString = JSObjectMakeFunctionWithCallback(
					jsContext, s, &ToStringCallback);
				JSStringRelease(s);
				return toString;
			}

			if (!strcmp(name, "equals"))
			{
				JSStringRef s = JSStringCreateWithUTF8CString("equals");
				JSValueRef equals = JSObjectMakeFunctionWithCallback(
					jsContext, s, &EqualsCallback);
				JSStringRelease(s);
				return equals;
			}
		}

		// Otherwise this is just a normal JS value
		return ToJSValue(objValue, jsContext);
	}

	static bool DoSpecialSetBehavior(ValueRef target, const char* name, ValueRef newValue)
	{
		// We only do something special if we are trying to set
		// the length property of a list to a new int value.
		if (strcmp(name, "length") || !target->IsList() || !newValue->IsInt())
		{
			return false;
		}
		target->ToList()->ResizeTo(newValue->ToInt());
		return true;
	}

	static JSValueRef ToStringCallback(JSContextRef jsContext,
		JSObjectRef jsFunction, JSObjectRef jsThis, size_t argCount,
		const JSValueRef args[], JSValueRef* exception)
	{
		ValueRef* value = static_cast<ValueRef*>(JSObjectGetPrivate(jsThis));
		if (value == NULL)
			return JSValueMakeUndefined(jsContext);

		SharedString ss = (*value)->DisplayString(2);
		ValueRef dsv = Value::NewString(ss);
		return ToJSValue(dsv, jsContext);
	}

	static JSValueRef EqualsCallback(JSContextRef jsContext, JSObjectRef function,
		JSObjectRef jsThis, size_t numArgs, const JSValueRef args[],
		JSValueRef* exception)
	{
		ValueRef* value = static_cast<ValueRef*>(JSObjectGetPrivate(jsThis));
		if (value == NULL || numArgs < 1)
		{
			return JSValueMakeBoolean(jsContext, false);
		}

		// Ensure argument is a JavaScript object
		if (!JSValueIsObject(jsContext, args[0]))
		{
			return JSValueMakeBoolean(jsContext, false);
		}

		// Ensure argument is a Tide JavaScript
		JSObjectRef otherObject = JSValueToObject(jsContext, args[0], NULL);
		ValueRef* otherValue = static_cast<ValueRef*>(JSObjectGetPrivate(otherObject));
		if (otherValue == NULL)
		{
			return JSValueMakeBoolean(jsContext, false);
		}

		// Test equality
		return JSValueMakeBoolean(jsContext, (*value)->Equals(*otherValue));
	}

	static void GetPropertyNamesCallback(JSContextRef jsContext,
		JSObjectRef jsObject, JSPropertyNameAccumulatorRef jsProperties)
	{
		ValueRef* value = static_cast<ValueRef*>(JSObjectGetPrivate(jsObject));
		if (value == NULL)
			return;

		KObjectRef object = (*value)->ToObject();
		SharedStringList props = object->GetPropertyNames();
		AddSpecialPropertyNames(*value, props, false);
		for (size_t i = 0; i < props->size(); i++)
		{
			SharedString propertyName = props->at(i);
			JSStringRef name = JSStringCreateWithUTF8CString(propertyName->c_str());
			JSPropertyNameAccumulatorAddName(jsProperties, name);
			JSStringRelease(name);
		}
	}

	JSGlobalContextRef CreateGlobalContext()
	{
		JSGlobalContextRef jsContext = JSGlobalContextCreate(0);
		JSObjectRef globalObject = JSContextGetGlobalObject(jsContext);

		JSValueRef jsAPI = ToJSValue(
			Value::NewObject(GlobalObject::GetInstance()), jsContext);
		JSStringRef propertyName = JSStringCreateWithUTF8CString(PRODUCT_NAME);
		JSObjectSetProperty(jsContext, globalObject, propertyName,
			jsAPI, kJSPropertyAttributeNone, NULL);
		JSStringRelease(propertyName);

		RegisterGlobalContext(globalObject, jsContext);
		return jsContext;
	}

	static std::map<JSObjectRef, JSGlobalContextRef> jsContextMap;
	Poco::Mutex jsContextMapMutex;
	void RegisterGlobalContext(JSObjectRef object, JSGlobalContextRef globalContext)
	{
		Poco::Mutex::ScopedLock lock(jsContextMapMutex);
		jsContextMap[object] = globalContext;
	}

	void UnregisterGlobalContext(JSGlobalContextRef jsContext)
	{
		JSObjectRef globalObject(JSContextGetGlobalObject(jsContext));
		Poco::Mutex::ScopedLock lock(jsContextMapMutex);
		std::map<JSObjectRef, JSGlobalContextRef>::iterator i = jsContextMap.find(globalObject);
		if (i != jsContextMap.end())
		{
			jsContextMap.erase(i);
		}
	}
	

	JSGlobalContextRef GetGlobalContext(JSObjectRef object)
	{
		Poco::Mutex::ScopedLock lock(jsContextMapMutex);
		if (jsContextMap.find(object) == jsContextMap.end())
		{
			return NULL;
		}
		else
		{
			return jsContextMap[object];
		}
	}

	static std::map<JSGlobalContextRef, int> jsContextRefCounts;
	Poco::Mutex jsContextRefCountsMutex;
	void ProtectGlobalContext(JSGlobalContextRef globalContext)
	{
		Poco::Mutex::ScopedLock lock(jsContextRefCountsMutex);
		if (jsContextRefCounts.find(globalContext) == jsContextRefCounts.end())
		{
			JSGlobalContextRetain(globalContext);
			jsContextRefCounts[globalContext] = 1;
		}
		else
		{
			jsContextRefCounts[globalContext]++;
		}

		std::map<JSGlobalContextRef, int>::iterator i = jsContextRefCounts.begin();
		while (i != jsContextRefCounts.end())
		{
			std::map<JSGlobalContextRef, int>::iterator toRelease = i++;
			if (toRelease->second <= 0)
			{
				JSGlobalContextRelease(toRelease->first);
				jsContextRefCounts.erase(toRelease);
			}
		}
	}

	void UnprotectGlobalContext(JSGlobalContextRef globalContext)
	{
		Poco::Mutex::ScopedLock lock(jsContextRefCountsMutex);
		std::map<JSGlobalContextRef, int>::iterator i
			= jsContextRefCounts.find(globalContext);

		if (i == jsContextRefCounts.end())
		{
			GetLogger()->Error("Tried to unprotect an unknown jsContext!");
			return;
		}

		// Defer the release of this global context until the next protection.
		// This function may be called from JavaScript garbage collection. Unprotecting
		// the contet here might spawn another garbage collection causing a segfault.
		jsContextRefCounts[globalContext]--;
	}

	ValueRef Evaluate(JSContextRef jsContext, const char* script, const char* url)
	{
		JSObjectRef globalObject = JSContextGetGlobalObject(jsContext);
		JSStringRef scriptContents = JSStringCreateWithUTF8CString(script);
		JSStringRef jsURL = JSStringCreateWithUTF8CString(url);
		JSValueRef exception = NULL;

		JSValueRef returnValue = JSEvaluateScript(jsContext, scriptContents, globalObject, 
			jsURL, 0, &exception);

		JSStringRelease(jsURL);
		JSStringRelease(scriptContents);

		if (exception)
			throw ValueException(ToTideValue(exception, jsContext, NULL));

		return ToTideValue(returnValue, jsContext, globalObject);
	}

	ValueRef EvaluateFile(JSContextRef jsContext, const std::string& fullPath)
	{
		GetLogger()->Debug("Evaluating JavaScript file at: %s", fullPath.c_str());
		std::string scriptContents(FileUtils::ReadFile(fullPath));
		std::string fileURL(URLUtils::PathToFileURL(fullPath));
		return Evaluate(jsContext, scriptContents.c_str(), fileURL.c_str());
	}

	//===========================================================================//
	// METHODS BORROWED ARE TAKEN FROM GWT - modifications under same license
	//===========================================================================//
	/*
	 * Copyright 2008 Google Inc.
	 * 
	 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
	 * use this file except in compliance with the License. You may obtain a copy of
	 * the License at
	 * 
	 * http://www.apache.org/licenses/LICENSE-2.0
	 * 
	 * Unless required by applicable law or agreed to in writing, software
	 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
	 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
	 * License for the specific language governing permissions and limitations under
	 * the License.
	 */
	
	/*
	 * The following takes the prototype from the Function constructor, this allows
	 * us to easily support call and apply on our objects that support CallAsFunction.
	 *
	 * NOTE: The return value is not protected.
	 */
	static JSValueRef GetFunctionPrototype(JSContextRef jsContext, JSValueRef* exception) 
	{
		JSObjectRef globalObject = JSContextGetGlobalObject(jsContext);
		JSStringRef fnPropName = JSStringCreateWithUTF8CString("Function");
		JSValueRef fnCtorValue = JSObjectGetProperty(jsContext, globalObject,
			fnPropName, exception);
		JSStringRelease(fnPropName);
		if (!fnCtorValue)
		{
			return JSValueMakeUndefined(jsContext);
		}

		JSObjectRef fnCtorObject = JSValueToObject(jsContext, fnCtorValue, exception);
		if (!fnCtorObject)
		{
			return JSValueMakeUndefined(jsContext);
		}

		JSStringRef protoPropName = JSStringCreateWithUTF8CString("prototype");
		JSValueRef fnPrototype = JSObjectGetProperty(jsContext, fnCtorObject,
			protoPropName, exception);
		JSStringRelease(protoPropName);
		if (!fnPrototype)
		{
			return JSValueMakeUndefined(jsContext);
		}

	return fnPrototype;
	}

	/*
	 * The following takes the prototype from the Array constructor, this allows
	 * us to easily support array-like functions
	 *
	 * NOTE: The return value is not protected.
	 */
	static JSValueRef GetArrayPrototype(JSContextRef jsContext, JSValueRef* exception) 
	{
		JSObjectRef globalObject = JSContextGetGlobalObject(jsContext);
		JSStringRef fnPropName = JSStringCreateWithUTF8CString("Array");
		JSValueRef fnCtorValue = JSObjectGetProperty(jsContext, globalObject,
			fnPropName, exception);
		JSStringRelease(fnPropName);
		if (!fnCtorValue) 
		{
			return JSValueMakeUndefined(jsContext);
		}

		JSObjectRef fnCtorObject = JSValueToObject(jsContext, fnCtorValue, exception);
		if (!fnCtorObject)
		{
			return JSValueMakeUndefined(jsContext);
		}

		JSStringRef protoPropName = JSStringCreateWithUTF8CString("prototype");
		JSValueRef fnPrototype = JSObjectGetProperty(jsContext, fnCtorObject,
			protoPropName, exception);
		JSStringRelease(protoPropName);
		if (!fnPrototype) 
		{
			return JSValueMakeUndefined(jsContext);
		}

		return fnPrototype;
	}

	ValueRef GetProperty(JSObjectRef globalObject, std::string name)
	{
		JSGlobalContextRef jsContext = GetGlobalContext(globalObject);
		JSStringRef jsName = JSStringCreateWithUTF8CString(name.c_str());
		JSValueRef prop = JSObjectGetProperty(jsContext, globalObject, jsName, NULL);
		JSStringRelease(jsName);
		return ToTideValue(prop, jsContext, globalObject);
	}
}
}
