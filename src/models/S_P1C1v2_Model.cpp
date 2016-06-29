// Copyright (c) 2014-2015, Pacific Biosciences of California, Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted (subject to the limitations in the
// disclaimer below) provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//  * Neither the name of Pacific Biosciences nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
// GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY PACIFIC
// BIOSCIENCES AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL PACIFIC BIOSCIENCES OR ITS
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

// Author: Lance Hepler

#include <cassert>
#include <cmath>
#include <memory>
#include <stdexcept>

#include <pacbio/consensus/ModelConfig.h>
#include <pacbio/consensus/Read.h>

#include "../ModelFactory.h"
#include "../Recursor.h"

namespace PacBio {
namespace Consensus {
namespace {

template <typename T>
inline T clip(const T val, const T range[2])
{
    return std::max(range[0], std::min(val, range[1]));
}

constexpr double kCounterWeight = 20.0;
constexpr size_t nOutcomes = 12;
constexpr size_t nContexts = 16;

class S_P1C1v2_Model : public ModelConfig
{
    REGISTER_MODEL(S_P1C1v2_Model);

public:
    static std::set<std::string> Names() { return {"S/P1-C1.2"}; }
    S_P1C1v2_Model(const SNR& snr);
    std::unique_ptr<AbstractRecursor> CreateRecursor(std::unique_ptr<AbstractTemplate>&& tpl,
                                                     const MappedRead& mr, double scoreDiff) const;
    std::vector<TemplatePosition> Populate(const std::string& tpl) const;
    double ExpectedLLForEmission(MoveType move, uint8_t prev, uint8_t curr,
                                 MomentType moment) const;

private:
    SNR snr_;
    double ctxTrans_[nContexts][4];
    double cachedEmissionExpectations_[nContexts][3][2];
};

REGISTER_MODEL_IMPL(S_P1C1v2_Model);

// TODO(lhepler) comments regarding the CRTP
class S_P1C1v2_Recursor : public Recursor<S_P1C1v2_Recursor>
{
public:
    S_P1C1v2_Recursor(std::unique_ptr<AbstractTemplate>&& tpl, const MappedRead& mr,
                      double scoreDiff);
    static inline std::vector<uint8_t> EncodeRead(const MappedRead& read);
    static inline double EmissionPr(MoveType move, uint8_t emission, uint8_t prev, uint8_t curr);
    virtual double UndoCounterWeights(size_t nEmissions) const;
};

constexpr double snrRanges[4][2] = {{4.001438, 9.300400},    // A
                                    {7.132999, 18.840239},   // C
                                    {4.017619, 9.839173},    // G
                                    {5.553696, 15.040482}};  // T

constexpr double emissionPmf[3][nContexts][nOutcomes] = {
    {// matchPmf
     {0.0538934269, 0.00145009847, 0.000903540653, 5.61451444e-05, 0.0717385523, 0.00138207722,
      0.000983663388, 2.98716179e-05, 0.863881703, 0.00436234248, 0.00108967626, 0.000162001338},
     {0.00440213807, 0.0125860226, 7.60499349e-05, 2.11752707e-05, 0.0153821856, 0.0427745313,
      0.000264941701, 0.000280821504, 0.0178753594, 0.905210325, 0.000105142401, 0.000939700363},
     {0.000540246421, 0.000218767963, 0.01836689, 0.00340121939, 0.000730234523, 3.4737868e-05,
      0.0772714471, 0.00471209957, 0.000843144059, 0.000588970797, 0.870275654, 0.0229348618},
     {0.000111108679, 0.000203758415, 0.00804596969, 0.019115043, 0.000549249541, 9.80612339e-05,
      0.0344882713, 0.051657787, 0.000221063495, 0.000465020805, 0.0610234598, 0.823952168},
     {0.0202789584, 0.00148903221, 0.000597163411, 0.000259954647, 0.064020036, 0.00104431313,
      0.00124614496, 0.00013358629, 0.905761027, 0.0029572048, 0.00200379219, 0.000143327084},
     {0.0402724632, 0.0128518475, 6.42501609e-05, 2.72583767e-05, 0.0132786761, 0.0373069841,
      5.29716119e-05, 9.14398755e-05, 0.0162147808, 0.879564809, 0.000141573402, 5.9945642e-05},
     {0.000201881601, 4.29369365e-05, 0.00728725582, 0.00149461331, 0.000409273215, 0.000167745802,
      0.0346508126, 0.00200385599, 0.000794645188, 0.000287314863, 0.925552349, 0.0270401396},
     {0.000133065007, 7.1321816e-05, 0.00958536688, 0.0187994622, 0.00028633053, 9.12768574e-05,
      0.033314986, 0.0553368214, 0.000384136098, 0.000731762669, 0.0598679875, 0.821318207},
     {0.0106324794, 0.00067658496, 0.000409726657, 8.44568278e-05, 0.0352580824, 0.000480117964,
      0.000394468725, 3.89470939e-05, 0.946170299, 0.00473155911, 0.000572506557, 0.000477970619},
     {0.00197964334, 0.0054988032, 0.000105200025, 1.84904847e-05, 0.00740934367, 0.0233884094,
      0.000111958009, 5.9308563e-05, 0.0117473508, 0.949256624, 0.000233843345, 0.000134414698},
     {0.03469053, 2.12215021e-05, 0.00596709145, 0.000976029849, 0.000399217726, 0.000137996197,
      0.0300521512, 0.00181745646, 0.000422486611, 0.000438039184, 0.901604554, 0.0233980917},
     {0.000413418726, 3.87097781e-05, 0.0051991451, 0.00888802651, 7.01912149e-05, 1.93669472e-05,
      0.0190797047, 0.0304401645, 0.000143986073, 0.00102353111, 0.0393876177, 0.895213831},
     {0.0206986587, 0.00139920374, 0.000577718694, 6.30728439e-05, 0.0638528341, 0.000787266592,
      0.000963775424, 7.07162141e-05, 0.904225376, 0.00531211609, 0.00172552057, 0.000220755728},
     {0.002398519, 0.00597008512, 6.42789108e-05, 4.3328933e-05, 0.00712968535, 0.0222720582,
      0.000281381205, 2.45760734e-05, 0.0109258848, 0.950205967, 9.46913182e-05, 0.000517544635},
     {0.000304324236, 2.30017401e-05, 0.00799304005, 0.00141347855, 0.000168190169, 1.64084167e-05,
      0.0379674798, 0.0021998025, 0.000906751406, 0.000542188155, 0.92718794, 0.0212130746},
     {0.0280311711, 0.00051862335, 0.00593624703, 0.0083684267, 0.00324521337, 0.000994877985,
      0.0173524216, 0.0263269812, 0.0279441, 0.0276693067, 0.0653178223, 0.788237005}},
    {// branchPmf
     {0.305613932, 0.000558996368, 0.000558996368, 0.000558996368, 0.144283703, 0.000558996368,
      0.000558996368, 0.000558996368, 0.542276416, 0.000558996368, 0.000558996368, 0.000558996368},
     {0.000912790048, 0.0706793677, 0.000912790048, 0.000912790048, 0.000912790048, 0.0660077837,
      0.000912790048, 0.000912790048, 0.000912790048, 0.850533788, 0.000912790048, 0.000912790048},
     {0.000370148971, 0.000370148971, 0.263701996, 0.000370148971, 0.000370148971, 0.000370148971,
      0.136580197, 0.000370148971, 0.000370148971, 0.000370148971, 0.594535721, 0.000370148971},
     {0.000992671062, 0.000992671062, 0.000992671062, 0.0903402269, 0.000992671062, 0.000992671062,
      0.000992671062, 0.116624552, 0.000992671062, 0.000992671062, 0.000992671062, 0.779137827},
     {0.153053079, 0.000140879113, 0.000140879113, 0.000140879113, 0.129535583, 0.000140879113,
      0.000140879113, 0.000140879113, 0.71543903, 0.000140879113, 0.000140879113, 0.000140879113},
     {0.000273593053, 0.0390475313, 0.000273026027, 0.000273026027, 0.0002731413, 0.0609617098,
      0.000273026027, 0.000273026027, 0.00027418863, 0.89616655, 0.000273026027, 0.000273026027},
     {0.00024935686, 0.00024935686, 0.224134214, 0.00024935686, 0.00024935686, 0.00024935686,
      0.119989707, 0.00024935686, 0.00024935686, 0.00024935686, 0.652385082, 0.00024935686},
     {0.000703357057, 0.000703357057, 0.000703357057, 0.0774320717, 0.000703357057, 0.000703357057,
      0.000703357057, 0.0881611725, 0.000703357057, 0.000703357057, 0.000703357057, 0.824559757},
     {0.120522857, 0.000164103067, 0.000164103067, 0.000164103067, 0.0827988704, 0.000164103067,
      0.000164103067, 0.000164103067, 0.79438083, 0.000164103067, 0.000164103067, 0.000164103067},
     {0.000324650526, 0.0436389616, 0.000324650526, 0.000324650526, 0.000324650526, 0.0610063298,
      0.000324650526, 0.000324650526, 0.000324650526, 0.890809601, 0.000324650526, 0.000324650526},
     {0.00044702111, 0.000447020831, 0.298059864, 0.000447020831, 0.000447537949, 0.000447020831,
      0.0757672197, 0.000447020831, 0.000447039189, 0.000447020831, 0.619914089, 0.000447020831},
     {0.000460428661, 0.000460428661, 0.000460428661, 0.0445651204, 0.000460428661, 0.000460428661,
      0.000460428661, 0.0472083803, 0.000460428661, 0.000460428661, 0.000460428661, 0.901780498},
     {0.129409727, 0.000180205845, 0.000180205845, 0.000180205845, 0.110681588, 0.000180205845,
      0.000180205845, 0.000180205845, 0.757385803, 0.000180205845, 0.000180205845, 0.000180205845},
     {0.000237587758, 0.0323776588, 0.000237587758, 0.000237587758, 0.000237587758, 0.0435983253,
      0.000237587758, 0.000237587758, 0.000237587758, 0.920697787, 0.000237587758, 0.000237587758},
     {0.000188825576, 0.000188825576, 0.166170812, 0.000188825576, 0.000188825576, 0.000188825576,
      0.0914469645, 0.000188825576, 0.000188825576, 0.000188825576, 0.739738666, 0.000188825576},
     {0.000120613319, 0.000119901068, 0.000119901068, 0.0411298523, 0.00011990536, 0.000119901068,
      0.000119901068, 0.0701345987, 0.000119982783, 0.000119901068, 0.000119901068, 0.887056136}},
    {// stickPmf
     {0.000572145812, 0.0249470981, 0.444128106, 0.0284293982, 0.000572145812, 0.00410024964,
      0.126982064, 0.0141199516, 0.000572145812, 0.0811671877, 0.200116148, 0.0714326301},
     {0.157990255, 0.00032657149, 0.325814692, 0.0288899281, 0.0375283566, 0.00032657149,
      0.132842697, 0.0224909505, 0.0879452518, 0.00032657149, 0.115500492, 0.0883848042},
     {0.358522418, 0.0235568496, 0.000864084339, 0.12651633, 0.052549674, 0.00798327989,
      0.000864084339, 0.0533075108, 0.101824542, 0.0657216859, 0.000864084339, 0.203105036},
     {0.327657509, 0.0162688106, 0.283895663, 0.000488837542, 0.0404076842, 0.00606060934,
      0.121371198, 0.000488837542, 0.073572133, 0.0352442565, 0.0916114349, 0.000488837542},
     {0.000292823503, 0.020723219, 0.422716082, 0.037557393, 0.000292823503, 0.00878801277,
      0.124731093, 0.00806155344, 0.000292823503, 0.225808232, 0.102362669, 0.0469091586},
     {0.138184081, 0.000176275107, 0.353691494, 0.0366734007, 0.0669928396, 0.000160372684,
      0.152689688, 0.0113283557, 0.0933947819, 0.000164997607, 0.112044223, 0.0337002216},
     {0.376363111, 0.0147959899, 0.000406742121, 0.0326187747, 0.124202612, 0.00485744034,
      0.000406742121, 0.0134983573, 0.154095152, 0.172573194, 0.000406742121, 0.103741432},
     {0.262107723, 0.0139625679, 0.198087107, 0.000317282509, 0.0764115908, 0.00637190626,
      0.0728789745, 0.000317282509, 0.104982564, 0.168668474, 0.0939908336, 0.000317282509},
     {0.000568426528, 0.023701166, 0.506335229, 0.0366977326, 0.000568426528, 0.0181752864,
      0.100905875, 0.0188386785, 0.000568426528, 0.103148426, 0.104347954, 0.0833022395},
     {0.122032869, 0.000140837724, 0.364441855, 0.0318879599, 0.0573868815, 0.000140837724,
      0.218659155, 0.0182715104, 0.0477966054, 0.000140837724, 0.0787912025, 0.0596052591},
     {0.396405225, 0.016157559, 0.0022109107, 0.0436250361, 0.107726515, 0.0146690405,
      0.00100899286, 0.00886596365, 0.127007802, 0.0802697981, 0.000874635434, 0.196943448},
     {0.290414877, 0.0177202936, 0.298960183, 0.000526686986, 0.092257921, 0.00700004966,
      0.131147972, 0.000526686986, 0.0859777319, 0.0271600613, 0.0451474144, 0.000526686986},
     {0.000428202504, 0.0191583409, 0.293741471, 0.0182063946, 0.000428202504, 0.0180388612,
      0.128372288, 0.0257390091, 0.000428202504, 0.107512898, 0.199359888, 0.186445229},
     {0.149686594, 0.000207150428, 0.229916753, 0.0144187336, 0.0651607498, 0.000207150428,
      0.0690123099, 0.0156771722, 0.0675998749, 0.000207150428, 0.109695096, 0.277175513},
     {0.403196746, 0.0177901114, 0.000543576149, 0.0343915039, 0.122395417, 0.00275829252,
      0.000543576149, 0.0269263943, 0.0864036291, 0.043786384, 0.000543576149, 0.258002912},
     {0.142268585, 0.0113274953, 0.228643486, 0.000236113134, 0.0585796342, 0.00333756943,
      0.175460415, 0.000247031094, 0.0538439511, 0.0417853213, 0.282875612, 0.00024225267}}};

constexpr double transProbs[nContexts][3][4] = {
    {// AA
     {-10.2090406, 3.32117257, -0.520117212, 0.0249921413},
     {8.9793309, -5.3961519, 0.763613361, -0.0364406433},
     {7.97224482, -3.91644241, 0.485967707, -0.0210642023}},
    {// AC
     {-8.67385002, 1.79692678, -0.189152589, 0.00593875207},
     {-2.06471994, -0.248833169, 0.0239433129, -0.00081901914},
     {1.9844226, -0.834246008, 0.0238906298, 0.000299399008}},
    {// AG
     {-5.57888183, 0.8609919, -0.0601847875, -0.00209638118},
     {-0.0651352822, -0.37813552, -0.116036072, 0.0118044463},
     {10.3924646, -4.87649024, 0.583074371, -0.0230668231}},
    {// AT
     {-6.05918189, 1.12045586, -0.175515022, 0.0081588429},
     {-7.72954717, 1.74329164, -0.228908375, 0.00956968799},
     {0.739498015, -0.820985799, 0.034285628, 0.000308222633}},
    {// CA
     {6.71916863, -4.10669176, 0.61911044, -0.0312872449},
     {-10.7393976, 3.94632964, -0.664301449, 0.0363137581},
     {0.534509761, -0.614208207, -0.0230577035, 0.00504754227}},
    {// CC
     {-11.8487027, 2.18336459, -0.165744338, 0.00381936993},
     {-5.40511615, 0.582166777, -0.0352022429, 0.000649948647},
     {3.62707756, -1.60860162, 0.127123969, -0.00333194156}},
    {// CG
     {-5.33620523, 1.30599458, -0.227619416, 0.0129127127},
     {-8.16537748, 2.82193426, -0.53094601, 0.0319445398},
     {13.0580439, -6.8013044, 0.926568827, -0.0424609205}},
    {// CT
     {12.613656, -5.19052298, 0.561451908, -0.0206707933},
     {-9.73651127, 2.15521846, -0.225290947, 0.00767974576},
     {7.16886431, -2.96320727, 0.2726381, -0.00837654404}},
    {// GA
     {-3.6938331, 0.483533725, -0.0450473465, 0.000160356592},
     {6.68949621, -4.66399411, 0.677757326, -0.0318630437},
     {1.33410944, -0.988217134, -0.00422287463, 0.00562702615}},
    {// GC
     {2.66843035, -1.62797424, 0.147491681, -0.00444976878},
     {1.14199306, -0.843025302, 0.0604509646, -0.00133222978},
     {5.92824303, -2.33995279, 0.175825931, -0.00449046566}},
    {// GG
     {-9.53335176, 2.51765873, -0.318510141, 0.0117320354},
     {-0.524486685, -0.436816275, -0.0926096178, 0.0112705438},
     {8.88135834, -4.79762534, 0.664134538, -0.0311007622}},
    {// GT
     {3.35914891, -1.38173016, 0.0781044221, -0.000763136872},
     {10.8476498, -4.75169784, 0.525334182, -0.0193477301},
     {15.5403876, -5.8956994, 0.604296436, -0.0209965825}},
    {// TA
     {5.16029404, -3.68434804, 0.621640606, -0.0350676726},
     {11.8825308, -7.04640014, 1.07909276, -0.0537996376},
     {-9.20746548, 4.71271573, -0.951489582, 0.0565839306}},
    {// TC
     {2.73609906, -1.3992629, 0.120116348, -0.00349409007},
     {-4.54672948, 0.344140535, -0.0129698225, -0.000227430524},
     {0.840535522, -1.10879024, 0.0800657995, -0.00206209905}},
    {// TG
     {-6.45925912, 1.63128009, -0.226062028, 0.0099607621},
     {-8.86160161, 3.12245384, -0.583442174, 0.033990528},
     {3.46514973, -1.91203442, 0.12119972, 0.00103341906}},
    {// TT
     {2.70097834, -1.96301816, 0.246186023, -0.00988186677},
     {1.80568256, -1.56446979, 0.176114086, -0.00665610868},
     {1.91982573, -1.50191002, 0.153230363, -0.00550115233}}};

inline double CalculateExpectedLLForEmission(const size_t move, const uint8_t row,
                                             const size_t moment)
{
    double expectedLL = 0;
    for (size_t i = 0; i < nOutcomes; i++) {
        double curProb = emissionPmf[move][row][i];
        double lgCurProb = std::log(curProb);
        if (moment == static_cast<uint8_t>(MomentType::FIRST))
            expectedLL += curProb * lgCurProb;
        else if (moment == static_cast<uint8_t>(MomentType::SECOND))
            expectedLL += curProb * (lgCurProb * lgCurProb);
    }
    return expectedLL;
}

S_P1C1v2_Model::S_P1C1v2_Model(const SNR& snr) : snr_(snr)
{
    for (size_t ctx = 0; ctx < nContexts; ++ctx) {
        const uint8_t bp = ctx & 3;  // (equivalent to % 4)
        const double snr1 = clip(snr_[bp], snrRanges[bp]), snr2 = snr1 * snr1, snr3 = snr2 * snr1;
        double sum = 1.0;

        // cached transitions
        ctxTrans_[ctx][0] = 1.0;
        for (size_t j = 0; j < 3; ++j) {
            double xb = transProbs[ctx][j][0] + snr1 * transProbs[ctx][j][1] +
                        snr2 * transProbs[ctx][j][2] + snr3 * transProbs[ctx][j][3];
            xb = std::exp(xb);
            ctxTrans_[ctx][j + 1] = xb;
            sum += xb;
        }
        for (size_t j = 0; j < 4; ++j)
            ctxTrans_[ctx][j] /= sum;

        // cached expectations
        for (size_t move = 0; move < 3; ++move)
            for (size_t moment = 0; moment < 2; ++moment)
                cachedEmissionExpectations_[ctx][move][moment] =
                    CalculateExpectedLLForEmission(move, ctx, moment);
    }
}

std::unique_ptr<AbstractRecursor> S_P1C1v2_Model::CreateRecursor(
    std::unique_ptr<AbstractTemplate>&& tpl, const MappedRead& mr, double scoreDiff) const
{
    return std::unique_ptr<AbstractRecursor>(
        new S_P1C1v2_Recursor(std::forward<std::unique_ptr<AbstractTemplate>>(tpl), mr, scoreDiff));
}

std::vector<TemplatePosition> S_P1C1v2_Model::Populate(const std::string& tpl) const
{
    std::vector<TemplatePosition> result;

    if (tpl.empty()) return result;

    result.reserve(tpl.size());

    // calculate transition probabilities
    uint8_t prev = detail::TranslationTable[static_cast<uint8_t>(tpl[0])];
    if (prev > 3) throw std::invalid_argument("invalid character in template!");

    for (size_t i = 1; i < tpl.size(); ++i) {
        const uint8_t curr = detail::TranslationTable[static_cast<uint8_t>(tpl[i])];
        if (curr > 3) throw std::invalid_argument("invalid character in template!");
        const auto row = (prev << 2) | curr;
        const auto params = ctxTrans_[row];
        result.emplace_back(TemplatePosition{
            tpl[i - 1], prev,
            params[0],  // match
            params[1],  // branch
            params[2],  // stick
            params[3]   // deletion
        });
        prev = curr;
    }
    result.emplace_back(TemplatePosition{tpl.back(), prev, 1.0, 0.0, 0.0, 0.0});

    return result;
}

double S_P1C1v2_Model::ExpectedLLForEmission(const MoveType move, const uint8_t prev,
                                             const uint8_t curr, const MomentType moment) const
{
    const size_t row = (prev << 2) | curr;
    return cachedEmissionExpectations_[row][static_cast<uint8_t>(move)]
                                      [static_cast<uint8_t>(moment)];
}

S_P1C1v2_Recursor::S_P1C1v2_Recursor(std::unique_ptr<AbstractTemplate>&& tpl, const MappedRead& mr,
                                     double scoreDiff)
    : Recursor<S_P1C1v2_Recursor>(std::forward<std::unique_ptr<AbstractTemplate>>(tpl), mr,
                                  scoreDiff)
{
}

std::vector<uint8_t> S_P1C1v2_Recursor::EncodeRead(const MappedRead& read)
{
    std::vector<uint8_t> result;

    result.reserve(read.Length());

    for (size_t i = 0; i < read.Length(); ++i) {
        if (read.PulseWidth[i] < 1U) throw std::runtime_error("invalid PulseWidth in read!");
        const uint8_t pw = std::min(2, read.PulseWidth[i] - 1);
        const uint8_t bp = detail::TranslationTable[static_cast<uint8_t>(read.Seq[i])];
        if (bp > 3) throw std::invalid_argument("invalid character in read!");
        const uint8_t em = (pw << 2) | bp;
        if (em > 11) throw std::runtime_error("read encoding error!");
        result.emplace_back(em);
    }

    return result;
}

double S_P1C1v2_Recursor::EmissionPr(MoveType move, uint8_t emission, uint8_t prev, uint8_t curr)
{
    assert(move != MoveType::DELETION);
    const auto row = (prev << 2) | curr;
    return emissionPmf[static_cast<uint8_t>(move)][row][emission] * kCounterWeight;
}

double S_P1C1v2_Recursor::UndoCounterWeights(const size_t nEmissions) const
{
    return -std::log(kCounterWeight) * nEmissions;
}
}  // namespace anonymous
}  // namespace Consensus
}  // namespace PacBio
