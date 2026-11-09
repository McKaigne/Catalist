#include <doctest/doctest.h>

#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

#include "catalist/cli/commands/commands.hpp"
#include "catalist/settings/settings.hpp"
#include "catalist/storage/database.hpp"
#include "support/sampleLibrary.hpp"
#include "support/testSupport.hpp"

using catalist::test::loadGolden;
using catalist::test::streamCapture;
using catalist::test::tempDirFixture;

namespace {

struct cliRun {
  int exitCode = 0;
  std::string out;
  std::string err;
};

template <typename commandFn>
cliRun runCli(commandFn&& command) {
  cliRun run;
  streamCapture out(std::cout);
  streamCapture err(std::cerr);
  run.exitCode = command();
  run.out = out.text();
  run.err = err.text();
  return run;
}

struct sampleLibraryFixture : tempDirFixture {
  sampleLibraryFixture()
      : db(fileSystem(), clock(), tempDir()),
        harness(db, fileSystem(), clock(), tempDir()) {
    catalist::test::seedSampleLibrary(db);
  }
  catalist::database db;
  catalist::test::cliHarness harness;

  cliRun ls(const std::vector<std::string>& args) {
    return runCli([&] { return catalist::cli::runLs(args, harness.context); });
  }
};

void checkGolden(const std::string& name, const cliRun& run) {
  CHECK(run.exitCode == 0);
  CHECK(run.err == "");
  CHECK(run.out == loadGolden(name, run.out));
}

}  // namespace

// The table layout switches on isatty(STDOUT_FILENO) rather than on the
// captured stream, so the golden file is only meaningful for piped stdout,
// which is how ctest runs it.
TEST_CASE_FIXTURE(sampleLibraryFixture, "golden ls table") {
  if (::isatty(STDOUT_FILENO)) {
    MESSAGE("stdout is a terminal; the piped table layout cannot be checked");
    return;
  }
  checkGolden("lsTable.txt", ls({}));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden ls plain") {
  checkGolden("lsPlain.txt", ls({"--plain"}));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden ls ids") {
  checkGolden("lsIds.txt", ls({"--ids", "--sort", "title"}));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden ls csv") {
  checkGolden("lsCsv.csv", ls({"--csv"}));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden ls json") {
  checkGolden("lsJson.json", ls({"--json", "-t", "anime"}));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden show by id") {
  checkGolden("showById.txt", runCli([&] {
                return catalist::cli::runShow({"52991"}, harness.context);
              }));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden show manga json") {
  checkGolden("showMangaJson.json", runCli([&] {
                return catalist::cli::runShow({"Chainsaw", "--json"}, harness.context);
              }));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden count") {
  checkGolden("count.txt", runCli([&] {
                return catalist::cli::runCount({"-s", "watching"}, harness.context);
              }));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden count json") {
  checkGolden("countJson.json", runCli([&] {
                return catalist::cli::runCount({"--json"}, harness.context);
              }));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden stats json") {
  checkGolden("statsJson.json", runCli([&] {
                return catalist::cli::runStats({"--json"}, harness.context);
              }));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden export json") {
  checkGolden("exportJson.json", runCli([&] {
                return catalist::cli::runExport({"json"}, harness.context);
              }));
}

TEST_CASE_FIXTURE(sampleLibraryFixture, "golden export csv") {
  checkGolden("exportCsv.csv", runCli([&] {
                return catalist::cli::runExport({"csv"}, harness.context);
              }));
}
