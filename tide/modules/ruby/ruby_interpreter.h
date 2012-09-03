/**
 * Appcelerator Kroll - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2008 Appcelerator, Inc. All Rights Reserved.
 */

#ifndef _RUBY_INTERPRETER_H_
#define _RUBY_INTERPRETER_H_

#include <tide/Interpreter.h>

namespace tide {

class RubyInterpreter : public Interpreter {
public:
	RubyInterpreter();

    ValueRef EvaluateFile(const char* filepath, KObjectRef context);

private:
	std::string GetContextId(KObjectRef global);
	VALUE GetContext(KObjectRef global);
	void ContextToGlobal(VALUE ctx, KObjectRef o);
};

} // namespace tide

#endif
