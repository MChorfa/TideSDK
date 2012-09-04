/*
 * Copyright (c) 2009-2010 Appcelerator, Inc. All Rights Reserved.
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

#ifndef Monkey_h
#define Monkey_h

#include <vector>

#include <tide/tide.h>

namespace ti {

class Script {
public:
    bool Matches(std::string& url);
    static bool Matches(std::vector<std::string>&, std::string& url);
    static bool Matches(const char* pattern, const char* target);

    std::vector<std::string> includes;
    std::vector<std::string> excludes;
    std::string source;
};

class Monkey : public tide::StaticBoundObject {
public:
    Monkey(Host*, ObjectRef);

protected:
    virtual ~Monkey();
    void ParseFile(string filePath);
    void Callback(const ValueList &args, ValueRef result);
    void EvaluateUserScript(
        ObjectRef, std::string&,ObjectRef, std::string&);

    ObjectRef global;
    Logger* logger;
    MethodRef callback;
    std::vector<Script*> scripts;
};

} // namespace ti

#endif
