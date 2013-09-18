/*

libetherdream bindings for Node.js

(c) Ryan Alexander <ryan@onecm.com>, 2013

This program is free software: you can redistribute it and/or modify
it under the terms of either the GNU General Public License version 2
or 3, or the GNU Lesser General Public License version 3, as published
by the Free Software Foundation, at your option.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <node.h>
#include <v8.h>
#include <unistd.h>

#include "etherdream.h"

using namespace v8;


// Helpers from v8_typed_array by Dean McNamee
// https://github.com/deanm/v8_typed_array

int SizeOfArrayElementForType(ExternalArrayType type) {
  switch (type) {
    case kExternalByteArray:
    case kExternalUnsignedByteArray:
    case kExternalPixelArray:
      return 1;
    case kExternalShortArray:
    case kExternalUnsignedShortArray:
      return 2;
    case kExternalIntArray:
    case kExternalUnsignedIntArray:
    case kExternalFloatArray:
      return 4;
    case kExternalDoubleArray:
      return 8;
    default:
      return 0;
  }
}

Handle<Value> ThrowError(const char* msg) {
  return ThrowException(Exception::Error(String::New(msg)));
}

Handle<Value> ThrowTypeError(const char* msg) {
  return ThrowException(Exception::TypeError(String::New(msg)));
}


class Etherdream : public node::ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructor;
  static void Init(Handle<Object> target);

protected:
  Etherdream(etherdream *ed_);

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Connect(const Arguments& args);
  static Handle<Value> Disconnect(const Arguments& args);
  static Handle<Value> Write(const Arguments& args);
  static Handle<Value> IsReady(const Arguments& args);
  static Handle<Value> WhenReady(const Arguments& args);

  etherdream *ed;
};

Persistent<FunctionTemplate> Etherdream::constructor;

Etherdream::Etherdream(etherdream *ed_)
  : ObjectWrap(),
    ed(ed_) {}

void Etherdream::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  Local<String> name = String::NewSymbol("Etherdream");

  constructor = Persistent<FunctionTemplate>::New(tpl);
  // ObjectWrap uses the first internal field to store the wrapped pointer.
  constructor->InstanceTemplate()->SetInternalFieldCount(1);
  constructor->SetClassName(name);

  NODE_SET_PROTOTYPE_METHOD(constructor, "connect", Connect);
  NODE_SET_PROTOTYPE_METHOD(constructor, "disconnect", Disconnect);
  NODE_SET_PROTOTYPE_METHOD(constructor, "write", Write);
  NODE_SET_PROTOTYPE_METHOD(constructor, "isReady", IsReady);
  NODE_SET_PROTOTYPE_METHOD(constructor, "whenReady", WhenReady);

  // This has to be last, otherwise the properties won't show up on the
  // object in JavaScript.
  target->Set(name, constructor->GetFunction());
}

Handle<Value> Etherdream::New(const Arguments& args) {
  HandleScope scope;

  int id = args[0]->ToInteger()->Value();
  etherdream *ed = etherdream_get(id);

  Etherdream* obj = new Etherdream(ed);
  obj->Wrap(args.This());

  return args.This();
}

Handle<Value> Etherdream::Connect(const Arguments& args) {
  HandleScope scope;
  Etherdream* self = ObjectWrap::Unwrap<Etherdream>(args.This());
  int res = etherdream_connect(self->ed);
  return scope.Close(Integer::New(res));
}

Handle<Value> Etherdream::Disconnect(const Arguments& args) {
  HandleScope scope;
  Etherdream* self = ObjectWrap::Unwrap<Etherdream>(args.This());
  etherdream_disconnect(self->ed);
  return Undefined();
}

Handle<Value> Etherdream::Write(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 3)
    return ThrowError("Function requires 3 arguments");

  Local<Object> ptsObj = Local<Object>::Cast(args[0]);
  if (!ptsObj->HasIndexedPropertiesInExternalArrayData())
    return ThrowTypeError("Points data must be an ArrayBuffer");

  int ptsSize = SizeOfArrayElementForType(
      ptsObj->GetIndexedPropertiesExternalArrayDataType());
  int ptsLen = ptsObj->GetIndexedPropertiesExternalArrayDataLength();
  int npts = ptsLen * ptsSize / sizeof(etherdream_point);

  void *pts = ptsObj->GetIndexedPropertiesExternalArrayData();

  int pps = args[1]->Int32Value();
  int repeatCount = args[2]->Int32Value();

  Etherdream* self = ObjectWrap::Unwrap<Etherdream>(args.This());

  int res = etherdream_write(
      self->ed, static_cast<etherdream_point*>(pts), npts, pps, repeatCount);

  return scope.Close(Integer::New(res));
}

Handle<Value> Etherdream::IsReady(const Arguments& args) {
  HandleScope scope;
  Etherdream *self = ObjectWrap::Unwrap<Etherdream>(args.This());
  int res = etherdream_is_ready(self->ed);
  return scope.Close(Integer::New(res));
}


struct Baton {
  Persistent<Function> callback;
  etherdream *ed;
};

void AsyncWaitForReady(uv_work_t *req) {
  Baton* baton = static_cast<Baton*>(req->data);

  // This function currently only returns 0 (success) so we don't bother
  // handling errors.
  etherdream_wait_for_ready(baton->ed);
}

void AsyncAfterWaitForReady(uv_work_t *req) {
  HandleScope scope;
  Baton* baton = static_cast<Baton*>(req->data);

  TryCatch tryCatch;
  baton->callback->Call(Context::GetCurrent()->Global(), 0, NULL);
  if (tryCatch.HasCaught())
    node::FatalException(tryCatch);
}

Handle<Value> Etherdream::WhenReady(const Arguments& args) {
  HandleScope scope;

  if (!args[0]->IsFunction())
    return ThrowTypeError("First argument must be a callback function");

  Local<Function> callback = Local<Function>::Cast(args[0]);

  Baton* baton = new Baton();
  baton->callback = Persistent<Function>::New(callback);

  Etherdream *self = ObjectWrap::Unwrap<Etherdream>(args.This());
  baton->ed = self->ed;

  uv_work_t *req = new uv_work_t();
  req->data = baton;

  int status = uv_queue_work(uv_default_loop(), req,
      AsyncWaitForReady, (uv_after_work_cb)AsyncAfterWaitForReady);
  assert(status == 0);

  return Undefined();
}


Handle<Value> EDStart(const Arguments& args) {
  HandleScope scope;

  // Contrary to what the comments in etherdream.h, this function only ever
  // returns 0 (success). To stay consistent we return the value anyway.
  int res = etherdream_lib_start();

  // Wait for DACs to send a ping.. This can happen in JavaScript.
  // usleep(1200000);

  return scope.Close(Integer::New(res));
}

Handle<Value> EDGetCount(const Arguments& args) {
  HandleScope scope;
  int count = etherdream_dac_count();
  return scope.Close(Integer::New(count));
}


void init(Handle<Object> target) {
  Etherdream::Init(target);
  NODE_SET_METHOD(target, "start", EDStart);
  NODE_SET_METHOD(target, "getCount", EDGetCount);
}

NODE_MODULE(binding, init);
