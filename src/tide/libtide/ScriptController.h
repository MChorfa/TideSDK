/*
 * Copyright (c) 2011 Appcelerator, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ScriptController_h
#define ScriptController_h

#include <map>

#include "tide.h"

namespace tide {

class Interpreter;

class TIDE_API ScriptController {
public:
    ScriptController();

    void AddInterpreter(Interpreter* interpreter, const char* supportedScriptTypes[]);
    void RemoveInterpreter(Interpreter* interpreter);

    ValueRef EvaluateFile(const char* filepath, ObjectRef context);

private:
    Interpreter* findInterpreterForType(const char* scriptType);

    typedef std::map<std::string, Interpreter*> InterpreterMapping;
    InterpreterMapping interpreters;
};

} // namespace tide

#endif
