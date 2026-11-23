#include "support/sample_library.hpp"

#include "support/test_support.hpp"

namespace catalist::test {

std::vector<Entry> SampleList(MediaType type) {
	if (type == MediaType::kManga)
		return {MakeEntry()
		            .Id(1)
		            .MalId(2)
		            .Title("Berserk")
		            .Type(MediaType::kManga)
		            .Status(WatchStatus::kWatching)
		            .Score(10)
		            .Progress(364)
		            .Volumes(42)
		            .StartDate("2019-05-01")
		            .UpdatedAt("2026-10-17"),
		        MakeEntry()
		            .Id(2)
		            .MalId(116778)
		            .Title("Chainsaw Man")
		            .Type(MediaType::kManga)
		            .Status(WatchStatus::kOnHold)
		            .Score(9)
		            .Progress(97)
		            .TotalUnits(97)
		            .Volumes(11)
		            .TotalVolumes(11)
		            .UpdatedAt("2026-10-17")};
	return {MakeEntry()
	            .Id(1)
	            .MalId(52991)
	            .Title("Sousou no Frieren")
	            .Status(WatchStatus::kCompleted)
	            .Score(10)
	            .Progress(28)
	            .TotalUnits(28)
	            .Rewatched(1)
	            .StartDate("2023-09-29")
	            .FinishDate("2024-03-22")
	            .UpdatedAt("2026-10-17"),
	        MakeEntry()
	            .Id(2)
	            .MalId(44265)
	            .Title("Odd Taxi")
	            .Status(WatchStatus::kWatching)
	            .Score(8)
	            .Progress(5)
	            .TotalUnits(13)
	            .StartDate("2021-04-06")
	            .UpdatedAt("2026-10-17"),
	        MakeEntry()
	            .Id(3)
	            .Title("A Very Long Title That Will Definitely Be Truncated")
	            .Status(WatchStatus::kPlanToWatch)
	            .UpdatedAt("2026-10-18"),
	        MakeEntry()
	            .Id(4)
	            .Title("Ghost, \"in\" the Shell")
	            .Status(WatchStatus::kDropped)
	            .Score(6)
	            .Progress(3)
	            .StartDate("2020-01-01")
	            .UpdatedAt("2026-10-18")};
}

}  // namespace catalist::test
