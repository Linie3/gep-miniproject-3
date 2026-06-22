#ifndef TEST_SUMMATOR_H
#define TEST_SUMMATOR_H

#include "tests/test_macros.h"

#include "modules/summator/summator.h"

namespace TestSummator {

TEST_CASE("[Modules][Summator] Adding numbers") {
	Ref<Summator> s = memnew(Summator);
	CHECK(s->get_total() == 0);

	s->add(10);
	CHECK(s->get_total() == 10);

	s->add(20);
	CHECK(s->get_total() == 30);

	s->add(30);
	CHECK(s->get_total() == 60);

	s->reset();
	CHECK(s->get_total() == 0);
}

} // namespace TestSummator

#endif // TEST_SUMMATOR_H