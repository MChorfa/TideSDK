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

#include "CodecModule.h"

#include "Codec.h"

namespace ti {

TIDE_MODULE(CodecModule, STRING(MODULE_NAME), STRING(MODULE_VERSION));

void CodecModule::Initialize()
{
    // load our variables
    this->binding = new Codec(host->GetGlobalObject());

    // set our ti.Codec
    KValueRef value = Value::NewObject(this->binding);
    host->GetGlobalObject()->Set("Codec", value);
}

void CodecModule::Stop()
{
}

} // namespace ti
