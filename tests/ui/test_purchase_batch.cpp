#include <doctest.h>
#include "UI/NPCs/PurchaseBatch.h"

using UI::NPCs::PurchaseBatch;

TEST_CASE("merchant purchases allow only one request until its acknowledgement [ui][npc]")
{
    PurchaseBatch batch;
    REQUIRE(batch.Begin(0, 42, 3));
    CHECK(batch.Send());
    CHECK_FALSE(batch.Send());
    CHECK_FALSE(batch.Begin(1, 99, 2));
    CHECK(batch.Confirmed() == 0);
    CHECK(batch.Answer(true));
    CHECK(batch.Confirmed() == 1);
    CHECK(batch.Send());
    CHECK(batch.Answer(true));
    CHECK(batch.Send());
    CHECK(batch.Answer(true));
    CHECK_FALSE(batch.Active());
    CHECK_FALSE(batch.Answer(true));
    CHECK(batch.Confirmed() == 3);
}

TEST_CASE("stopping merchant batches keeps only the request already in flight [ui][npc]")
{
    PurchaseBatch batch;
    REQUIRE(batch.Begin(119, 42, 100));
    CHECK(batch.Send());
    batch.Stop();
    CHECK(batch.Active());
    CHECK(batch.Requested() == 1);
    CHECK(batch.Answer(true));
    CHECK_FALSE(batch.Active());
    CHECK_FALSE(batch.Send());
    REQUIRE(batch.Begin(0, 99, 2));
    CHECK(batch.Send());
    CHECK(batch.Answer(true));
    batch.Stop();
    CHECK_FALSE(batch.Active());
    CHECK(batch.Confirmed() == 1);
}

TEST_CASE("refusal and invalid purchase input cannot generate further requests [ui][npc]")
{
    PurchaseBatch batch;
    CHECK_FALSE(batch.Begin(-1, 42, 1));
    CHECK_FALSE(batch.Begin(120, 42, 1));
    CHECK_FALSE(batch.Begin(0, 0, 1));
    CHECK_FALSE(batch.Begin(0, 42, 0));
    CHECK_FALSE(batch.Begin(0, 42, 101));
    REQUIRE(batch.Begin(0, 42, 3));
    CHECK(batch.Send());
    CHECK(batch.Answer(true));
    CHECK(batch.Send());
    CHECK(batch.Answer(false));
    CHECK_FALSE(batch.Active());
    CHECK_FALSE(batch.Send());
    CHECK(batch.Confirmed() == 1);
}
