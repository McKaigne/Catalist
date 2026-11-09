#include "support/sampleLibrary.hpp"

#include "support/testSupport.hpp"

namespace catalist::test {

void seedSampleLibrary(database& db) {
  db.addEntry(makeEntry().id(52991).malId(52991).anilistId(154587)
                  .title("Sousou no Frieren").status(watchStatus::kCompleted)
                  .score(10).progress(28).totalUnits(28).rewatched(1)
                  .startDate("2023-09-29").finishDate("2024-03-22")
                  .notes("Masterpiece adaptation.").updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(44265).malId(44265).title("Odd Taxi")
                  .status(watchStatus::kWatching).score(8).progress(5)
                  .totalUnits(13).startDate("2021-04-06")
                  .notes("Line one\nLine \"two\", with comma")
                  .updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(10001)
                  .title("A Very Long Title That Will Definitely Be Truncated")
                  .status(watchStatus::kPlanToWatch).updatedAt("2026-10-18"));
  db.addEntry(makeEntry().id(10002).title("Ghost, \"in\" the Shell")
                  .status(watchStatus::kDropped).score(6).progress(3)
                  .startDate("2020-01-01").updatedAt("2026-10-18"));
  db.addEntry(makeEntry().id(2).malId(2).anilistId(30002).title("Berserk")
                  .type(mediaType::kManga).status(watchStatus::kWatching)
                  .score(10).progress(364).volumes(42)
                  .startDate("2019-05-01").notes("Dark fantasy pinnacle.")
                  .updatedAt("2026-10-17"));
  db.addEntry(makeEntry().id(116778).malId(116778).title("Chainsaw Man")
                  .type(mediaType::kManga).status(watchStatus::kOnHold)
                  .score(9).progress(97).totalUnits(97).volumes(11)
                  .totalVolumes(11).updatedAt("2026-10-17"));
}

}  // namespace catalist::test
