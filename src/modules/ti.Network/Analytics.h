/*
 * Copyright (c) 2010 Appcelerator, Inc. All Rights Reserved.
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

#ifndef Analytics_h
#define Analytics_h

#include <queue>

#include <tide/tide.h>
#include <Poco/Condition.h>
#include <Poco/Mutex.h>
#include <Poco/Runnable.h>
#include <Poco/Thread.h>

#include <curl/curl.h>

namespace ti {

class Analytics : public KEventObject, public Poco::Runnable {
public:
    Analytics();
    ~Analytics();
    void Shutdown();

private:
    bool running;
    std::string url;
    std::string baseData;
    CURL* curlHandle;
    Poco::Thread thread;
    std::queue<std::string> events;
    Poco::Mutex eventsLock;
    MethodRef startCallback;

    void run();
    void SendEventToAPIServer(std::string& eventData);
    void _SendEvent(const ValueList& args, ValueRef result);
    void _StartAnalyticsThread(const ValueList &args, ValueRef result);
};

} // namespace ti

#endif
