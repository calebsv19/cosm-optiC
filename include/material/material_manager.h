#ifndef MATERIAL_MANAGER_H
#define MATERIAL_MANAGER_H

#include "material/material.h"

// Global material library accessors
void MaterialManagerInit(void);
const Material* MaterialManagerGet(int id);
int  MaterialManagerDefaultId(void);
int  MaterialManagerCount(void);
void MaterialManagerResetDefaults(void);
void MaterialManagerLoadDir(const char* dirPath);

#endif // MATERIAL_MANAGER_H
