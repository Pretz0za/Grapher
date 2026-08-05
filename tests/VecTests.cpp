// Port of tests/core/gvizVecTests.c against gviz::Vec* (Vec.hpp). Mechanical
// rename port: same scenarios, same tolerances, gvizVecX -> gviz::VecX.

#include "Vec.hpp"

#include "unity/unity.h"

#include <cmath>
#include <initializer_list>

void setUp(void) {}
void tearDown(void) {}

static void assertVecNear(size_t n, const double *a, const double *b,
                           double tol) {
  for (size_t i = 0; i < n; i++)
    TEST_ASSERT_DOUBLE_WITHIN(tol, b[i], a[i]);
}

// Overload taking a brace-init-list so call sites can write the expected
// vector inline (standard C++ has no C-style compound-literal array syntax).
static void assertVecNear(size_t n, const double *a,
                           std::initializer_list<double> expected,
                           double tol) {
  assertVecNear(n, a, expected.begin(), tol);
}

static void test_VecCopyAndDot(void) {
  double a[3] = {1.0, 2.0, 3.0};
  double b[3] = {4.0, 5.0, 6.0};
  double out[3];

  gviz::VecCopy(3, a, out);
  assertVecNear(3, out, a, 1e-12);
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 32.0, gviz::VecDot(3, a, b));
}

static void test_VecNormAndScale(void) {
  double v[2] = {3.0, 4.0};
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, gviz::VecNorm2(2, v));
  gviz::VecScale(2, 0.5, v);
  assertVecNear(2, v, {1.5, 2.0}, 1e-12);
}

static void test_VecAxpy(void) {
  double x[2] = {1.0, 2.0};
  double y[2] = {3.0, 4.0};
  gviz::VecAxpy(2, 2.0, x, y);
  assertVecNear(2, y, {5.0, 8.0}, 1e-12);
}

static void test_VecAccKKForce(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {3.0, 4.0};
  double acc[2] = {0.0, 0.0};
  gviz::VecAccKKForce(2, v, u, 5.0, acc);
  assertVecNear(2, acc, {0.0, 0.0}, 1e-12);

  acc[0] = acc[1] = 0.0;
  gviz::VecAccKKForce(2, v, u, 10.0, acc);
  assertVecNear(2, acc, {-1.5, -2.0}, 1e-12);
}

static void test_VecAccGRIPFRRepForce(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {2.0, 0.0};
  double acc[2] = {0.0, 0.0};
  gviz::VecAccGRIPFRRepForce(2, v, u, 1.0, acc);
  assertVecNear(2, acc, {8.0, 0.0}, 1e-12);
}

static void test_VecAccGRIPFRAttForce(void) {
  double v[2] = {2.0, 0.0};
  double u[2] = {0.0, 0.0};
  double acc[2] = {0.0, 0.0};
  gviz::VecAccGRIPFRAttForce(2, v, u, 1.0, 0.05, acc);
  assertVecNear(2, acc, {0.025, 0.0}, 1e-12);
}

static void test_VecAccFRAttForce(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {3.0, 4.0};
  double acc[2] = {0.0, 0.0};
  gviz::VecAccFRAttForce(2, v, u, 5.0, acc);
  assertVecNear(2, acc, {3.0, 4.0}, 1e-12);
}

static void test_VecAccFRRepForce(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {2.0, 0.0};
  double acc[2] = {0.0, 0.0};
  gviz::VecAccFRRepForce(2, v, u, 1.0, 0.0, 100.0, acc);
  assertVecNear(2, acc, {-0.5, 0.0}, 1e-12);
}

/* radiusSum shifts the effective distance from 2.0 (center distance) to 1.0
 * (center distance minus the two vertices' combined radii): gap = 2-1 = 1 > 0
 * still takes the divide branch, so mag = k^2/gap = 1/1 = 1.0 along (1,0). */
static void test_VecAccFRRepForce_radiusSumShrinksEffectiveDistance(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {2.0, 0.0};
  double acc[2] = {0.0, 0.0};
  gviz::VecAccFRRepForce(2, v, u, 1.0, 1.0, 100.0, acc);
  assertVecNear(2, acc, {-1.0, 0.0}, 1e-12);
}

/* Once radiusSum exceeds the center distance (gap = 1-2 = -1 <= 0), the
 * circles overlap and the magnitude saturates at k^2 * overlapConstant
 * instead of dividing by the vanishing gap: mag = 1*10 = 10 along (1,0). */
static void test_VecAccFRRepForce_overlapUsesConstantForce(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {1.0, 0.0};
  double acc[2] = {0.0, 0.0};
  gviz::VecAccFRRepForce(2, v, u, 1.0, 2.0, 10.0, acc);
  assertVecNear(2, acc, {-10.0, 0.0}, 1e-12);
}

/* Two exactly coincident points give no direction to normalize; the force is
 * still finite and its magnitude is the saturated overlap value k^2 *
 * overlapConstant, in some deterministic pseudo-random direction. */
static void test_VecAccFRRepForce_coincidentIsFiniteAndBounded(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {0.0, 0.0};
  double acc[2] = {0.0, 0.0};
  double k = 2.0, overlapConstant = 10.0;
  gviz::VecAccFRRepForce(2, v, u, k, 1.0, overlapConstant, acc);
  TEST_ASSERT_TRUE(std::isfinite(acc[0]));
  TEST_ASSERT_TRUE(std::isfinite(acc[1]));
  double norm = sqrt(acc[0] * acc[0] + acc[1] * acc[1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, k * k * overlapConstant, norm);
}

static void test_VecDim4(void) {
  double a[4] = {1.0, 2.0, 3.0, 4.0};
  double b[4] = {2.0, 0.0, 1.0, 3.0};
  double out[4];

  gviz::VecCopy(4, a, out);
  assertVecNear(4, out, a, 1e-12);
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 17.0, gviz::VecDot(4, a, b));
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, sqrt(30.0), gviz::VecNorm2(4, a));

  gviz::VecZero(4, out);
  assertVecNear(4, out, {0.0, 0.0, 0.0, 0.0}, 1e-12);

  gviz::VecAxpy(4, 2.0, a, out);
  assertVecNear(4, out, {2.0, 4.0, 6.0, 8.0}, 1e-12);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_VecCopyAndDot);
  RUN_TEST(test_VecNormAndScale);
  RUN_TEST(test_VecAxpy);
  RUN_TEST(test_VecAccKKForce);
  RUN_TEST(test_VecAccGRIPFRRepForce);
  RUN_TEST(test_VecAccGRIPFRAttForce);
  RUN_TEST(test_VecAccFRAttForce);
  RUN_TEST(test_VecAccFRRepForce);
  RUN_TEST(test_VecAccFRRepForce_radiusSumShrinksEffectiveDistance);
  RUN_TEST(test_VecAccFRRepForce_overlapUsesConstantForce);
  RUN_TEST(test_VecAccFRRepForce_coincidentIsFiniteAndBounded);
  RUN_TEST(test_VecDim4);
  return UNITY_END();
}
