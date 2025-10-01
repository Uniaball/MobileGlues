//
// Created by hanji on 2025/2/9.
//

#ifndef MOBILEGLUES_PLUGIN_GPU_UTILS_H
#define MOBILEGLUES_PLUGIN_GPU_UTILS_H

#include <string.h>
#include <string>

std::string getGPUInfo();

#ifdef __cplusplus
extern "C" {
#endif

int isAdreno(const char *gpu);

int isAdreno730(const char *gpu);

int isAdreno740(const char *gpu);

int isAdreno830(const char *gpu);

int hasVulkan12();

bool checkIfANGLESupported(const char* gpu);

#ifdef __cplusplus
}
#endif

#endif //MOBILEGLUES_PLUGIN_GPU_UTILS_H
