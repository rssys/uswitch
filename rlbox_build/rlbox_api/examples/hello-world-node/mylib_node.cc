#include <node.h>
#include "mylib.h"
#include "RLBox_DynLib.h"
#include "rlbox.h"

using namespace rlbox;

namespace mylib_node {

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Exception;
using v8::Uint32;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Value;

RLBoxSandbox<RLBox_DynLib>* sandbox;

void Hello(const FunctionCallbackInfo<Value>& args) {
  sandbox_invoke(sandbox, hello);
}


void Echo(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  if (args.Length() != 1 || !args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate,
              "Call echo with a string",
              NewStringType::kNormal).ToLocalChecked()));
    return;
  }
  v8::String::Utf8Value v8str(isolate, args[0].As<String>());
  sandbox_invoke(sandbox, echo, sandbox->stackarr(*v8str));
}

void Add(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  if (args.Length() != 2 || !args[0]->IsUint32() || !args[1]->IsUint32()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate,
              "Call add with two integers please",
              NewStringType::kNormal).ToLocalChecked()));
    return;
  }

  unsigned arg0 = args[0].As<Uint32>()->Value();
  unsigned arg1 = args[1].As<Uint32>()->Value();

  unsigned ret = sandbox_invoke(sandbox, add, arg0, arg1).copyAndVerify([](unsigned ret){
      return ret;
  });
  Local<Uint32> num = Uint32::NewFromUnsigned(isolate, ret)->ToUint32();
  args.GetReturnValue().Set(num);
}

void Initialize(Local<Object> exports) {

  sandbox = RLBoxSandbox<RLBox_DynLib>::createSandbox("", "./mylib.so");
  NODE_SET_METHOD(exports, "hello", Hello);
  NODE_SET_METHOD(exports, "echo", Echo);
  NODE_SET_METHOD(exports, "add", Add);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}
