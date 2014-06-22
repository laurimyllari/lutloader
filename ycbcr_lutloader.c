#include "lcms2.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// borrow some lcms2 internals - probably a bad idea?
extern cmsToneCurve** _cmsStageGetPtrToCurveSet(const cmsStage* mpe);

typedef struct
{
    cmsUInt16Number prevval[3];
    int valcount[3];
} countcargo_t;

cmsInt32Number lutSizeCounter(register const cmsUInt16Number In[],
        register cmsUInt16Number Out[],
        register void * Cargo)
{
    countcargo_t *c = (countcargo_t*)Cargo;

    for (int d = 0; d < 3; d++)
    {
        if (In[d] > c->prevval[d])
        {
            c->prevval[d] = In[d];
            c->valcount[d]++;
        }
    }
    return 1;
}

typedef struct
{
    cmsFloat32Number *lut;  // lut output data
    int s1, s2, s3;         // lut dimensions
} fillcargo_t;

cmsInt32Number lutFiller(register const cmsFloat32Number In[],
        register cmsFloat32Number Out[],
        register void * Cargo)
{
    fillcargo_t *f = (fillcargo_t*)Cargo;
    int x = In[0] * f->s1;
    int y = In[1] * f->s2;
    int z = In[2] * f->s3;
    int index = 3 * (z*f->s2 + y*f->s1 + x);
    f->lut[index] = Out[0];
    f->lut[index+1] = Out[1];
    f->lut[index+2] = Out[2];
    return 1;
}

int main(int  argc, char* argv[])
{
    cmsHPROFILE hProfile;
    cmsPipeline *pipeline;
    cmsStage *inputstage, *clutstage, *outputstage;
    cmsToneCurve **inputcurves, **outputcurves;
    countcargo_t countcargo;
    int outputtablesize;
    float *outputtable[3];
    fillcargo_t fillcargo;
    int lutsize[3];
    float *lut;

    if (argc != 2) {
        printf("ycbcr_lotloader <YCbCr->RGB icc device link>\n");
        return 1;
    }

    hProfile = cmsOpenProfileFromFile(argv[1], "r");
    if (cmsGetDeviceClass(hProfile) != cmsSigLinkClass) {
        printf("expected device link profile\n");
        return 1;
    }

    if (!cmsIsTag(hProfile, cmsSigAToB0Tag)) {
        printf("expected to find AToB0 tag\n");
        return 1;
    }

    pipeline = cmsReadTag(hProfile, cmsSigAToB0Tag);

    if (!cmsPipelineCheckAndRetreiveStages(
            pipeline,
            3,
            cmsSigCurveSetElemType, cmsSigCLutElemType, cmsSigCurveSetElemType,
            &inputstage, &clutstage, &outputstage)) {
        printf("expected <table, clut, table> in AToB0\n");
        return 1;
    }

    // to use YCbCr source encoding with collink, curves must be disabled
    inputcurves = _cmsStageGetPtrToCurveSet(inputstage);

    for (int c = 0; c < 3; c++ )
        if (!cmsIsToneCurveLinear(inputcurves[c]))
            {
                printf("expected linear input table\n");
                return 1;
            }

    // It is possible that output curve has been added with applycal. This
    // gives two options for including device calibration: -a to collink
    // (calibration is applied to the 3DLUT?) or applycal. Would the latter
    // give better precision?
    outputcurves = _cmsStageGetPtrToCurveSet(outputstage);
    if (cmsIsToneCurveLinear(outputcurves[0])
            && cmsIsToneCurveLinear(outputcurves[1])
            && cmsIsToneCurveLinear(outputcurves[2]))
    {
        printf("output curves seem linear and could be ignored\n");
        outputtablesize = 16;
    }
    else
    {
        // Use the same size as lcms2 uses for its estimated representation
        outputtablesize = cmsGetToneCurveEstimatedTableEntries(outputcurves[0]);
        printf("using output table size %d\n", outputtablesize);
    }

#ifdef DEBUG
    for (int c = 0; c < 3; c++ )
    {
        printf("outputcurves[%d]:\n" \
                "\tisMultisegment: %s\n" \
                "\tisLinear: %s\n" \
                "\tisMonotonic: %s\n" \
                "\tisDescending: %s\n" \
                "\tgammaEstimate: %f\n" \
                "\testimatedTableEntries: %d\n",
                c,
                cmsIsToneCurveMultisegment(outputcurves[c]) ? "true" : "false",
                cmsIsToneCurveLinear(outputcurves[c]) ? "true" : "false",
                cmsIsToneCurveMonotonic(outputcurves[c]) ? "true" : "false",
                cmsIsToneCurveDescending(outputcurves[c]) ? "true" : "false",
                cmsEstimateGamma(outputcurves[c], 0.01),
                cmsGetToneCurveEstimatedTableEntries(outputcurves[c]));
    }
#endif

    for (int c = 0; c < 3; c++ )
    {
        outputtable[c] = malloc(outputtablesize * sizeof(float));
        for (int x = 0; x < outputtablesize; x++)
        {
            outputtable[c][x] = cmsEvalToneCurveFloat(
                    outputcurves[c],
                    (float)x / (outputtablesize-1));
        }
    }

#ifdef DEBUG
    for (int x = 0; x < outputtablesize; x++)
    {
        printf("%2d: ", x);
        for (int c = 0; c < 3; c++ )
            printf(" %0.4f", outputtable[c][x]);
        printf("\n");
    }
#endif

    // this is a little convoluted, but lcms2 doesn't give us the LUT size?
    memset(&countcargo, 0, sizeof(countcargo_t));
    cmsStageSampleCLut16bit(clutstage, lutSizeCounter, &countcargo, SAMPLER_INSPECT);
    lutsize[0] = countcargo.valcount[0]; lutsize[1] = countcargo.valcount[1]; lutsize[2] = countcargo.valcount[2];
    printf("lut size %dx%dx%d\n",
            lutsize[0],
            lutsize[1],
            lutsize[2]);

    lut = malloc(lutsize[0] * lutsize[1] * lutsize[2] * 3 * sizeof(float));
    fillcargo.lut = lut;
    fillcargo.s1 = lutsize[0];
    fillcargo.s2 = lutsize[1];
    fillcargo.s3 = lutsize[2];
    cmsStageSampleCLutFloat(clutstage, lutFiller, &fillcargo, SAMPLER_INSPECT);



    for (int c = 0; c < 3; c++ )
    {
        free(outputtable[c]);
    }
    free(lut);

    cmsCloseProfile(hProfile);

    return 0;
}
