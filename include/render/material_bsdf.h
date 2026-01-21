#ifndef RENDER_MATERIAL_BSDF_H
#define RENDER_MATERIAL_BSDF_H

#include <stdbool.h>

#include "scene/object_manager.h"
#include "render/fast_rng.h"

typedef enum {
    MATERIAL_BSDF_LAMBERT = 0,
    MATERIAL_BSDF_GGX = 1
} MaterialBSDFModel;

typedef struct MaterialBSDF {
    double albedo;        // grayscale reflectance [0,1]
    double opacity;       // alpha channel [0,1]
    double reflectivity;  // specular weight [0,1]
    double roughness;     // microfacet alpha mapped from [0,1]
    double ior;           // index of refraction for Fresnel
    int textureId;        // placeholder for future sampling
    MaterialBSDFModel model;
    double diffuseWeight;
    double specWeight;
    double weightSum;
    double emissive;      // scalar emission (luminance)
} MaterialBSDF;

typedef struct {
    double dirX;
    double dirY;
    double dirZ;
    double pdf;
    double weight;
    bool specular;
} BSDFSample;

void MaterialBSDFInitFromSceneObject(const SceneObject* obj, MaterialBSDF* material);

double MaterialBSDFDiffuseProbability(const MaterialBSDF* material);
double MaterialBSDFSpecProbability(const MaterialBSDF* material);

bool MaterialBSDFSample(const MaterialBSDF* material,
                        double nx,
                        double ny,
                        double inDirX,
                        double inDirY,
                        double inDirZ,
                        FastRNG* rng,
                        BSDFSample* out);

double MaterialBSDFEvaluateCos(const MaterialBSDF* material,
                               double nx,
                               double ny,
                               double inDirX,
                               double inDirY,
                               double outDirX,
                               double outDirY);

double MaterialBSDFAngularPdf(const MaterialBSDF* material,
                              double nx,
                              double ny,
                              double inDirX,
                              double inDirY,
                              double outDirX,
                              double outDirY);

// 3D convenience wrappers (simple lift with nz/in/out z)
double MaterialBSDFEvaluateCos3(const MaterialBSDF* material,
                                double nx, double ny, double nz,
                                double inDirX, double inDirY, double inDirZ,
                                double outDirX, double outDirY, double outDirZ);

double MaterialBSDFAngularPdf3(const MaterialBSDF* material,
                               double nx, double ny, double nz,
                               double inDirX, double inDirY, double inDirZ,
                               double outDirX, double outDirY, double outDirZ);

double FresnelSchlick(double cosTheta, double f0);

#endif
