/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkLights.h"
#include "SkReadBuffer.h"
#include "SkShadowShader.h"
#include "SkPoint3.h"

////////////////////////////////////////////////////////////////////////////
#ifdef SK_EXPERIMENTAL_SHADOWING


/** \class SkShadowShaderImpl
    This subclass of shader applies shadowing
*/
class SkShadowShaderImpl : public SkShader {
public:
    /** Create a new shadowing shader that shadows
        @param to do        to do
    */
    SkShadowShaderImpl(sk_sp<SkShader> povDepthShader,
                       sk_sp<SkShader> diffuseShader,
                       sk_sp<SkLights> lights,
                       int diffuseWidth, int diffuseHeight)
            : fPovDepthShader(std::move(povDepthShader))
            , fDiffuseShader(std::move(diffuseShader))
            , fLights(std::move(lights))
            , fDiffuseWidth(diffuseWidth)
            , fDiffuseHeight(diffuseHeight) { }

    bool isOpaque() const override;

#if SK_SUPPORT_GPU
    sk_sp<GrFragmentProcessor> asFragmentProcessor(const AsFPArgs&) const override;
#endif

    class ShadowShaderContext : public SkShader::Context {
    public:
        // The context takes ownership of the states. It will call their destructors
        // but will NOT free the memory.
        ShadowShaderContext(const SkShadowShaderImpl&, const ContextRec&,
                            SkShader::Context* povDepthContext,
                            SkShader::Context* diffuseContext,
                            void* heapAllocated);

        ~ShadowShaderContext() override;

        void shadeSpan(int x, int y, SkPMColor[], int count) override;

        uint32_t getFlags() const override { return fFlags; }

    private:
        SkShader::Context*        fPovDepthContext;
        SkShader::Context*        fDiffuseContext;
        uint32_t                  fFlags;

        void* fHeapAllocated;

        typedef SkShader::Context INHERITED;
    };

    SK_TO_STRING_OVERRIDE()
    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(SkShadowShaderImpl)

protected:
    void flatten(SkWriteBuffer&) const override;
    size_t onContextSize(const ContextRec&) const override;
    Context* onCreateContext(const ContextRec&, void*) const override;

private:
    sk_sp<SkShader> fPovDepthShader;
    sk_sp<SkShader> fDiffuseShader;
    sk_sp<SkLights> fLights;

    int fDiffuseWidth;
    int fDiffuseHeight;

    friend class SkShadowShader;

    typedef SkShader INHERITED;
};

////////////////////////////////////////////////////////////////////////////

#if SK_SUPPORT_GPU

#include "GrCoordTransform.h"
#include "GrFragmentProcessor.h"
#include "GrInvariantOutput.h"
#include "glsl/GrGLSLFragmentProcessor.h"
#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "SkGr.h"
#include "SkGrPriv.h"
#include "SkSpecialImage.h"
#include "SkImage_Base.h"
#include "GrContext.h"

class ShadowFP : public GrFragmentProcessor {
public:
    ShadowFP(sk_sp<GrFragmentProcessor> povDepth,
             sk_sp<GrFragmentProcessor> diffuse,
             sk_sp<SkLights> lights,
             int diffuseWidth, int diffuseHeight,
             GrContext* context) {

        // fuse all ambient lights into a single one
        fAmbientColor.set(0.0f, 0.0f, 0.0f);

        fNumDirLights = 0; // refers to directional lights.
        for (int i = 0; i < lights->numLights(); ++i) {
            if (SkLights::Light::kAmbient_LightType == lights->light(i).type()) {
                fAmbientColor += lights->light(i).color();
            } else if (fNumDirLights < SkShadowShader::kMaxNonAmbientLights) {
                fLightColor[fNumDirLights] = lights->light(i).color();
                fLightDir[fNumDirLights] = lights->light(i).dir();
                SkImage_Base* shadowMap = ((SkImage_Base*)lights->light(i).getShadowMap());

                // gets deleted when the ShadowFP is destroyed, and frees the GrTexture*
                fTexture[fNumDirLights] = sk_sp<GrTexture>(shadowMap->asTextureRef(context,
                                                           GrTextureParams::ClampNoFilter(),
                                                           SkSourceGammaTreatment::kIgnore));
                fDepthMapAccess[fNumDirLights].reset(fTexture[fNumDirLights].get());
                this->addTextureAccess(&fDepthMapAccess[fNumDirLights]);

                fDepthMapHeight[fNumDirLights] = shadowMap->height();
                fDepthMapWidth[fNumDirLights] = shadowMap->width();

                fNumDirLights++;
            }
        }

        fWidth = diffuseWidth;
        fHeight = diffuseHeight;

        this->registerChildProcessor(std::move(povDepth)); 
        this->registerChildProcessor(std::move(diffuse));
        this->initClassID<ShadowFP>();
    }

    class GLSLShadowFP : public GrGLSLFragmentProcessor {
    public:
        GLSLShadowFP() { }

        void emitCode(EmitArgs& args) override {

            GrGLSLFragmentBuilder* fragBuilder = args.fFragBuilder;
            GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;

            // add uniforms
            int32_t numLights = args.fFp.cast<ShadowFP>().fNumDirLights;
            SkASSERT(numLights <= SkShadowShader::kMaxNonAmbientLights);

            const char* lightDirUniName[SkShadowShader::kMaxNonAmbientLights] = {nullptr};
            const char* lightColorUniName[SkShadowShader::kMaxNonAmbientLights] = {nullptr};

            const char* depthMapWidthUniName[SkShadowShader::kMaxNonAmbientLights]
                    = {nullptr};
            const char* depthMapHeightUniName[SkShadowShader::kMaxNonAmbientLights]
                    = {nullptr};

            SkString lightDirUniNameBase("lightDir");
            SkString lightColorUniNameBase("lightColor");

            SkString depthMapWidthUniNameBase("dmapWidth");
            SkString depthMapHeightUniNameBase("dmapHeight");

            for (int i = 0; i < numLights; i++) {
                SkString lightDirUniNameStr(lightDirUniNameBase);
                lightDirUniNameStr.appendf("%d", i);
                SkString lightColorUniNameStr(lightColorUniNameBase);
                lightColorUniNameStr.appendf("%d", i);

                SkString depthMapWidthUniNameStr(depthMapWidthUniNameBase);
                depthMapWidthUniNameStr.appendf("%d", i);
                SkString depthMapHeightUniNameStr(depthMapHeightUniNameBase);
                depthMapHeightUniNameStr.appendf("%d", i);

                fLightDirUni[i] = uniformHandler->addUniform(kFragment_GrShaderFlag,
                                                             kVec3f_GrSLType,
                                                             kDefault_GrSLPrecision,
                                                             lightDirUniNameStr.c_str(),
                                                             &lightDirUniName[i]);
                fLightColorUni[i] = uniformHandler->addUniform(kFragment_GrShaderFlag,
                                                               kVec3f_GrSLType,
                                                               kDefault_GrSLPrecision,
                                                               lightColorUniNameStr.c_str(),
                                                               &lightColorUniName[i]);

                fDepthMapWidthUni[i]  = uniformHandler->addUniform(kFragment_GrShaderFlag,
                                                   kInt_GrSLType,
                                                   kDefault_GrSLPrecision,
                                                   depthMapWidthUniNameStr.c_str(),
                                                   &depthMapWidthUniName[i]);
                fDepthMapHeightUni[i] = uniformHandler->addUniform(kFragment_GrShaderFlag,
                                                   kInt_GrSLType,
                                                   kDefault_GrSLPrecision,
                                                   depthMapHeightUniNameStr.c_str(),
                                                   &depthMapHeightUniName[i]);
            }


            const char* widthUniName = nullptr;
            const char* heightUniName = nullptr;

            fWidthUni = uniformHandler->addUniform(kFragment_GrShaderFlag,
                                                   kInt_GrSLType,
                                                   kDefault_GrSLPrecision,
                                                   "width", &widthUniName);
            fHeightUni = uniformHandler->addUniform(kFragment_GrShaderFlag,
                                                    kInt_GrSLType,
                                                    kDefault_GrSLPrecision,
                                                    "height", &heightUniName);


            SkString povDepth("povDepth");
            this->emitChild(0, nullptr, &povDepth, args);

            SkString diffuseColor("inDiffuseColor");
            this->emitChild(1, nullptr, &diffuseColor, args);

            SkString depthMaps[SkShadowShader::kMaxNonAmbientLights];

            for (int i = 0; i < numLights; i++) {
                SkString povCoord("povCoord");
                povCoord.appendf("%d", i);

                // vMatrixCoord_0_1_Stage0 is the texture sampler coordinates.
                // povDepth.b * 255 scales it to 0 - 255, bringing it to world space,
                // and the / 400 brings it back to a sampler coordinate, 0 - 1
                // The 400 comes from the shadowmaps GM.
                // TODO use real shadowmaps size
                SkString offset("offset");
                offset.appendf("%d", i);

                SkString scaleVec("scaleVec");
                scaleVec.appendf("%d", i);

                SkString scaleOffsetVec("scaleOffsetVec");
                scaleOffsetVec.appendf("%d", i);

                fragBuilder->codeAppendf("vec2 %s = vec2(%s) * povDepth.b * 255 / 400;\n",
                                         offset.c_str(), lightDirUniName[i]);

                fragBuilder->codeAppendf("vec2 %s = (vec2(%s, %s) / vec2(%s, %s));\n",
                                         scaleVec.c_str(),
                                         widthUniName, heightUniName,
                                         depthMapWidthUniName[i], depthMapHeightUniName[i]);

                fragBuilder->codeAppendf("vec2 %s = 1 - %s;\n",
                                         scaleOffsetVec.c_str(), scaleVec.c_str());


                fragBuilder->codeAppendf("vec2 %s = (vMatrixCoord_0_1_Stage0 + "
                                                    "vec2(%s.x, 0 - %s.y)) "
                                                   " * %s + vec2(0,1) * %s;\n",

                                         povCoord.c_str(), offset.c_str(), offset.c_str(),
                                         scaleVec.c_str(), scaleOffsetVec.c_str());

                fragBuilder->appendTextureLookup(&depthMaps[i], args.fTexSamplers[i],
                                                 povCoord.c_str(),
                                                 kVec2f_GrSLType);
            }

            const char* ambientColorUniName = nullptr;
            fAmbientColorUni = uniformHandler->addUniform(kFragment_GrShaderFlag,
                                                          kVec3f_GrSLType, kDefault_GrSLPrecision,
                                                          "AmbientColor", &ambientColorUniName);

            fragBuilder->codeAppendf("vec4 resultDiffuseColor = %s;", diffuseColor.c_str());

            // Essentially,
            // diffColor * (ambientLightTot + foreachDirLight(lightColor * (N . L)))
            SkString totalLightColor("totalLightColor");
            fragBuilder->codeAppendf("vec3 %s = vec3(0);", totalLightColor.c_str());

            for (int i = 0; i < numLights; i++) {
                fragBuilder->codeAppendf("if (%s.b >= %s.b) {",
                                         povDepth.c_str(), depthMaps[i].c_str());
                // Note that dot(vec3(0,0,1), %s) == %s.z * %s
                fragBuilder->codeAppendf("%s += %s.z * %s;",
                                         totalLightColor.c_str(),
                                         lightDirUniName[i],
                                         lightColorUniName[i]);
                fragBuilder->codeAppendf("}");
            }

            fragBuilder->codeAppendf("%s += %s;",
                                     totalLightColor.c_str(),
                                     ambientColorUniName);

            fragBuilder->codeAppendf("resultDiffuseColor *= vec4(%s, 1);",
                                     totalLightColor.c_str());

            fragBuilder->codeAppendf("%s = resultDiffuseColor;", args.fOutputColor);
        }

        static void GenKey(const GrProcessor& proc, const GrGLSLCaps&,
                           GrProcessorKeyBuilder* b) {
            const ShadowFP& shadowFP = proc.cast<ShadowFP>();
            b->add32(shadowFP.fNumDirLights);
        }

    protected:
        void onSetData(const GrGLSLProgramDataManager& pdman, const GrProcessor& proc) override {
            const ShadowFP &shadowFP = proc.cast<ShadowFP>();

            fNumDirLights = shadowFP.numLights();

            for (int i = 0; i < fNumDirLights; i++) {
                const SkVector3& lightDir = shadowFP.lightDir(i);
                if (lightDir != fLightDir[i]) {
                    pdman.set3fv(fLightDirUni[i], 1, &lightDir.fX);
                    fLightDir[i] = lightDir;
                }
                const SkColor3f& lightColor = shadowFP.lightColor(i);
                if (lightColor != fLightColor[i]) {
                    pdman.set3fv(fLightColorUni[i], 1, &lightColor.fX);
                    fLightColor[i] = lightColor;
                }

                int depthMapWidth = shadowFP.depthMapWidth(i);
                if (depthMapWidth != fDepthMapWidth[i]) {
                    pdman.set1i(fDepthMapWidthUni[i], depthMapWidth);
                    fDepthMapWidth[i] = depthMapWidth;
                }
                int depthMapHeight = shadowFP.depthMapHeight(i);
                if (depthMapHeight != fDepthMapHeight[i]) {
                    pdman.set1i(fDepthMapHeightUni[i], depthMapHeight);
                    fDepthMapHeight[i] = depthMapHeight;
                }
            }

            int width = shadowFP.width();
            if (width != fWidth) {
                pdman.set1i(fWidthUni, width);
                fWidth = width;
            }
            int height = shadowFP.height();
            if (height != fHeight) {
                pdman.set1i(fHeightUni, height);
                fHeight = height;
            }

            const SkColor3f& ambientColor = shadowFP.ambientColor();
            if (ambientColor != fAmbientColor) {
                pdman.set3fv(fAmbientColorUni, 1, &ambientColor.fX);
                fAmbientColor = ambientColor;
            }
        }

    private:
        SkVector3 fLightDir[SkShadowShader::kMaxNonAmbientLights];
        GrGLSLProgramDataManager::UniformHandle
                fLightDirUni[SkShadowShader::kMaxNonAmbientLights];

        SkColor3f fLightColor[SkShadowShader::kMaxNonAmbientLights];
        GrGLSLProgramDataManager::UniformHandle
                fLightColorUni[SkShadowShader::kMaxNonAmbientLights];

        int fDepthMapWidth[SkShadowShader::kMaxNonAmbientLights];
        GrGLSLProgramDataManager::UniformHandle
                fDepthMapWidthUni[SkShadowShader::kMaxNonAmbientLights];

        int fDepthMapHeight[SkShadowShader::kMaxNonAmbientLights];
        GrGLSLProgramDataManager::UniformHandle
                fDepthMapHeightUni[SkShadowShader::kMaxNonAmbientLights];

        int fWidth;
        GrGLSLProgramDataManager::UniformHandle fWidthUni;
        int fHeight;
        GrGLSLProgramDataManager::UniformHandle fHeightUni;

        SkColor3f fAmbientColor;
        GrGLSLProgramDataManager::UniformHandle fAmbientColorUni;

        int fNumDirLights;
    };

    void onGetGLSLProcessorKey(const GrGLSLCaps& caps, GrProcessorKeyBuilder* b) const override {
        GLSLShadowFP::GenKey(*this, caps, b);
    }

    const char* name() const override { return "shadowFP"; }

    void onComputeInvariantOutput(GrInvariantOutput* inout) const override {
        inout->mulByUnknownFourComponents();
    }
    int32_t numLights() const { return fNumDirLights; }
    const SkColor3f& ambientColor() const { return fAmbientColor; }
    const SkVector3& lightDir(int i) const {
        SkASSERT(i < fNumDirLights);
        return fLightDir[i];
    }
    const SkVector3& lightColor(int i) const {
        SkASSERT(i < fNumDirLights);
        return fLightColor[i];
    }

    int depthMapWidth(int i) const {
        SkASSERT(i < fNumDirLights);
        return fDepthMapWidth[i];
    }
    int depthMapHeight(int i) const {
        SkASSERT(i < fNumDirLights);
        return fDepthMapHeight[i];
    }
    int width() const {return fWidth; }
    int height() const {return fHeight; }

private:
    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override { return new GLSLShadowFP; }

    bool onIsEqual(const GrFragmentProcessor& proc) const override {
        const ShadowFP& shadowFP = proc.cast<ShadowFP>();
        if (fAmbientColor != shadowFP.fAmbientColor || fNumDirLights != shadowFP.fNumDirLights) {
            return false;
        }

        if (fWidth != shadowFP.fWidth || fHeight != shadowFP.fHeight) {
            return false;
        }

        for (int i = 0; i < fNumDirLights; i++) {
            if (fLightDir[i] != shadowFP.fLightDir[i] ||
                fLightColor[i] != shadowFP.fLightColor[i]) {
                return false;
            }

            if (fDepthMapWidth[i] != shadowFP.fDepthMapWidth[i] ||
                fDepthMapHeight[i] != shadowFP.fDepthMapHeight[i]) {
                return false;
            }
        }

        return true;
    }

    int              fNumDirLights;

    SkVector3        fLightDir[SkShadowShader::kMaxNonAmbientLights];
    SkColor3f        fLightColor[SkShadowShader::kMaxNonAmbientLights];
    GrTextureAccess  fDepthMapAccess[SkShadowShader::kMaxNonAmbientLights];
    sk_sp<GrTexture> fTexture[SkShadowShader::kMaxNonAmbientLights];

    int              fDepthMapWidth[SkShadowShader::kMaxNonAmbientLights];
    int              fDepthMapHeight[SkShadowShader::kMaxNonAmbientLights];

    int              fHeight;
    int              fWidth;

    SkColor3f        fAmbientColor;
};

////////////////////////////////////////////////////////////////////////////

sk_sp<GrFragmentProcessor> SkShadowShaderImpl::asFragmentProcessor(const AsFPArgs& fpargs) const {

    sk_sp<GrFragmentProcessor> povDepthFP = fPovDepthShader->asFragmentProcessor(fpargs);

    sk_sp<GrFragmentProcessor> diffuseFP = fDiffuseShader->asFragmentProcessor(fpargs);

    sk_sp<GrFragmentProcessor> shadowfp = sk_make_sp<ShadowFP>(std::move(povDepthFP),
                                                               std::move(diffuseFP),
                                                               std::move(fLights),
                                                               fDiffuseWidth, fDiffuseHeight,
                                                               fpargs.fContext);
    return shadowfp;
}


#endif

////////////////////////////////////////////////////////////////////////////

bool SkShadowShaderImpl::isOpaque() const {
    return fDiffuseShader->isOpaque();
}

SkShadowShaderImpl::ShadowShaderContext::ShadowShaderContext(
        const SkShadowShaderImpl& shader, const ContextRec& rec,
        SkShader::Context* povDepthContext,
        SkShader::Context* diffuseContext,
        void* heapAllocated)
        : INHERITED(shader, rec)
        , fPovDepthContext(povDepthContext)
        , fDiffuseContext(diffuseContext)
        , fHeapAllocated(heapAllocated) {
    bool isOpaque = shader.isOpaque();

    // update fFlags
    uint32_t flags = 0;
    if (isOpaque && (255 == this->getPaintAlpha())) {
        flags |= kOpaqueAlpha_Flag;
    }

    fFlags = flags;
}

SkShadowShaderImpl::ShadowShaderContext::~ShadowShaderContext() {
    // The dependencies have been created outside of the context on memory that was allocated by
    // the onCreateContext() method. Call the destructors and free the memory.
    fPovDepthContext->~Context();
    fDiffuseContext->~Context();

    sk_free(fHeapAllocated);
}

static inline SkPMColor convert(SkColor3f color, U8CPU a) {
    if (color.fX <= 0.0f) {
        color.fX = 0.0f;
    } else if (color.fX >= 255.0f) {
        color.fX = 255.0f;
    }

    if (color.fY <= 0.0f) {
        color.fY = 0.0f;
    } else if (color.fY >= 255.0f) {
        color.fY = 255.0f;
    }

    if (color.fZ <= 0.0f) {
        color.fZ = 0.0f;
    } else if (color.fZ >= 255.0f) {
        color.fZ = 255.0f;
    }

    return SkPreMultiplyARGB(a, (int) color.fX,  (int) color.fY, (int) color.fZ);
}

// larger is better (fewer times we have to loop), but we shouldn't
// take up too much stack-space (each one here costs 16 bytes)
#define BUFFER_MAX 16
void SkShadowShaderImpl::ShadowShaderContext::shadeSpan(int x, int y,
                                                        SkPMColor result[], int count) {
    const SkShadowShaderImpl& lightShader = static_cast<const SkShadowShaderImpl&>(fShader);

    SkPMColor diffuse[BUFFER_MAX];

    do {
        int n = SkTMin(count, BUFFER_MAX);

        fPovDepthContext->shadeSpan(x, y, diffuse, n);
        fDiffuseContext->shadeSpan(x, y, diffuse, n);

        for (int i = 0; i < n; ++i) {

            SkColor diffColor = SkUnPreMultiply::PMColorToColor(diffuse[i]);

            SkColor3f accum = SkColor3f::Make(0.0f, 0.0f, 0.0f);
            // This is all done in linear unpremul color space (each component 0..255.0f though)
            for (int l = 0; l < lightShader.fLights->numLights(); ++l) {
                const SkLights::Light& light = lightShader.fLights->light(l);

                if (SkLights::Light::kAmbient_LightType == light.type()) {
                    accum.fX += light.color().fX * SkColorGetR(diffColor);
                    accum.fY += light.color().fY * SkColorGetG(diffColor);
                    accum.fZ += light.color().fZ * SkColorGetB(diffColor);
                } else {
                    // scaling by fZ accounts for lighting direction
                    accum.fX += light.color().makeScale(light.dir().fZ).fX * SkColorGetR(diffColor);
                    accum.fY += light.color().makeScale(light.dir().fZ).fY * SkColorGetG(diffColor);
                    accum.fZ += light.color().makeScale(light.dir().fZ).fZ * SkColorGetB(diffColor);
                }
            }

            result[i] = convert(accum, SkColorGetA(diffColor));
        }

        result += n;
        x += n;
        count -= n;
    } while (count > 0);
}

////////////////////////////////////////////////////////////////////////////

#ifndef SK_IGNORE_TO_STRING
void SkShadowShaderImpl::toString(SkString* str) const {
    str->appendf("ShadowShader: ()");
}
#endif

sk_sp<SkFlattenable> SkShadowShaderImpl::CreateProc(SkReadBuffer& buf) {

    // Discarding SkShader flattenable params
    bool hasLocalMatrix = buf.readBool();
    SkAssertResult(!hasLocalMatrix);

    sk_sp<SkLights> lights = SkLights::MakeFromBuffer(buf);

    int diffuseWidth = buf.readInt();
    int diffuseHeight = buf.readInt();

    sk_sp<SkShader> povDepthShader(buf.readFlattenable<SkShader>());
    sk_sp<SkShader> diffuseShader(buf.readFlattenable<SkShader>());

    return sk_make_sp<SkShadowShaderImpl>(std::move(povDepthShader),
                                          std::move(diffuseShader),
                                          std::move(lights),
                                          diffuseWidth, diffuseHeight);
}

void SkShadowShaderImpl::flatten(SkWriteBuffer& buf) const {
    this->INHERITED::flatten(buf);

    fLights->flatten(buf);

    buf.writeInt(fDiffuseWidth);
    buf.writeInt(fDiffuseHeight);

    buf.writeFlattenable(fPovDepthShader.get());
    buf.writeFlattenable(fDiffuseShader.get());
}

size_t SkShadowShaderImpl::onContextSize(const ContextRec& rec) const {
    return sizeof(ShadowShaderContext);
}

SkShader::Context* SkShadowShaderImpl::onCreateContext(const ContextRec& rec,
                                                       void* storage) const {
    size_t heapRequired = fPovDepthShader->contextSize(rec) +
                          fDiffuseShader->contextSize(rec);

    void* heapAllocated = sk_malloc_throw(heapRequired);

    void* povDepthContextStorage = heapAllocated;

    SkShader::Context* povDepthContext =
            fPovDepthShader->createContext(rec, povDepthContextStorage);

    if (!povDepthContext) {
        sk_free(heapAllocated);
        return nullptr;
    }

    void* diffuseContextStorage = (char*)heapAllocated + fPovDepthShader->contextSize(rec);

    SkShader::Context* diffuseContext = fDiffuseShader->createContext(rec, diffuseContextStorage);
    if (!diffuseContext) {
        sk_free(heapAllocated);
        return nullptr;
    }

    return new (storage) ShadowShaderContext(*this, rec, povDepthContext, diffuseContext,
                                             heapAllocated);
}

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkShader> SkShadowShader::Make(sk_sp<SkShader> povDepthShader,
                                     sk_sp<SkShader> diffuseShader,
                                     sk_sp<SkLights> lights,
                                     int diffuseWidth, int diffuseHeight) {
    if (!povDepthShader || !diffuseShader) {
        // TODO: Use paint's color in absence of a diffuseShader
        // TODO: Use a default implementation of normalSource instead
        return nullptr;
    }

    return sk_make_sp<SkShadowShaderImpl>(std::move(povDepthShader),
                                          std::move(diffuseShader),
                                          std::move(lights),
                                          diffuseWidth, diffuseHeight);
}

///////////////////////////////////////////////////////////////////////////////

SK_DEFINE_FLATTENABLE_REGISTRAR_GROUP_START(SkShadowShader)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkShadowShaderImpl)
SK_DEFINE_FLATTENABLE_REGISTRAR_GROUP_END

///////////////////////////////////////////////////////////////////////////////

#endif
