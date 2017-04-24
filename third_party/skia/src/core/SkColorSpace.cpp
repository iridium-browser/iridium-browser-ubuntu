/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkColorSpace.h"
#include "SkColorSpace_Base.h"
#include "SkColorSpace_XYZ.h"
#include "SkColorSpacePriv.h"
#include "SkOnce.h"
#include "SkPoint3.h"

bool SkColorSpacePrimaries::toXYZD50(SkMatrix44* toXYZ_D50) const {
    if (!is_zero_to_one(fRX) || !is_zero_to_one(fRY) ||
        !is_zero_to_one(fGX) || !is_zero_to_one(fGY) ||
        !is_zero_to_one(fBX) || !is_zero_to_one(fBY) ||
        !is_zero_to_one(fWX) || !is_zero_to_one(fWY))
    {
        return false;
    }

    // First, we need to convert xy values (primaries) to XYZ.
    SkMatrix primaries;
    primaries.setAll(             fRX,              fGX,              fBX,
                                  fRY,              fGY,              fBY,
                     1.0f - fRX - fRY, 1.0f - fGX - fGY, 1.0f - fBX - fBY);
    SkMatrix primariesInv;
    if (!primaries.invert(&primariesInv)) {
        return false;
    }

    // Assumes that Y is 1.0f.
    SkVector3 wXYZ = SkVector3::Make(fWX / fWY, 1.0f, (1.0f - fWX - fWY) / fWY);
    SkVector3 XYZ;
    XYZ.fX = primariesInv[0] * wXYZ.fX + primariesInv[1] * wXYZ.fY + primariesInv[2] * wXYZ.fZ;
    XYZ.fY = primariesInv[3] * wXYZ.fX + primariesInv[4] * wXYZ.fY + primariesInv[5] * wXYZ.fZ;
    XYZ.fZ = primariesInv[6] * wXYZ.fX + primariesInv[7] * wXYZ.fY + primariesInv[8] * wXYZ.fZ;
    SkMatrix toXYZ;
    toXYZ.setAll(XYZ.fX,   0.0f,   0.0f,
                   0.0f, XYZ.fY,   0.0f,
                   0.0f,   0.0f, XYZ.fZ);
    toXYZ.postConcat(primaries);

    // Now convert toXYZ matrix to toXYZD50.
    SkVector3 wXYZD50 = SkVector3::Make(0.96422f, 1.0f, 0.82521f);

    // Calculate the chromatic adaptation matrix.  We will use the Bradford method, thus
    // the matrices below.  The Bradford method is used by Adobe and is widely considered
    // to be the best.
    SkMatrix mA, mAInv;
    mA.setAll(+0.8951f, +0.2664f, -0.1614f,
              -0.7502f, +1.7135f, +0.0367f,
              +0.0389f, -0.0685f, +1.0296f);
    mAInv.setAll(+0.9869929f, -0.1470543f, +0.1599627f,
                 +0.4323053f, +0.5183603f, +0.0492912f,
                 -0.0085287f, +0.0400428f, +0.9684867f);

    SkVector3 srcCone;
    srcCone.fX = mA[0] * wXYZ.fX + mA[1] * wXYZ.fY + mA[2] * wXYZ.fZ;
    srcCone.fY = mA[3] * wXYZ.fX + mA[4] * wXYZ.fY + mA[5] * wXYZ.fZ;
    srcCone.fZ = mA[6] * wXYZ.fX + mA[7] * wXYZ.fY + mA[8] * wXYZ.fZ;
    SkVector3 dstCone;
    dstCone.fX = mA[0] * wXYZD50.fX + mA[1] * wXYZD50.fY + mA[2] * wXYZD50.fZ;
    dstCone.fY = mA[3] * wXYZD50.fX + mA[4] * wXYZD50.fY + mA[5] * wXYZD50.fZ;
    dstCone.fZ = mA[6] * wXYZD50.fX + mA[7] * wXYZD50.fY + mA[8] * wXYZD50.fZ;

    SkMatrix DXToD50;
    DXToD50.setIdentity();
    DXToD50[0] = dstCone.fX / srcCone.fX;
    DXToD50[4] = dstCone.fY / srcCone.fY;
    DXToD50[8] = dstCone.fZ / srcCone.fZ;
    DXToD50.postConcat(mAInv);
    DXToD50.preConcat(mA);

    toXYZ.postConcat(DXToD50);
    toXYZ_D50->set3x3(toXYZ[0], toXYZ[3], toXYZ[6],
                      toXYZ[1], toXYZ[4], toXYZ[7],
                      toXYZ[2], toXYZ[5], toXYZ[8]);
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

SkColorSpace_Base::SkColorSpace_Base(sk_sp<SkData> profileData, uint32_t flags)
    : fProfileData(std::move(profileData))
    , fFlags(flags)
{}

/**
 *  Checks if our toXYZ matrix is a close match to a known color gamut.
 *
 *  @param toXYZD50 transformation matrix deduced from profile data
 *  @param standard 3x3 canonical transformation matrix
 */
static bool xyz_almost_equal(const SkMatrix44& toXYZD50, const float* standard) {
    return color_space_almost_equal(toXYZD50.getFloat(0, 0), standard[0]) &&
           color_space_almost_equal(toXYZD50.getFloat(0, 1), standard[1]) &&
           color_space_almost_equal(toXYZD50.getFloat(0, 2), standard[2]) &&
           color_space_almost_equal(toXYZD50.getFloat(1, 0), standard[3]) &&
           color_space_almost_equal(toXYZD50.getFloat(1, 1), standard[4]) &&
           color_space_almost_equal(toXYZD50.getFloat(1, 2), standard[5]) &&
           color_space_almost_equal(toXYZD50.getFloat(2, 0), standard[6]) &&
           color_space_almost_equal(toXYZD50.getFloat(2, 1), standard[7]) &&
           color_space_almost_equal(toXYZD50.getFloat(2, 2), standard[8]) &&
           color_space_almost_equal(toXYZD50.getFloat(0, 3), 0.0f) &&
           color_space_almost_equal(toXYZD50.getFloat(1, 3), 0.0f) &&
           color_space_almost_equal(toXYZD50.getFloat(2, 3), 0.0f) &&
           color_space_almost_equal(toXYZD50.getFloat(3, 0), 0.0f) &&
           color_space_almost_equal(toXYZD50.getFloat(3, 1), 0.0f) &&
           color_space_almost_equal(toXYZD50.getFloat(3, 2), 0.0f) &&
           color_space_almost_equal(toXYZD50.getFloat(3, 3), 1.0f);
}

sk_sp<SkColorSpace> SkColorSpace_Base::MakeRGB(SkGammaNamed gammaNamed, const SkMatrix44& toXYZD50,
                                               uint32_t flags)
{
    bool nonLinearBlending = SkToBool(flags & kNonLinearBlending_ColorSpaceFlag);
    switch (gammaNamed) {
        case kSRGB_SkGammaNamed:
            if (xyz_almost_equal(toXYZD50, gSRGB_toXYZD50)) {
                return nonLinearBlending
                    ? SkColorSpace_Base::MakeNamed(kSRGB_NonLinearBlending_Named)
                    : SkColorSpace_Base::MakeNamed(kSRGB_Named);
            }
            break;
        case k2Dot2Curve_SkGammaNamed:
            if (xyz_almost_equal(toXYZD50, gAdobeRGB_toXYZD50) && !nonLinearBlending) {
                return SkColorSpace_Base::MakeNamed(kAdobeRGB_Named);
            }
            break;
        case kLinear_SkGammaNamed:
            if (xyz_almost_equal(toXYZD50, gSRGB_toXYZD50) && !nonLinearBlending) {
                return SkColorSpace_Base::MakeNamed(kSRGBLinear_Named);
            }
            break;
        case kNonStandard_SkGammaNamed:
            // This is not allowed.
            return nullptr;
        default:
            break;
    }

    return sk_sp<SkColorSpace>(new SkColorSpace_XYZ(gammaNamed, toXYZD50, flags));
}

sk_sp<SkColorSpace> SkColorSpace::MakeRGB(RenderTargetGamma gamma, const SkMatrix44& toXYZD50,
                                          uint32_t flags) {
    switch (gamma) {
        case kLinear_RenderTargetGamma:
            return SkColorSpace_Base::MakeRGB(kLinear_SkGammaNamed, toXYZD50, flags);
        case kSRGB_RenderTargetGamma:
            return SkColorSpace_Base::MakeRGB(kSRGB_SkGammaNamed, toXYZD50, flags);
        default:
            return nullptr;
    }
}

sk_sp<SkColorSpace> SkColorSpace::MakeRGB(const SkColorSpaceTransferFn& coeffs,
                                          const SkMatrix44& toXYZD50, uint32_t flags) {
    if (!is_valid_transfer_fn(coeffs)) {
        return nullptr;
    }

    if (is_almost_srgb(coeffs)) {
        return SkColorSpace::MakeRGB(kSRGB_RenderTargetGamma, toXYZD50, flags);
    }

    if (is_almost_2dot2(coeffs)) {
        return SkColorSpace_Base::MakeRGB(k2Dot2Curve_SkGammaNamed, toXYZD50, flags);
    }

    if (is_almost_linear(coeffs)) {
        return SkColorSpace_Base::MakeRGB(kLinear_SkGammaNamed, toXYZD50, flags);
    }

    void* memory = sk_malloc_throw(sizeof(SkGammas) + sizeof(SkColorSpaceTransferFn));
    sk_sp<SkGammas> gammas = sk_sp<SkGammas>(new (memory) SkGammas(3));
    SkColorSpaceTransferFn* fn = SkTAddOffset<SkColorSpaceTransferFn>(memory, sizeof(SkGammas));
    *fn = coeffs;
    SkGammas::Data data;
    data.fParamOffset = 0;
    for (int channel = 0; channel < 3; ++channel) {
        gammas->fType[channel] = SkGammas::Type::kParam_Type;
        gammas->fData[channel] = data;
    }
    return sk_sp<SkColorSpace>(new SkColorSpace_XYZ(kNonStandard_SkGammaNamed,
                                                    std::move(gammas), toXYZD50, nullptr, flags));
}

sk_sp<SkColorSpace> SkColorSpace::MakeRGB(RenderTargetGamma gamma, Gamut gamut, uint32_t flags) {
    SkMatrix44 toXYZD50(SkMatrix44::kUninitialized_Constructor);
    to_xyz_d50(&toXYZD50, gamut);
    return SkColorSpace::MakeRGB(gamma, toXYZD50, flags);
}

sk_sp<SkColorSpace> SkColorSpace::MakeRGB(const SkColorSpaceTransferFn& coeffs, Gamut gamut,
                                          uint32_t flags) {
    SkMatrix44 toXYZD50(SkMatrix44::kUninitialized_Constructor);
    to_xyz_d50(&toXYZD50, gamut);
    return SkColorSpace::MakeRGB(coeffs, toXYZD50, flags);
}

static SkColorSpace* gAdobeRGB;
static SkColorSpace* gSRGB;
static SkColorSpace* gSRGBLinear;
static SkColorSpace* gSRGBNonLinearBlending;

sk_sp<SkColorSpace> SkColorSpace_Base::MakeNamed(Named named) {
    static SkOnce sRGBOnce;
    static SkOnce adobeRGBOnce;
    static SkOnce sRGBLinearOnce;
    static SkOnce sRGBNonLinearBlendingOnce;

    switch (named) {
        case kSRGB_Named: {
            sRGBOnce([] {
                SkMatrix44 srgbToxyzD50(SkMatrix44::kUninitialized_Constructor);
                srgbToxyzD50.set3x3RowMajorf(gSRGB_toXYZD50);

                // Force the mutable type mask to be computed.  This avoids races.
                (void)srgbToxyzD50.getType();
                gSRGB = new SkColorSpace_XYZ(kSRGB_SkGammaNamed, srgbToxyzD50, 0 /* flags */);
            });
            return sk_ref_sp<SkColorSpace>(gSRGB);
        }
        case kAdobeRGB_Named: {
            adobeRGBOnce([] {
                SkMatrix44 adobergbToxyzD50(SkMatrix44::kUninitialized_Constructor);
                adobergbToxyzD50.set3x3RowMajorf(gAdobeRGB_toXYZD50);

                // Force the mutable type mask to be computed.  This avoids races.
                (void)adobergbToxyzD50.getType();
                gAdobeRGB = new SkColorSpace_XYZ(k2Dot2Curve_SkGammaNamed, adobergbToxyzD50,
                                                 0 /* flags */);
            });
            return sk_ref_sp<SkColorSpace>(gAdobeRGB);
        }
        case kSRGBLinear_Named: {
            sRGBLinearOnce([] {
                SkMatrix44 srgbToxyzD50(SkMatrix44::kUninitialized_Constructor);
                srgbToxyzD50.set3x3RowMajorf(gSRGB_toXYZD50);

                // Force the mutable type mask to be computed.  This avoids races.
                (void)srgbToxyzD50.getType();
                gSRGBLinear = new SkColorSpace_XYZ(kLinear_SkGammaNamed, srgbToxyzD50,
                                                   0 /* flags */);
            });
            return sk_ref_sp<SkColorSpace>(gSRGBLinear);
        }
        case kSRGB_NonLinearBlending_Named: {
            sRGBNonLinearBlendingOnce([] {
                SkMatrix44 srgbToxyzD50(SkMatrix44::kUninitialized_Constructor);
                srgbToxyzD50.set3x3RowMajorf(gSRGB_toXYZD50);

                // Force the mutable type mask to be computed.  This avoids races.
                (void)srgbToxyzD50.getType();
                gSRGBNonLinearBlending = new SkColorSpace_XYZ(kSRGB_SkGammaNamed, srgbToxyzD50,
                                                              kNonLinearBlending_ColorSpaceFlag);
            });
            return sk_ref_sp<SkColorSpace>(gSRGBNonLinearBlending);
        }
        default:
            break;
    }
    return nullptr;
}

sk_sp<SkColorSpace> SkColorSpace::MakeSRGB() {
    return SkColorSpace_Base::MakeNamed(SkColorSpace_Base::kSRGB_Named);
}

sk_sp<SkColorSpace> SkColorSpace::MakeSRGBLinear() {
    return SkColorSpace_Base::MakeNamed(SkColorSpace_Base::kSRGBLinear_Named);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool SkColorSpace::gammaCloseToSRGB() const {
    return as_CSB(this)->onGammaCloseToSRGB();
}

bool SkColorSpace::gammaIsLinear() const {
    return as_CSB(this)->onGammaIsLinear();
}

bool SkColorSpace::isNumericalTransferFn(SkColorSpaceTransferFn* fn) const {
    return as_CSB(this)->onIsNumericalTransferFn(fn);
}

bool SkColorSpace::toXYZD50(SkMatrix44* toXYZD50) const {
    const SkMatrix44* matrix = as_CSB(this)->toXYZD50();
    if (matrix) {
        *toXYZD50 = *matrix;
        return true;
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkColorSpace> SkColorSpace_Base::makeWithoutFlags() {
    if (!fFlags) {
        return sk_ref_sp(this);
    }

    SkASSERT(Type::kXYZ == this->type());
    SkColorSpaceTransferFn fn;
    SkAssertResult(this->onIsNumericalTransferFn(&fn));
    return SkColorSpace::MakeRGB(fn, *this->toXYZD50(), 0);
}

sk_sp<SkColorSpace> SkColorSpace_Base::makeWithNonLinearBlending() {
    if (SkToBool(SkColorSpace::kNonLinearBlending_ColorSpaceFlag & fFlags)) {
        return sk_ref_sp(this);
    }

    // This should only be called on XYZ color spaces.  A2B color spaces are never
    // allowed to be destinations - which means that this flag does not make any
    // sense for them.
    SkASSERT(Type::kXYZ == this->type());
    SkColorSpaceTransferFn fn;
    SkAssertResult(this->onIsNumericalTransferFn(&fn));
    return SkColorSpace::MakeRGB(fn, *this->toXYZD50(),
                                 SkColorSpace::kNonLinearBlending_ColorSpaceFlag);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

enum Version {
    k0_Version, // Initial version, header + flags for matrix and profile
};

struct ColorSpaceHeader {
    /**
     *  It is only valid to set zero or one flags.
     *  Setting multiple flags is invalid.
     */

    /**
     *  If kMatrix_Flag is set, we will write 12 floats after the header.
     */
    static constexpr uint8_t kMatrix_Flag     = 1 << 0;

    /**
     *  If kICC_Flag is set, we will write an ICC profile after the header.
     *  The ICC profile will be written as a uint32 size, followed immediately
     *  by the data (padded to 4 bytes).
     */
    static constexpr uint8_t kICC_Flag        = 1 << 1;

    /**
     *  If kTransferFn_Flag is set, we will write 19 floats after the header.
     *  The first seven represent the transfer fn, and the next twelve are the
     *  matrix.
     */
    static constexpr uint8_t kTransferFn_Flag = 1 << 3;

    static ColorSpaceHeader Pack(Version version, uint8_t named, uint8_t gammaNamed,
                                 bool nonLinearBlending, uint8_t flags)
    {
        ColorSpaceHeader header;

        SkASSERT(k0_Version == version);
        header.fVersion = (uint8_t) version;

        SkASSERT(named <= SkColorSpace_Base::kSRGBLinear_Named);
        header.fNamed = (uint8_t) named;

        SkASSERT(gammaNamed <= kNonStandard_SkGammaNamed);
        header.fGammaNamed = (uint8_t) gammaNamed;

        header.fNonLinearBlend = nonLinearBlending ? 1 : 0;

        SkASSERT(flags <= kTransferFn_Flag);
        header.fFlags = flags;
        return header;
    }

    uint8_t fVersion;            // Always zero
    uint8_t fNamed;              // Must be a SkColorSpace::Named
    uint8_t fGammaNamed;         // Must be a SkGammaNamed
    uint8_t fNonLinearBlend : 1; // kNonLinearBlending_ColorSpaceFlag
    uint8_t fFlags          : 7; // Some combination of the flags listed above
};

size_t SkColorSpace::writeToMemory(void* memory) const {
    // Start by trying the serialization fast path.  If we haven't saved ICC profile data,
    // we must have a profile that we can serialize easily.
    if (!as_CSB(this)->fProfileData) {
        // Profile data is mandatory for A2B0 color spaces.
        SkASSERT(SkColorSpace_Base::Type::kXYZ == as_CSB(this)->type());
        const SkColorSpace_XYZ* thisXYZ = static_cast<const SkColorSpace_XYZ*>(this);
        // If we have a named profile, only write the enum.
        const SkGammaNamed gammaNamed = thisXYZ->gammaNamed();
        if (this == gSRGB) {
            if (memory) {
                *((ColorSpaceHeader*) memory) = ColorSpaceHeader::Pack(
                        k0_Version, SkColorSpace_Base::kSRGB_Named, gammaNamed, false, 0);
            }
            return sizeof(ColorSpaceHeader);
        } else if (this == gAdobeRGB) {
            if (memory) {
                *((ColorSpaceHeader*) memory) = ColorSpaceHeader::Pack(
                        k0_Version, SkColorSpace_Base::kAdobeRGB_Named, gammaNamed, false, 0);
            }
            return sizeof(ColorSpaceHeader);
        } else if (this == gSRGBLinear) {
            if (memory) {
                *((ColorSpaceHeader*) memory) = ColorSpaceHeader::Pack(
                        k0_Version, SkColorSpace_Base::kSRGBLinear_Named, gammaNamed, false, 0);
            }
            return sizeof(ColorSpaceHeader);
        } else if (this == gSRGBNonLinearBlending) {
            if (memory) {
                *((ColorSpaceHeader*)memory) = ColorSpaceHeader::Pack(
                        k0_Version, SkColorSpace_Base::kSRGB_NonLinearBlending_Named, gammaNamed,
                        true, 0);
            }
            return sizeof(ColorSpaceHeader);
        }

        // If we have a named gamma, write the enum and the matrix.
        switch (gammaNamed) {
            case kSRGB_SkGammaNamed:
            case k2Dot2Curve_SkGammaNamed:
            case kLinear_SkGammaNamed: {
                if (memory) {
                    *((ColorSpaceHeader*) memory) =
                            ColorSpaceHeader::Pack(k0_Version, 0, gammaNamed,
                                                   thisXYZ->nonLinearBlending(),
                                                   ColorSpaceHeader::kMatrix_Flag);
                    memory = SkTAddOffset<void>(memory, sizeof(ColorSpaceHeader));
                    thisXYZ->toXYZD50()->as3x4RowMajorf((float*) memory);
                }
                return sizeof(ColorSpaceHeader) + 12 * sizeof(float);
            }
            default: {
                const SkGammas* gammas = thisXYZ->gammas();
                SkASSERT(gammas);
                SkASSERT(gammas->isParametric(0));
                SkASSERT(gammas->isParametric(1));
                SkASSERT(gammas->isParametric(2));
                SkASSERT(gammas->data(0) == gammas->data(1));
                SkASSERT(gammas->data(0) == gammas->data(2));

                if (memory) {
                    *((ColorSpaceHeader*) memory) =
                            ColorSpaceHeader::Pack(k0_Version, 0, thisXYZ->fGammaNamed,
                                                   thisXYZ->nonLinearBlending(),
                                                   ColorSpaceHeader::kTransferFn_Flag);
                    memory = SkTAddOffset<void>(memory, sizeof(ColorSpaceHeader));

                    *(((float*) memory) + 0) = gammas->params(0).fA;
                    *(((float*) memory) + 1) = gammas->params(0).fB;
                    *(((float*) memory) + 2) = gammas->params(0).fC;
                    *(((float*) memory) + 3) = gammas->params(0).fD;
                    *(((float*) memory) + 4) = gammas->params(0).fE;
                    *(((float*) memory) + 5) = gammas->params(0).fF;
                    *(((float*) memory) + 6) = gammas->params(0).fG;
                    memory = SkTAddOffset<void>(memory, 7 * sizeof(float));

                    thisXYZ->fToXYZD50.as3x4RowMajorf((float*) memory);
                }

                return sizeof(ColorSpaceHeader) + 19 * sizeof(float);
            }
        }
    }

    // Otherwise, serialize the ICC data.
    size_t profileSize = as_CSB(this)->fProfileData->size();
    if (SkAlign4(profileSize) != (uint32_t) SkAlign4(profileSize)) {
        return 0;
    }

    if (memory) {
        *((ColorSpaceHeader*) memory) = ColorSpaceHeader::Pack(k0_Version, 0,
                                                               kNonStandard_SkGammaNamed,
                                                               as_CSB(this)->nonLinearBlending(),
                                                               ColorSpaceHeader::kICC_Flag);
        memory = SkTAddOffset<void>(memory, sizeof(ColorSpaceHeader));

        *((uint32_t*) memory) = (uint32_t) SkAlign4(profileSize);
        memory = SkTAddOffset<void>(memory, sizeof(uint32_t));

        memcpy(memory, as_CSB(this)->fProfileData->data(), profileSize);
        memset(SkTAddOffset<void>(memory, profileSize), 0, SkAlign4(profileSize) - profileSize);
    }
    return sizeof(ColorSpaceHeader) + sizeof(uint32_t) + SkAlign4(profileSize);
}

sk_sp<SkData> SkColorSpace::serialize() const {
    size_t size = this->writeToMemory(nullptr);
    if (0 == size) {
        return nullptr;
    }

    sk_sp<SkData> data = SkData::MakeUninitialized(size);
    this->writeToMemory(data->writable_data());
    return data;
}

sk_sp<SkColorSpace> SkColorSpace::Deserialize(const void* data, size_t length) {
    if (length < sizeof(ColorSpaceHeader)) {
        return nullptr;
    }

    ColorSpaceHeader header = *((const ColorSpaceHeader*) data);
    data = SkTAddOffset<const void>(data, sizeof(ColorSpaceHeader));
    length -= sizeof(ColorSpaceHeader);
    if (0 == header.fFlags) {
        return SkColorSpace_Base::MakeNamed((SkColorSpace_Base::Named) header.fNamed);
    }

    uint32_t colorSpaceFlags = header.fNonLinearBlend ? kNonLinearBlending_ColorSpaceFlag : 0;

    switch ((SkGammaNamed) header.fGammaNamed) {
        case kSRGB_SkGammaNamed:
        case k2Dot2Curve_SkGammaNamed:
        case kLinear_SkGammaNamed: {
            if (ColorSpaceHeader::kMatrix_Flag != header.fFlags || length < 12 * sizeof(float)) {
                return nullptr;
            }

            SkMatrix44 toXYZ(SkMatrix44::kUninitialized_Constructor);
            toXYZ.set3x4RowMajorf((const float*) data);
            return SkColorSpace_Base::MakeRGB((SkGammaNamed) header.fGammaNamed, toXYZ,
                                              colorSpaceFlags);
        }
        default:
            break;
    }

    switch (header.fFlags) {
        case ColorSpaceHeader::kICC_Flag: {
            if (length < sizeof(uint32_t)) {
                return nullptr;
            }

            uint32_t profileSize = *((uint32_t*) data);
            data = SkTAddOffset<const void>(data, sizeof(uint32_t));
            length -= sizeof(uint32_t);
            if (length < profileSize) {
                return nullptr;
            }

            return MakeICC(data, profileSize);
        }
        case ColorSpaceHeader::kTransferFn_Flag: {
            if (length < 19 * sizeof(float)) {
                return nullptr;
            }

            SkColorSpaceTransferFn transferFn;
            transferFn.fA = *(((const float*) data) + 0);
            transferFn.fB = *(((const float*) data) + 1);
            transferFn.fC = *(((const float*) data) + 2);
            transferFn.fD = *(((const float*) data) + 3);
            transferFn.fE = *(((const float*) data) + 4);
            transferFn.fF = *(((const float*) data) + 5);
            transferFn.fG = *(((const float*) data) + 6);
            data = SkTAddOffset<const void>(data, 7 * sizeof(float));

            SkMatrix44 toXYZ(SkMatrix44::kUninitialized_Constructor);
            toXYZ.set3x4RowMajorf((const float*) data);
            return SkColorSpace::MakeRGB(transferFn, toXYZ, colorSpaceFlags);
        }
        default:
            return nullptr;
    }
}

bool SkColorSpace_Base::EqualsIgnoreFlags(SkColorSpace* src, SkColorSpace* dst) {
    return SkColorSpace::Equals(as_CSB(src)->makeWithoutFlags().get(),
                                as_CSB(dst)->makeWithoutFlags().get());
}

bool SkColorSpace::Equals(const SkColorSpace* src, const SkColorSpace* dst) {
    if (src == dst) {
        return true;
    }

    if (!src || !dst) {
        return false;
    }

    SkData* srcData = as_CSB(src)->fProfileData.get();
    SkData* dstData = as_CSB(dst)->fProfileData.get();
    if (srcData || dstData) {
        if (srcData && dstData) {
            return srcData->size() == dstData->size() &&
                   0 == memcmp(srcData->data(), dstData->data(), srcData->size());
        }

        return false;
    }

    // profiles are mandatory for A2B0 color spaces
    SkASSERT(as_CSB(src)->type() == SkColorSpace_Base::Type::kXYZ);
    const SkColorSpace_XYZ* srcXYZ = static_cast<const SkColorSpace_XYZ*>(src);
    const SkColorSpace_XYZ* dstXYZ = static_cast<const SkColorSpace_XYZ*>(dst);

    if (srcXYZ->gammaNamed() != dstXYZ->gammaNamed()) {
        return false;
    }
    if (srcXYZ->nonLinearBlending() != dstXYZ->nonLinearBlending()) {
        return false;
    }

    switch (srcXYZ->gammaNamed()) {
        case kSRGB_SkGammaNamed:
        case k2Dot2Curve_SkGammaNamed:
        case kLinear_SkGammaNamed:
            if (srcXYZ->toXYZD50Hash() == dstXYZ->toXYZD50Hash()) {
                SkASSERT(*srcXYZ->toXYZD50() == *dstXYZ->toXYZD50() && "Hash collision");
                return true;
            }
            return false;
        default:
            // It is unlikely that we will reach this case.
            sk_sp<SkData> serializedSrcData = src->serialize();
            sk_sp<SkData> serializedDstData = dst->serialize();
            return serializedSrcData->size() == serializedDstData->size() &&
                   0 == memcmp(serializedSrcData->data(), serializedDstData->data(),
                               serializedSrcData->size());
    }
}
