// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataBox/ObservationBox.hpp"
#include "DataStructures/DataBox/Tag.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "DataStructures/Variables.hpp"
#include "DataStructures/VariablesTag.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"

namespace {
void multiply_by_two(const gsl::not_null<double*> result, const double value) {
  *result = 2.0 * value;
}

struct Tag0 : db::SimpleTag {
  using type = double;
};

struct Tag1 : db::SimpleTag {
  using type = double;
};

struct Tag1Compute : Tag1, db::ComputeTag {
  using base = Tag1;
  using return_type = double;
  static constexpr auto function = multiply_by_two;
  using argument_tags = tmpl::list<Tag0>;
};

struct Tag2 : db::SimpleTag {
  using type = double;
};

struct Tag2Compute : Tag2, db::ComputeTag {
  using base = Tag2;
  using return_type = double;
  static constexpr auto function = multiply_by_two;
  using argument_tags = tmpl::list<Tag1>;
};

struct Tag3 : db::SimpleTag {
  using type = double;
};

struct Tag3Compute : Tag3, db::ComputeTag {
  using base = Tag3;
  using return_type = double;
  static auto function(const gsl::not_null<double*> result, const double value0,
                       const double value1, const double value2) {
    ++times_called;
    *result = value0 + value1 + value2;
  }
  using argument_tags = tmpl::list<Tag0, Tag1, Tag2>;
  static size_t times_called;
};

size_t Tag3Compute::times_called = 0;

struct ScalarVar : db::SimpleTag {
  using type = Scalar<DataVector>;
};

struct VectorVar : db::SimpleTag {
  using type = tnsr::I<DataVector, 3, Frame::Inertial>;
};

struct VarsCompute : db::ComputeTag,
                     ::Tags::Variables<tmpl::list<ScalarVar, VectorVar>> {
  using base = ::Tags::Variables<tmpl::list<ScalarVar, VectorVar>>;
  using return_type = typename base::type;
  using argument_tags = tmpl::list<>;
  static void function(
      const gsl::not_null<::Variables<tmpl::list<ScalarVar, VectorVar>>*>
          result) {
    result->initialize(5, 1.0);
    for (size_t i = 0; i < 3; ++i) {
      get<VectorVar>(*result).get(i) += static_cast<double>(i) + 1.0;
    }
  }
};

// Test that we can return values from the apply functions and that we can
// retrieve both the nested DataBox and the ObservationBox via the argument
// tags.
struct SubtractNumberAndReturn {
  using argument_tags =
      tmpl::list<::Tags::ObservationBox, ::Tags::DataBox, Tag3>;

  template <typename DbTagsList, typename ComputeTagsList>
  double operator()(
      const ObservationBox<ComputeTagsList, db::DataBox<DbTagsList>>& obs_box,
      const db::DataBox<DbTagsList>& box, const double tag3,
      const double number) const {
    CHECK(get<Tag0>(obs_box) == 2.0);
    CHECK(get<Tag1>(obs_box) == 4.0);
    CHECK(get<Tag2>(obs_box) == (2.0 * 2.0 * 2.0));
    CHECK(get<Tag3>(obs_box) == (2.0 + 2.0 * 2.0 + 2.0 * 2.0 * 2.0));
    CHECK(get<Tag0>(box) == 2.0);
    CHECK(get<Tag1>(box) == 4.0);
    return tag3 - number;
  }
};

struct SubtractNumberAndReturnApply {
  using argument_tags =
      tmpl::list<::Tags::ObservationBox, ::Tags::DataBox, Tag3>;

  template <typename DbTagsList, typename ComputeTagsList>
  static double apply(
      const ObservationBox<ComputeTagsList, db::DataBox<DbTagsList>>& obs_box,
      const db::DataBox<DbTagsList>& box, const double tag3,
      const double number) {
    CHECK(get<Tag0>(obs_box) == 2.0);
    CHECK(get<Tag1>(obs_box) == 4.0);
    CHECK(get<Tag2>(obs_box) == (2.0 * 2.0 * 2.0));
    CHECK(get<Tag3>(obs_box) == (2.0 + 2.0 * 2.0 + 2.0 * 2.0 * 2.0));
    CHECK(get<Tag0>(box) == 2.0);
    CHECK(get<Tag1>(box) == 4.0);
    return tag3 - number;
  }
};

SPECTRE_TEST_CASE("Unit.DataStructures.DataBox.ObservationBox",
                  "[Unit][DataStructures]") {
  const auto db_box =
      db::create<db::AddSimpleTags<Tag0>, db::AddComputeTags<Tag1Compute>>(2.0);
  const auto obs_box = make_observation_box<
      db::AddComputeTags<Tag2Compute, Tag3Compute, VarsCompute>>(db_box);
  CHECK(Tag3Compute::times_called == 0);
  CHECK(get<Tag0>(obs_box) == 2.0);
  CHECK(get<Tag1>(obs_box) == 4.0);
  CHECK(get<Tag2>(obs_box) == (2.0 * 2.0 * 2.0));
  CHECK(get<Tag3>(obs_box) == (2.0 + 2.0 * 2.0 + 2.0 * 2.0 * 2.0));
  CHECK(Tag3Compute::times_called == 1);
  // Call a second time to make sure the compute tag didn't get evaluated again.
  CHECK(get<Tag3>(obs_box) == (2.0 + 2.0 * 2.0 + 2.0 * 2.0 * 2.0));
  CHECK(Tag3Compute::times_called == 1);
  CHECK(&db_box == &get<::Tags::DataBox>(obs_box));
  CHECK(&obs_box == &get<::Tags::ObservationBox>(obs_box));

  CHECK(apply(SubtractNumberAndReturn{}, obs_box, 5.0) ==
        (get<Tag3>(obs_box) - 5.0));
  CHECK(get(get<ScalarVar>(obs_box)) == DataVector(5, 1.0));
  CHECK(get<0>(get<VectorVar>(obs_box)) == DataVector(5, 2.0));
  CHECK(get<1>(get<VectorVar>(obs_box)) == DataVector(5, 3.0));
  CHECK(get<2>(get<VectorVar>(obs_box)) == DataVector(5, 4.0));

  CHECK(apply(SubtractNumberAndReturnApply{}, obs_box, 2.0) ==
        (get<Tag3>(obs_box) - 2.0));
}

}  // namespace
