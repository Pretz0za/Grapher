// Port of the small, self-contained gvizForceModel vtable
// (embedders/gvizForceModel.h/.c) into the gviz::layout::ForceModel class
// hierarchy (ForceModel.hpp). No embedder consumes this yet -- ForceAtlas
// (the port of gvizForceEmbedder) is a separate, later M4 task -- so this
// suite only exercises ForceModel in isolation: sane vertex masses, sane
// force directions/magnitudes, and that virtual dispatch actually reaches
// the right override when both models are held polymorphically.

#include "ForceModel.hpp"

#include "unity/unity.h"

#include <cmath>
#include <memory>
#include <vector>

using gviz::layout::ForceModel;
using gviz::layout::FruchtermanReingold;
using gviz::layout::LinLog;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// VERTEX MASS
// ============================================================================

static void test_fr_vertexMass_isConstantOne(void) {
  FruchtermanReingold fr;
  TEST_ASSERT_EQUAL_DOUBLE(1.0, fr.VertexMass(0));
  TEST_ASSERT_EQUAL_DOUBLE(1.0, fr.VertexMass(1));
  TEST_ASSERT_EQUAL_DOUBLE(1.0, fr.VertexMass(100));
}

static void test_linlog_vertexMass_growsWithDegree(void) {
  LinLog ll;
  TEST_ASSERT_EQUAL_DOUBLE(1.0, ll.VertexMass(0));
  TEST_ASSERT_EQUAL_DOUBLE(2.0, ll.VertexMass(1));
  TEST_ASSERT_EQUAL_DOUBLE(11.0, ll.VertexMass(10));
}

// ============================================================================
// FRUCHTERMAN-REINGOLD FORCES
// ============================================================================

static void test_fr_attractive_pullsTogether(void) {
  FruchtermanReingold fr;
  double v[2] = {0.0, 0.0};
  double u[2] = {10.0, 0.0};
  double acc[2] = {0.0, 0.0};

  fr.Attractive(2, v, u, 5.0, acc);

  // Attraction on v pulls it toward u, i.e. in +x.
  TEST_ASSERT_TRUE(acc[0] > 0.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, acc[1]);
}

static void test_fr_repulsive_pushesApart(void) {
  FruchtermanReingold fr;
  double v[2] = {5.0, 0.0};
  double other[2] = {0.0, 0.0};
  double acc[2] = {0.0, 0.0};

  // vMass is deliberately ignored by FR's Repulsive -- pass a nonsense value
  // to make sure it really has no effect (see the header's design note).
  fr.Repulsive(2, v, other, /*vMass=*/999.0, /*otherMass=*/1.0,
               /*vRadius=*/0.0, /*otherRadius=*/0.0, /*overlapConstant=*/1.0,
               /*edgeLength=*/5.0, acc);

  // Repulsion on v pushes it away from other, i.e. in +x (v is at +x
  // already).
  TEST_ASSERT_TRUE(acc[0] > 0.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, acc[1]);
}

static void test_fr_repulsive_ignoresVMass(void) {
  FruchtermanReingold fr;
  double v[2] = {5.0, 0.0};
  double other[2] = {0.0, 0.0};
  double accLowMass[2] = {0.0, 0.0};
  double accHighMass[2] = {0.0, 0.0};

  fr.Repulsive(2, v, other, /*vMass=*/1.0, /*otherMass=*/3.0, 0.0, 0.0, 1.0,
               5.0, accLowMass);
  fr.Repulsive(2, v, other, /*vMass=*/1000.0, /*otherMass=*/3.0, 0.0, 0.0, 1.0,
               5.0, accHighMass);

  TEST_ASSERT_DOUBLE_WITHIN(1e-9, accLowMass[0], accHighMass[0]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, accLowMass[1], accHighMass[1]);
}

static void test_fr_repulsive_scalesWithOtherMass(void) {
  FruchtermanReingold fr;
  double v[2] = {5.0, 0.0};
  double other[2] = {0.0, 0.0};
  double accMass1[2] = {0.0, 0.0};
  double accMass4[2] = {0.0, 0.0};

  fr.Repulsive(2, v, other, 1.0, /*otherMass=*/1.0, 0.0, 0.0, 1.0, 5.0,
               accMass1);
  fr.Repulsive(2, v, other, 1.0, /*otherMass=*/4.0, 0.0, 0.0, 1.0, 5.0,
               accMass4);

  // VecAccFRRepForceWeighted scales linearly with the "other" mass.
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0 * accMass1[0], accMass4[0]);
}

// ============================================================================
// LINLOG FORCES
// ============================================================================

static void test_linlog_attractive_pullsTogether(void) {
  LinLog ll;
  double v[2] = {0.0, 0.0};
  double u[2] = {10.0, 0.0};
  double acc[2] = {0.0, 0.0};

  // edgeLength is ignored by LinLog's attraction; pass a nonsense value.
  ll.Attractive(2, v, u, 999.0, acc);

  TEST_ASSERT_TRUE(acc[0] > 0.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, acc[1]);
}

static void test_linlog_repulsive_pushesApart(void) {
  LinLog ll;
  double v[2] = {5.0, 0.0};
  double other[2] = {0.0, 0.0};
  double acc[2] = {0.0, 0.0};

  ll.Repulsive(2, v, other, /*vMass=*/2.0, /*otherMass=*/3.0, 0.0, 0.0, 1.0,
               5.0, acc);

  TEST_ASSERT_TRUE(acc[0] > 0.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, acc[1]);
}

static void test_linlog_repulsive_scalesWithMassProduct(void) {
  LinLog ll;
  double v[2] = {5.0, 0.0};
  double other[2] = {0.0, 0.0};
  double acc1x1[2] = {0.0, 0.0};
  double acc2x3[2] = {0.0, 0.0};

  ll.Repulsive(2, v, other, 1.0, 1.0, 0.0, 0.0, 1.0, 5.0, acc1x1);
  ll.Repulsive(2, v, other, 2.0, 3.0, 0.0, 0.0, 1.0, 5.0, acc2x3);

  // VecAccLinLogRepForce's magnitude is massProduct/gap: 1*1 vs 2*3 -> 6x.
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0 * acc1x1[0], acc2x3[0]);
}

// ============================================================================
// POLYMORPHISM
// ============================================================================

static void test_polymorphism_dispatchesToCorrectOverride(void) {
  std::vector<std::unique_ptr<ForceModel>> models;
  models.push_back(std::make_unique<FruchtermanReingold>());
  models.push_back(std::make_unique<LinLog>());

  // FR: constant mass 1.0 regardless of degree.
  TEST_ASSERT_EQUAL_DOUBLE(1.0, models[0]->VertexMass(7));
  // LinLog: 1 + degree.
  TEST_ASSERT_EQUAL_DOUBLE(8.0, models[1]->VertexMass(7));

  double v[2] = {0.0, 0.0};
  double u[2] = {10.0, 0.0};
  double accFr[2] = {0.0, 0.0};
  double accLinLog[2] = {0.0, 0.0};
  models[0]->Attractive(2, v, u, 5.0, accFr);
  models[1]->Attractive(2, v, u, 5.0, accLinLog);

  // Both pull v toward u (+x), but FR (d^2/k) and LinLog (1/sqrt(d)) give
  // different magnitudes for the same 10-unit separation -- confirms the
  // reference dispatched to two genuinely different implementations rather
  // than the same one twice.
  TEST_ASSERT_TRUE(accFr[0] > 0.0);
  TEST_ASSERT_TRUE(accLinLog[0] > 0.0);
  TEST_ASSERT_TRUE(std::fabs(accFr[0] - accLinLog[0]) > 1e-6);
}

static void test_polymorphism_referenceDispatch(void) {
  FruchtermanReingold fr;
  LinLog ll;

  const ForceModel &asFr = fr;
  const ForceModel &asLinLog = ll;

  TEST_ASSERT_EQUAL_DOUBLE(1.0, asFr.VertexMass(50));
  TEST_ASSERT_EQUAL_DOUBLE(51.0, asLinLog.VertexMass(50));
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_fr_vertexMass_isConstantOne);
  RUN_TEST(test_linlog_vertexMass_growsWithDegree);

  RUN_TEST(test_fr_attractive_pullsTogether);
  RUN_TEST(test_fr_repulsive_pushesApart);
  RUN_TEST(test_fr_repulsive_ignoresVMass);
  RUN_TEST(test_fr_repulsive_scalesWithOtherMass);

  RUN_TEST(test_linlog_attractive_pullsTogether);
  RUN_TEST(test_linlog_repulsive_pushesApart);
  RUN_TEST(test_linlog_repulsive_scalesWithMassProduct);

  RUN_TEST(test_polymorphism_dispatchesToCorrectOverride);
  RUN_TEST(test_polymorphism_referenceDispatch);

  return UNITY_END();
}
