#include "doctest.h"
#include "formats/ne.h"
#include "ne_builder.h"

using namespace is::fmt;

TEST_CASE("NE resource table: integer and named entries, shifted offsets") {
    auto img = build_ne("TESTMOD", {
        {RT16_STRING, 2, "", string_block({"a", "b"})},
        {RT16_ACCELERATOR, 0, "MAINACCEL", {0x81, 0x41, 0, 0x10, 0}},
        {4000, 60, "", std::vector<std::uint8_t>(40, 7)},
    });
    NeModule m(img);
    CHECK(m.module_name() == "TESTMOD");
    CHECK(m.align_shift() == 4);
    REQUIRE(m.resources().size() == 3);
    const NeResource* s = m.find(RT16_STRING, 2);
    REQUIRE(s);
    CHECK(s->offset % 16 == 0);
    CHECK(m.data(*s)[0] == 1);
    CHECK(m.find(RT16_ACCELERATOR, "MAINACCEL"));
    const NeResource* c = m.find(4000, 60);
    REQUIRE(c);
    CHECK(m.size(*c) == 48);  // rounded up to 16
    CHECK_FALSE(m.find(4000, 61));
    CHECK(m.all(4000).size() == 1);
}

TEST_CASE("NE reader rejects other formats") {
    std::vector<std::uint8_t> notmz(64, 0);
    CHECK_THROWS_AS(NeModule{notmz}, FormatError);
    auto img = build_ne("X", {});
    img[0x41] = 'X';
    CHECK_THROWS_AS(NeModule{img}, FormatError);
}
