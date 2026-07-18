#include "core/gvizVec.h"
#include "unity/unity.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

static void assertVecNear(size_t n, const double *a, const double *b,
                          double tol) {
  for (size_t i = 0; i < n; i++)
    TEST_ASSERT_DOUBLE_WITHIN(tol, b[i], a[i]);
}

void test_gvizVecCopyAndDot(void) {
  double a[3] = {1.0, 2.0, 3.0};
  double b[3] = {4.0, 5.0, 6.0};
  double out[3];

  gvizVecCopy(3, a, out);
  assertVecNear(3, out, a, 1e-12);
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 32.0, gvizVecDot(3, a, b));
}

void test_gvizVecNormAndScale(void) {
  double v[2] = {3.0, 4.0};
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, gvizVecNorm2(2, v));
  gvizVecScale(2, 0.5, v);
  assertVecNear(2, v, (double[]){1.5, 2.0}, 1e-12);
}

void test_gvizVecAxpy(void) {
  double x[2] = {1.0, 2.0};
  double y[2] = {3.0, 4.0};
  gvizVecAxpy(2, 2.0, x, y);
  assertVecNear(2, y, (double[]){5.0, 8.0}, 1e-12);
}

void test_gvizVecAccKKForce(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {3.0, 4.0};
  double acc[2] = {0.0, 0.0};
  gvizVecAccKKForce(2, v, u, 5.0, acc);
  assertVecNear(2, acc, (double[]){0.0, 0.0}, 1e-12);

  acc[0] = acc[1] = 0.0;
  gvizVecAccKKForce(2, v, u, 10.0, acc);
  assertVecNear(2, acc, (double[]){-1.5, -2.0}, 1e-12);
}

void test_gvizVecAccGRIPFRRepForce(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {2.0, 0.0};
  double acc[2] = {0.0, 0.0};
  gvizVecAccGRIPFRRepForce(2, v, u, 1.0, acc);
  assertVecNear(2, acc, (double[]){8.0, 0.0}, 1e-12);
}

void test_gvizVecAccGRIPFRAttForce(void) {
  double v[2] = {2.0, 0.0};
  double u[2] = {0.0, 0.0};
  double acc[2] = {0.0, 0.0};
  gvizVecAccGRIPFRAttForce(2, v, u, 1.0, 0.05, acc);
  assertVecNear(2, acc, (double[]){0.025, 0.0}, 1e-12);
}

void test_gvizVecAccFRAttForce(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {3.0, 4.0};
  double acc[2] = {0.0, 0.0};
  gvizVecAccFRAttForce(2, v, u, 5.0, acc);
  assertVecNear(2, acc, (double[]){3.0, 4.0}, 1e-12);
}

void test_gvizVecAccFRRepForce(void) {
  double v[2] = {0.0, 0.0};
  double u[2] = {2.0, 0.0};
  double acc[2] = {0.0, 0.0};
  gvizVecAccFRRepForce(2, v, u, 1.0, acc);
  assertVecNear(2, acc, (double[]){-0.5, 0.0}, 1e-12);
}

void test_gvizVecDim4(void) {
  double a[4] = {1.0, 2.0, 3.0, 4.0};
  double b[4] = {2.0, 0.0, 1.0, 3.0};
  double out[4];

  gvizVecCopy(4, a, out);
  assertVecNear(4, out, a, 1e-12);
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 17.0, gvizVecDot(4, a, b));
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, sqrt(30.0), gvizVecNorm2(4, a));

  gvizVecZero(4, out);
  assertVecNear(4, out, (double[]){0.0, 0.0, 0.0, 0.0}, 1e-12);

  gvizVecAxpy(4, 2.0, a, out);
  assertVecNear(4, out, (double[]){2.0, 4.0, 6.0, 8.0}, 1e-12);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_gvizVecCopyAndDot);
  RUN_TEST(test_gvizVecNormAndScale);
  RUN_TEST(test_gvizVecAxpy);
  RUN_TEST(test_gvizVecAccKKForce);
  RUN_TEST(test_gvizVecAccGRIPFRRepForce);
  RUN_TEST(test_gvizVecAccGRIPFRAttForce);
  RUN_TEST(test_gvizVecAccFRAttForce);
  RUN_TEST(test_gvizVecAccFRRepForce);
  RUN_TEST(test_gvizVecDim4);
  return UNITY_END();
}
