/******************************************************************************
 * Copyright (c) 2012, GeoData Institute (www.geodata.soton.ac.uk)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

/**
 * @file asynclog.cpp
 * @brief This defines the `AsyncLog` class.
 */

#include "mapcache.hpp"

/**
 * @details This obtains a reference to the emit method in the passed in
 * `EventEmitter` object and initialises the mutex and async watcher.
 *
 * @param emitter A Node `EventEmitter` handle
 */
AsyncLog::AsyncLog(Persistent<Object> emitter) {
  assert(!emitter.IsEmpty());
  Handle<Value> emit_val = emitter->Get(String::New("emit"));
  assert(emit_val->IsFunction());
  Handle<Function> emit_func = Handle<Function>::Cast(emit_val); 
  callback = Persistent<Function>::New(emit_func);
  this->emitter = Persistent<Object>::New(emitter); // our own persistent reference

  // ensure we can access this instance from the watcher, as it will be
  // accessed asynchronously via the static `EmitLogs` method.
  async.data = this;

  uv_mutex_init(&mutex);
  uv_async_init(uv_default_loop(), &async, EmitLogs);
}

/**
 * @details This is the callback that is passed to the mapcache core library.
 * It formats the message, queues it for emitting later, and then flags `libuv`
 * to deal with it by calling `EmitLogs` at its convenience.
 *
 * This callback will be fired in an asynchronous thread that will *not* be the
 * same as that the main V8/Node.js is running in.
 */
void AsyncLog::LogRequestContext(mapcache_context *c, mapcache_log_level level, char *message, ...) {
  MapCache::request_context *r_ctxt = (MapCache::request_context*) c;
  AsyncLog *self;
  va_list args;

  // we need a reference to the logger
  if (!r_ctxt->async_log) return;
  self = r_ctxt->async_log;

  Log *log = new Log();

  // format the message into a string
  va_start(args, message);
  log->message = apr_pvsprintf(c->pool, message, args);
  va_end(args);
  log->level = level;

  // add the message to the queue
  uv_mutex_lock(&(self->mutex));
  self->message_queue.push(log);
  uv_mutex_unlock(&(self->mutex));

  // flag the message queue as being ready for processing by `EmitLogs`
  uv_ref((uv_handle_t *) &(self->async)); // ensure `async` isn't destroyed
  uv_async_send(&(self->async));
}

/**
 * @details This drains the message queue, emitting each message via the
 * `EventEmitter` handle.  The queue paradigm is necessary as `uv_async_send`
 * does *not* guarantee that `EmitLogs` will be called each time that
 * `LogRequestContext` is fired and therefore messages have to be buffered.
 */
void AsyncLog::EmitLogs(uv_async_t *handle, int status /*UNUSED*/) {
  HandleScope scope;            // back in the main Node/V8 thread
  const unsigned short argc = 3;

  AsyncLog *self = static_cast<AsyncLog*>(handle->data);
  Handle<Value> argv[argc] = {
    String::New("log")          // event name
  };

  uv_mutex_lock(&(self->mutex)); // `handle->data` is thread sensitive
  while (!self->message_queue.empty()) {
    Log *log = self->message_queue.front();
    argv[1] = Uint32::New(log->level);
    argv[2] = String::New(log->message.c_str());

    self->message_queue.pop();   // drain the queue
    delete log;

    self->callback->Call(self->emitter, argc, argv);

    // decrement the reference count created in `LogRequestContext`
    uv_unref((uv_handle_t *) handle);
  }
  uv_mutex_unlock(&(self->mutex));
}
