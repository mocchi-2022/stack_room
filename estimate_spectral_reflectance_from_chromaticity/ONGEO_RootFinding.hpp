#define NOMINMAX
//#include "ONGEO.h"
#include <cmath>
 
// Brent法を用いて根を求める。区間 ti1 - ti2 の中で単調増加、または単調減少であること。
// @param ti1 [in] 狭めたい区間の下限
// @param ti2 [in] 狭めたい区間の上限 ti1 < ti2であること。
// @return 根となる値
// Reference : R.P.Brent,
//   "An algorithm with guaranteed convergence for finding a zero of a function".
//    The Computer Journal, 14(1971), pp.422-425.
template <typename EvalFunc>
double ONGEO_FindRootByBrentMethod(double ti1, double ti2, EvalFunc f, double tolerance) {
    double a = ti1, b = ti2;
    double fa = f(a);
    double fb = f(b);
    double c = a, fc = fa, d, dprev;
    d = dprev = b - a;
    // d : 収束処理でbを動かす量
    for(;;){
        // 根に近い方をb,fb、遠い方をa=c,fa=fcとする。
        if (std::abs(fc) < std::abs(fb)){
            a = b, b = c, c = a;
            fa = fb, fb = fc, fc = fa;
        }
        double tol = 2.0 * DBL_EPSILON * std::abs(b) + tolerance;
        double c_b = c - b;
        double m = 0.5 * c_b;
        if (std::abs(m) <= tol || fb == 0) break;
 
        // bisectionの場合のd、dprev
        d = dprev = m;
        if (std::abs(dprev) >= tol && std::abs(fa) > std::abs(fb)){
            double r3 = fb / fa;
            double p, q;
            if (a == c){
                // linear interpolation
                p = c_b * r3;
                q = 1.0 - r3;
            }else{
                // inverse quadratic interpolation
                double ifc = 1.0 / fc;
                double r1 = fa * ifc, r2 = fb * ifc;
                p = r3 * (c_b * r1 * (r1 - r2) - (b - a) * (r2 - 1));
                q = (r1 - 1) * (r2 - 1) * (r3 - 1);
            }
            if (p > 0) q = -q;
            else p = -p;
            if (2.0 * p < 1.5 * c_b * q - std::abs(tol * q) && p < std::abs(0.5 * dprev * q)){
                // 上記条件に合う場合は、dとdprevをlinear interpolation、
                // またはinverse quadratic interpolation由来のものに書き換える。
                dprev = d;
                d = p / q;
            }
        }
        // a, fa, b, fbを更新
        a = b, fa = fb;
        b += (std::abs(d) > tol) ? d : ((m > 0) ? tol : -tol);
        fb = f(b);
        if (fb * fc > 0){
            c = a, fc = fa;
            dprev = b - a;
        }
    }
    return b;
}
