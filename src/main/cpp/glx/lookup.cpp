//
// Created by BZLZHH on 2025/1/27.
//

#include "lookup.h"

#include "../config/settings.h"
#include "../gl/envvars.h"
#include "../gl/log.h"
#include "../gl/mg.h"
#include "../includes.h"
#include <EGL/egl.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

#define DEBUG 0

std::string handle_multidraw_func_name(std::string name) {
  std::string namestr = name;
  if (namestr != "glMultiDrawElementsBaseVertex" &&
      namestr != "glMultiDrawElements") {
    return name;
  } else {
    namestr = "mg_" + namestr;
  }

  switch (global_settings.multidraw_mode) {
  case multidraw_mode_t::PreferIndirect:
    namestr += "_indirect";
    break;
  case multidraw_mode_t::PreferBaseVertex:
    namestr += "_basevertex";
    break;
  case multidraw_mode_t::PreferMultidrawIndirect:
    namestr += "_multiindirect";
    break;
  case multidraw_mode_t::DrawElements:
    namestr += "_drawelements";
    break;
  case multidraw_mode_t::Compute:
    namestr += "_compute";
    break;
  default:
    LOG_W("get_multidraw_func() cannot determine multidraw emulation mode!")
    return {};
  }

  return namestr;
}

void *glXGetProcAddress(const char *name) {
  LOG()
  std::string real_func_name = handle_multidraw_func_name(std::string(name));
#ifdef __APPLE__
  return dlsym((void *)(~(uintptr_t)0), real_func_name.c_str());
#else

  void *proc = nullptr;

  proc = dlsym(RTLD_DEFAULT, real_func_name.c_str());

  if (!proc) {
    LOG_W("Failed to get OpenGL function: %s", real_func_name.c_str())
    return nullptr;
  }

  return proc;
#endif
}

void *glXGetProcAddressARB(const char *name) { return glXGetProcAddress(name); }