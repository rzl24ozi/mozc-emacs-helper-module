#if !defined(_WIN32) && !defined(__CYGWIN__)
#error This module can not use unless on Windows.
#endif

#include <windows.h>
#include "emacs-module.h"

int plugin_is_GPL_compatible;

int
emacs_module_init (struct emacs_runtime *ert)
{
  emacs_env *env = ert->get_environment (ert);

  ImmDisableIME (-1);

  emacs_value Qfeat = env->intern (env, "disableIme");
  emacs_value Qprovide = env->intern (env, "provide");
  emacs_value args[] = { Qfeat };
  env->funcall (env, Qprovide, 1, args);
  return 0;
}
