#include <node.h>
#include <handle_wrap.h>

#include <stdlib.h>

using namespace v8;

namespace node {

#define UNWRAP                                                              \
  assert(!args.Holder().IsEmpty());                                         \
  assert(args.Holder()->InternalFieldCount() > 0);                          \
  FSEventWrap* wrap =                                                       \
      static_cast<FSEventWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) {                                                              \
    SetErrno(UV_EBADF);                                                     \
    return scope.Close(Integer::New(-1));                                   \
  }

class FSEventWrap: public HandleWrap {
public:
  static void Initialize(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Start(const Arguments& args);

private:
  FSEventWrap(Handle<Object> object);
  virtual ~FSEventWrap();

  static void OnEvent(uv_fs_event_t* handle, const char* filename, int events,
    int status);

  uv_fs_event_t handle_;
};


FSEventWrap::FSEventWrap(Handle<Object> object): HandleWrap(object,
                                                    (uv_handle_t*)&handle_) {
  handle_.data = reinterpret_cast<void*>(this);
}


FSEventWrap::~FSEventWrap() {
}


void FSEventWrap::Initialize(Handle<Object> target) {
  HandleWrap::Initialize(target);

  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("FSEvent"));

  NODE_SET_PROTOTYPE_METHOD(t, "start", Start);
  NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

  target->Set(String::NewSymbol("FSEvent"),
              Persistent<FunctionTemplate>::New(t)->GetFunction());
}


Handle<Value> FSEventWrap::New(const Arguments& args) {
  HandleScope scope;

  assert(args.IsConstructCall());
  new FSEventWrap(args.This());

  return scope.Close(args.This());
}


Handle<Value> FSEventWrap::Start(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New("Bad arguments")));
  }

  String::Utf8Value path(args[0]->ToString());

  int r = uv_fs_event_init(uv_default_loop(), &wrap->handle_, *path, OnEvent);
  if (r == 0) {
    // Check for persistent argument
    if (!args[1]->IsTrue()) {
      uv_unref(uv_default_loop());
    }
  } else { 
    SetErrno(uv_last_error(uv_default_loop()).code);
  }

  return scope.Close(Integer::New(r));
}


void FSEventWrap::OnEvent(uv_fs_event_t* handle, const char* filename,
    int events, int status) {
  HandleScope scope;
  Local<String> eventStr;

  FSEventWrap* wrap = reinterpret_cast<FSEventWrap*>(handle->data);

  assert(wrap->object_.IsEmpty() == false);

  if (status) {
    SetErrno(uv_last_error(uv_default_loop()).code);
    eventStr = String::Empty();
  } else {
    switch (events) {
      case UV_RENAME:
        eventStr = String::New("rename");
        break;
      case UV_CHANGE:
        eventStr = String::New("change");
        break;
    }
  }

  Local<Value> argv[3] = {
    Integer::New(status),
    eventStr,
    filename ? (Local<Value>)String::New(filename) : Local<Value>::New(v8::Null())
  };

  MakeCallback(wrap->object_, "onchange", 3, argv);
}
} // namespace node

NODE_MODULE(node_fs_event_wrap, node::FSEventWrap::Initialize);
