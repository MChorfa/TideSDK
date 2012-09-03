/*
 * Copyright (c) 2008-2010 Appcelerator, Inc. All Rights Reserved.
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

#ifndef Network_h
#define Network_h

#include <string>
#include <vector>

#include <tide/tide.h>

namespace ti {

class NetworkStatus;
class Host;

class Network : public KAccessorObject {
public:
    Network();
    virtual ~Network();

    static const std::string& GetFirstIPAddress();

private:
    AutoPtr<Host> GetHostBinding(const std::string& host);

    void _GetFirstMACAddress(const ValueList& args, ValueRef result);
    void _GetFirstIPAddress(const ValueList& args, ValueRef result);
    void _GetInterfaces(const ValueList& args, ValueRef result);
    void _CreateIPAddress(const ValueList& args, ValueRef result);
    void _CreateTCPSocket(const ValueList& args, ValueRef result);
    void _CreateTCPServerSocket(const ValueList& args, ValueRef result);
    void _CreateIRCClient(const ValueList& args, ValueRef result);
    void _CreateHTTPClient(const ValueList& args, ValueRef result);
    void _CreateHTTPServer(const ValueList& args, ValueRef result);
    void _CreateHTTPCookie(const ValueList& args, ValueRef result);
    void _EncodeURIComponent(const ValueList &args, ValueRef result);
    void _DecodeURIComponent(const ValueList &args, ValueRef result);
    void _GetHostByName(const ValueList& args, ValueRef result);
    void _GetHostByAddress(const ValueList& args, ValueRef result);
    void _AddConnectivityListener(const ValueList& args, ValueRef result);
    void _RemoveConnectivityListener(const ValueList& args, ValueRef result);
    void _SetHTTPProxy(const ValueList& args, ValueRef result);
    void _SetHTTPSProxy(const ValueList& args, ValueRef result);
    void _GetHTTPProxy(const ValueList& args, ValueRef result);
    void _GetHTTPSProxy(const ValueList& args, ValueRef result);

    KObjectRef global;
};

} // namespace ti

#endif
