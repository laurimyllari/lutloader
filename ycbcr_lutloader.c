#include "lcms2.h"
#include <math.h>
#include <stdlib.h>

// borrow some lcms2 internals - probably a bad idea?
extern cmsToneCurve** _cmsStageGetPtrToCurveSet(const cmsStage* mpe);

int main(int  argc, char* argv[])
{
    cmsHPROFILE hProfile;
    cmsPipeline *pipeline;
    cmsStage *inputstage, *clutstage, *outputstage;
    cmsToneCurve **inputcurves, **outputcurves;
    float *outputtable[3];
    int outputtablesize;

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

    for (int c = 0; c < 3; c++ )
    {
        free(outputtable[c]);
    }

    cmsCloseProfile(hProfile);

    return 0;
}
