#include <ceres/ceres.h>

#include "geometry/xform.h"

using namespace Eigen;
using namespace xform;

struct Dynamics3DPlus {
  template<typename T>
  bool operator()(const T* _x, const T* _delta, T* _xp) const
  {
    Xform<T> x(_x);
    Map<const Matrix<T,6,1>> d(_delta);
    Xform<T> xp(_xp);
    xp = x + d;

    Map<const Matrix<T,3,1>> v(_x+7);
    Map<const Matrix<T,3,1>> dv(_delta+6);
    Map<Matrix<T,3,1>> vp(_xp+7);
    vp = v + dv;

    return true;
  }
};
typedef ceres::AutoDiffLocalParameterization<Dynamics3DPlus, 10, 9> Dynamics3DPlusParamAD;


class Dyn3dFunctor
{
public:
  Dyn3dFunctor(double dt, double& cost):
    cost_{cost}
  {
    dt_ = dt;
  }

  template <typename T>
  bool operator() (const T* _x0, const T* _x1, const T* _u0, const T* _u1, T* res) const
  {

    typedef Matrix<T,3,1> Vec3;
    typedef Matrix<T,9,1> Vec9;
    Map<Vec9> r(res);

    r.setZero();

    Map<Vec3> pr(res);
    Map<Vec3> qr(res+3);
    Map<Vec3> vr(res+6);

    Map<const Vec3> p0(_x0);
    Map<const Vec3> p1(_x1);
    Quat<T> q0(_x0+3);
    Quat<T> q1(_x1+3);
    Map<const Vec3> v0(_x0+7);
    Map<const Vec3> v1(_x0+7);

    Map<const Vec3> w0(_u0);
    Map<const Vec3> w1(_u0);
    const T& F0 (*(_u0+3));
    const T& F1 (*(_u1+3));

    static const Vec3 e_z = (Vec3() << (T)0, (T)0, (T)1).finished();
    static const Vec3 gravity = (Vec3() << (T)0, (T)0, (T)9.80665).finished();

    Vec3 dv0 = (T)-2.0*9.80665*e_z*F0 - (T)0.9*v0 + q0.rotp(gravity);// - w0.cross(v0);
    Vec3 dv1 = (T)-2.0*9.80665*e_z*F1 - (T)0.9*v1 + q1.rotp(gravity);// - w1.cross(v1);

    pr = p1 - (p0 + (T)0.5 * (T)dt_ * (q0.rotp(v0) + q1.rotp(v1)));
    qr = q1 - (q0 + (T)0.5 * (T)dt_ * (w0 + w1));
    vr = v1 - (v0 + (T)0.5 * (T)dt_ * (dv0 + dv1));
//    pr = p1 - (p0 + dt_ * q0.rotp(v0));
//    qr = q1 - (q0 + dt_ * w0);
//    vr = v1 - (v0 + dt_ * dv0);

    r = cost_ * r;
    return true;
  }
  double dt_;
  double& cost_;
};
typedef ceres::AutoDiffCostFunction<Dyn3dFunctor, 9, 10, 10, 4, 4> Dyn3DFactorAD;


class PosVel3DFunctor
{
public:
  PosVel3DFunctor(Ref<Vector3d> p, double vmag, double& cost) :
    cost_(cost)
  {
    p_ = p;
    vmag_ = vmag;
  }
  template <typename T>
  bool operator() (const T* x, T* res) const
  {
    typedef Matrix<T,3,1> Vec3;

    Map<const Vec3> p(x);
    Map<const Vec3> v(x+7);

    Map<Vec3> pr(res);
    T& vr(*(res+3));

    pr = cost_*(p - p_);
    vr = cost_*(v.array().abs().sum() - (T)vmag_);

    return true;
  }

  Vector3d p_;
  double vmag_;
  double& cost_;
};
typedef ceres::AutoDiffCostFunction<PosVel3DFunctor, 4, 10> PosVel3DFactorAD;


class Input3DFunctor
{
public:
  Input3DFunctor(Matrix4d& R):
    R_(R.data())
  {}

  template <typename T>
  bool operator() (const T* _u, T* res) const
  {
    typedef Matrix<T,4,1> Vec4;
    Map<const Vec4> u(_u);
    Map<Vec4> r(res);

    static const Vec4 eq_input_ = (Vec4() << (T)0, (T)0, (T)0, (T)0.5).finished();
    r = R_*(u - eq_input_);
    return true;
  }
  Map<Matrix4d> R_;
};
typedef ceres::AutoDiffCostFunction<Input3DFunctor, 4, 4> Input3DFactorAD;
