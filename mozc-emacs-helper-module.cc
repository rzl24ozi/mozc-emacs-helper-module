//#define disableIme
#include <cstdio>
#include <iostream>

#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#ifdef disableIme
#include "base/system_util.h"
#endif
#include "base/util.h"
#include "base/version.h"
#include "client/client.h"
#include "composer/key_parser.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "unix/emacs/client_pool.h"
#include "unix/emacs/mozc_emacs_helper_lib.h"

#ifdef OS_LINUX
#include <signal.h>
#endif // OS_LINUX
#include "emacs-module.h"

namespace {

/* emacs lisp symbols */
emacs_value Qnil;
emacs_value Qt;
emacs_value Fcons;
emacs_value Ffset;
emacs_value Fintegerp;
emacs_value Flist;
emacs_value Fprovide;
emacs_value Fstringp;
emacs_value Fsymbol_name;
emacs_value QCreateSession;
emacs_value QDeleteSession;
emacs_value QSendKey;

/* intern emacs lisp symbols. call at the biginning of emacs_module_init()! */
void
intern_lisp_symbols (emacs_env *env)
{
  Qnil = env->intern (env, "nil");
  Qt = env->intern (env, "t");
  Fcons = env->intern (env, "cons");
  Ffset = env->intern (env, "fset");
  Fintegerp = env->intern (env, "integerp");
  Flist = env->intern (env, "list");
  Fprovide = env->intern (env, "provide");
  Fstringp = env->intern (env, "stringp");
  Fsymbol_name = env->intern (env, "symbol-name");

  /* helper commands */
  QCreateSession = env->intern (env, "CreateSession");
  QDeleteSession = env->intern (env, "DeleteSession");
  QSendKey = env->intern (env, "SendKey");
}

emacs_value
cons (emacs_env *env, emacs_value car, emacs_value cdr)
{
  emacs_value args[] = { car, cdr };
  return env->funcall (env, Fcons, 2, args);
}

bool
integerp (emacs_env *env, emacs_value object)
{
  emacs_value args[] = { object };
  return env->is_not_nil (env, env->funcall (env, Fintegerp, 1, args));
}

/* return '(arg0 arg1) */
emacs_value
list2 (emacs_env *env, emacs_value arg0, emacs_value arg1)
{
  emacs_value args[2] = { arg0, arg1 };
  return env->funcall (env, Flist, 2, args);
}

/* return '(arg0 arg1 arg2) */
emacs_value
list3 (emacs_env *env, emacs_value arg0, emacs_value arg1, emacs_value arg2)
{
  emacs_value args[3] = { arg0, arg1, arg2 };
  return env->funcall (env, Flist, 3, args);
}

bool
stringp (emacs_env *env, emacs_value object)
{
  emacs_value args[] = { object };
  return env->is_not_nil (env, env->funcall (env, Fstringp, 1, args));
}

emacs_value
symbol_name (emacs_env *env, emacs_value symbol)
{
  emacs_value args[] = { symbol };
  return env->funcall (env, Fsymbol_name, 1, args);
}

/* Provide FEATURE to Emacs.  */
void
provide (emacs_env *env, const char *feature)
{
  emacs_value Qfeat = env->intern (env, feature);
  emacs_value args[] = { Qfeat };
  env->funcall (env, Fprovide, 1, args);
}

/* Bind NAME to FUN.  */
void
bind_function (emacs_env *env, const char *name, emacs_value Sfun)
{
  emacs_value Qsym = env->intern (env, name);
  emacs_value args[] = { Qsym, Sfun };
  env->funcall (env, Ffset, 2, args);
}

// return greeting message
emacs_value
Fmozc_emacs_helper_module_recv_greeting (emacs_env *env, ptrdiff_t nargs,
                                         emacs_value args[], void *data)
{
  mozc::config::Config config;
  mozc::config::ConfigHandler::GetConfig (&config);
  const char *preedit_method = "unknown";
  switch (config.preedit_method ())
    {
    case mozc::config::Config::ROMAN:
      preedit_method = "roman";
      break;
    case mozc::config::Config::KANA:
      preedit_method = "kana";
      break;
    }

  string ver = mozc::Version::GetMozcVersion();
  return list3 (env,
           // (mozc-emacs-helper . t)
           cons (env, env->intern (env, "mozc-emacs-helper"), Qt),
           // (version . <version>)
           cons (env, env->intern (env, "version"),
                      env->make_string (env, ver.c_str(), strlen(ver.c_str()))),
           // (config . ((preedit-method . <preedit_method>)))
           cons (env, env->intern (env, "config"),
                      cons (env, cons (env, env->intern (env, "preedit-method"),
                                            env->intern (env, preedit_method)),
                                 Qnil)));
}

emacs_value
make_error_response (emacs_env *env, const char *error, const char *message)
{
  return list2 (env, cons (env, env->intern (env, "error"),
                                env->intern (env, error)),
                     cons (env, env->intern (env, "message"),
                                env->make_string (env, message,
                                                       strlen(message))));
}

emacs_value
parse_args (emacs_env *env, ptrdiff_t nargs, emacs_value args[],
                            uint32 *event_id, uint32 *session_id,
                            mozc::commands::Input *input)
{
  CHECK(event_id);
  CHECK(session_id);
  CHECK(input);

  if (!integerp (env, args[0])) {
    return make_error_response(env, mozc::emacs::kErrWrongTypeArgument,
                                    "Event ID is not an integer");
  }
  *event_id = env->extract_integer (env, args[0]);

  emacs_value func = args[1];
  if (env->eq (env, func, QSendKey)) {
    input->set_type(mozc::commands::Input::SEND_KEY);
  } else if (env->eq (env, func, QCreateSession)) {
    input->set_type(mozc::commands::Input::CREATE_SESSION);
  } else if (env->eq (env, func, QDeleteSession)) {
    input->set_type(mozc::commands::Input::DELETE_SESSION);
  } else {
    return make_error_response(env, mozc::emacs::kErrVoidFunction,
                                    "Unknown function");
  }

  switch (input->type()) {
  case mozc::commands::Input::CREATE_SESSION:
    // EVENT_ID CreateSession
    if (nargs != 2) {
      return make_error_response(env, mozc::emacs::kErrWrongNumberOfArguments,
                                      "Wrong number of arguments");
    }
    break;
  case mozc::commands::Input::DELETE_SESSION:
    // EVENT_ID DeleteSession SESSION_ID
    if (nargs != 3) {
      return make_error_response(env, mozc::emacs::kErrWrongNumberOfArguments,
                                      "Wrong number of arguments");
    }
    if (!integerp (env, args[2])) {
      return make_error_response(env, mozc::emacs::kErrWrongTypeArgument,
                                      "Session ID is not an integer");
    }
    *session_id = env->extract_integer (env, args[2]);
    break;
  case mozc::commands::Input::SEND_KEY: {
    // EVENT_ID SendKey SESSION_ID KEY...
    if (nargs < 4) {
      return make_error_response(env, mozc::emacs::kErrWrongNumberOfArguments,
                                      "Wrong number of arguments");
    }
    if (!integerp (env, args[2])) {
      return make_error_response(env, mozc::emacs::kErrWrongTypeArgument,
                                      "Session ID is not an integer");
    }
    *session_id = env->extract_integer (env, args[2]);
    // Parse keys.
    std::vector<string> keys;
    string key_string;
    for (int i = 3; i < nargs; ++i) {
      if (integerp (env, args[i])) { // Numeric key code
        uint32 key_code;
        key_code = env->extract_integer (env, args[i]);
        if (key_code > 255) {
          return make_error_response(env, mozc::emacs::kErrWrongTypeArgument,
                                          "Wrong character code");
        }
        keys.push_back(string(1, static_cast<char>(key_code)));
      } else if (stringp (env, args[i])) { // String literal
#define LISP_STR_TO_C_STR(EMACS_ENV, LISP_STR, C_STR)                          \
{                                                                              \
  ptrdiff_t size = 0;                                                          \
  (EMACS_ENV)->copy_string_contents ((EMACS_ENV), (LISP_STR), (C_STR), &size); \
  (C_STR) = (char *) alloca (size);                                            \
  (EMACS_ENV)->copy_string_contents ((EMACS_ENV), (LISP_STR), (C_STR), &size); \
}
        if (!key_string.empty()) {
          return make_error_response(env, mozc::emacs::kErrWrongTypeArgument,
                                          "Wrong number of key strings");
        }
        char *key_c_str = NULL;
        LISP_STR_TO_C_STR(env, args[i], key_c_str);
        key_string = key_c_str;
      } else { //Key symbol
        char *keysym = NULL;
        emacs_value keysym_lisp = symbol_name (env, args[i]);
        LISP_STR_TO_C_STR(env, keysym_lisp, keysym);
        keys.push_back(keysym);
#undef LISP_STR_TO_C_STR
      }
    }
    if (!mozc::KeyParser::ParseKeyVector(keys, input->mutable_key()) &&
        !mozc::KeyParser::ParseKey("undefinedkey", input->mutable_key())) {
      DLOG(FATAL);
    }
    if (!key_string.empty()) {
      input->mutable_key()->set_key_string(key_string);
    }
    break;
  }
  default:
    DLOG(FATAL);
  }

  return Qnil;
}

emacs_value make_lisp_message(emacs_env *, const mozc::protobuf::Message &);

emacs_value
make_lisp_field_value (emacs_env *env,
                       const mozc::protobuf::Message &message,
                       const mozc::protobuf::Reflection &reflection,
                       const mozc::protobuf::FieldDescriptor &field, int index)
{
#define GET_FIELD_VALUE(METHOD_TYPE)                             \
  (field.is_repeated() ?                                         \
   reflection.GetRepeated##METHOD_TYPE(message, &field, index) : \
   reflection.Get##METHOD_TYPE(message, &field))

#define BUF_SIZE 256
  char buf[BUF_SIZE];
  emacs_value output;
  switch (field.cpp_type())
    {
    case mozc::protobuf::FieldDescriptor::CPPTYPE_INT32:
      output = env->make_integer (env, GET_FIELD_VALUE(Int32));
      break;
    case mozc::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      output = env->make_integer (env, GET_FIELD_VALUE(UInt32));
      break;
    case mozc::protobuf::FieldDescriptor::CPPTYPE_INT64:
      snprintf (buf, BUF_SIZE,
                     "%" GG_LL_FORMAT "d", (long long)GET_FIELD_VALUE(Int64));
      output = env->make_string (env, buf, strlen(buf));
      break;
    case mozc::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      snprintf (buf, BUF_SIZE,
                     "%" GG_LL_FORMAT "u", (long long)GET_FIELD_VALUE(UInt64)); 
      output = env->make_string (env, buf, strlen(buf));
      break;
    case mozc::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      output = env->make_float (env, GET_FIELD_VALUE(Double));
      break;
    case mozc::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      output = env->make_float (env, GET_FIELD_VALUE(Float));
      break;
    case mozc::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      output = GET_FIELD_VALUE(Bool) ? Qt : Qnil;
      break;
    case mozc::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      {
        string name =
          mozc::emacs::NormalizeSymbol(GET_FIELD_VALUE(Enum)->name());
        output = env->intern (env, name.c_str());
        break;
      }
    case mozc::protobuf::FieldDescriptor::CPPTYPE_STRING:
      {
        string str;
        str = field.is_repeated() ?
          reflection.GetRepeatedStringReference(message, &field, index, &str) :
          reflection.GetStringReference(message, &field, &str);
        output = env->make_string (env, str.c_str(), strlen(str.c_str()));
        break;
      }
    case mozc::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      output = make_lisp_message (env, GET_FIELD_VALUE(Message));
      break;
    }

  return output;
}

emacs_value
make_lisp_field (emacs_env *env, const mozc::protobuf::Message &message,
                                 const mozc::protobuf::Reflection &reflection,
                                 const mozc::protobuf::FieldDescriptor &field)
{
  emacs_value output = Qnil;
  emacs_value value;
  string name = mozc::emacs::NormalizeSymbol(field.name());
  if (!field.is_repeated()) {
    output = make_lisp_field_value (env, message, reflection, field, -1 /* dummy arg */);
  } else {
    const int count = reflection.FieldSize(message, &field);
    const bool is_message =
      field.cpp_type() == mozc::protobuf::FieldDescriptor::CPPTYPE_MESSAGE;
    for (int i = count - 1; i >= 0; --i) {
      if (!is_message) {
        value = make_lisp_field_value (env, message, reflection, field, i);
      } else {
        value =
          make_lisp_message (env,
                             reflection.GetRepeatedMessage(message, &field, i));
      }
      output = cons (env, value, output);
    }
  }
  return cons (env, env->intern (env, name.c_str()), output);
}

emacs_value
make_lisp_message(emacs_env *env, const mozc::protobuf::Message &message)
{
  const mozc::protobuf::Reflection *reflection = message.GetReflection();
  std::vector<const mozc::protobuf::FieldDescriptor*> fields;
  reflection->ListFields(message, &fields);

  emacs_value output = Qnil;
  for (int i = fields.size() - 1; i >= 0; --i)
    output = cons (env,
                   make_lisp_field (env, message, *reflection, *fields[i]),
                   output);

  return output;
}


mozc::emacs::ClientPool client_pool;

// return a response corresponding to args.
emacs_value
Fmozc_emacs_helper_module_send_sexpr (emacs_env *env, ptrdiff_t nargs,
                                      emacs_value args[], void *data)
{
  mozc::commands::Command command;
  command.clear_input();
  command.clear_output();
  uint32 event_id = 0;
  uint32 session_id = 0;

  emacs_value error = parse_args (env, nargs, args,
                                       &event_id, &session_id,
                                       command.mutable_input());
  if (env->is_not_nil (env, error)) return error;

  switch (command.input().type())
    {
    case mozc::commands::Input::CREATE_SESSION:
      session_id = client_pool.CreateClient();
#ifdef OS_WIN
      {
        std::shared_ptr<mozc::client::Client> client =
          client_pool.GetClient(session_id);
        CHECK(client.get());
        mozc::commands::KeyEvent key;
        key.set_special_key(mozc::commands::KeyEvent::ON);
        if (!client->SendKey(key, command.mutable_output()))
          {
            return make_error_response(env, mozc::emacs::kErrSessionError,
                                            "Send IME-On key failed");
          }
      }
#endif // OS_WIN
      break;
    case mozc::commands::Input::DELETE_SESSION:
      client_pool.DeleteClient(session_id);
      break;
    case mozc::commands::Input::SEND_KEY:
      {
        std::shared_ptr<mozc::client::Client> client =
          client_pool.GetClient(session_id);
        CHECK(client.get());
#ifdef OS_LINUX
        sigset_t set, oldset;
        sigfillset(&set);
        sigdelset(&set, SIGTERM);
        sigprocmask(SIG_SETMASK, &set, &oldset);
#endif // OS_LINUX
        bool err = false;
        if ((err = !client->SendKey(command.input().key(),
                                    command.mutable_output())))
          error = make_error_response(env, mozc::emacs::kErrSessionError,
                                             "Session failed.");
#ifdef OS_LINUX
        sigprocmask(SIG_SETMASK, &oldset, NULL);
#endif // OS_LINUX
        if (err) return error;
        break;
      }
    default:
      return make_error_response(env, mozc::emacs::kErrSessionError,
                                      "Session failed");
      break;
    }

  emacs_value resp = list3 (env,
           // (emacs-event-id . <event_id>)
           cons (env, env->intern (env, "emacs-event-id"),
                      env->make_integer (env, event_id)),
           // (emacs-session-id . <session_id>)
           cons (env, env->intern (env, "emacs-session-id"),
                      env->make_integer (env, session_id)),
           // (output . <output>)
           cons (env, env->intern (env, "output"),
                      make_lisp_message (env, command.output())));

  return resp;
}

} // namespace

extern "C" {

int plugin_is_GPL_compatible;

/* Module init function.  */
int
emacs_module_init (struct emacs_runtime *ert)
{
  emacs_env *env = ert->get_environment (ert);

#ifdef disableIme
  mozc::SystemUtil::DisableIME();
#endif // disableIme

  intern_lisp_symbols (env);

#define DEFUN(lsym, csym, amin, amax, doc, data) \
  bind_function (env, lsym, \
                 env->make_function (env, amin, amax, csym, doc, data))

  DEFUN ("mozc-emacs-helper-module-recv-greeting",
         Fmozc_emacs_helper_module_recv_greeting, 0, 0,
         "Return greeting message.", NULL);

  DEFUN ("mozc-emacs-helper-module-send-sexpr",
         Fmozc_emacs_helper_module_send_sexpr, 2, emacs_variadic_function,
         "Return a response corresponding to S-expressions ARGS.", NULL);
#undef DEFUN

  provide (env, "mozc-emacs-helper-module");
  return 0;
}

} // extern "C"
