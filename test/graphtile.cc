#include "test.h"

#include "baldr/graphtile.h"

using namespace std;
using namespace valhalla::baldr;

namespace {

void TestFileSuffix() {
  TileHierarchy h("/data/valhalla");

  if(GraphTile::FileSuffix(GraphId(2, 2, 0), h) != "2/000/000/002.gph")
    throw std::runtime_error("Unexpected graphtile suffix");

  if(GraphTile::FileSuffix(GraphId(4, 2, 0), h) != "2/000/000/004.gph")
    throw std::runtime_error("Unexpected graphtile suffix");

  if(GraphTile::FileSuffix(GraphId(6897468, 2, 0), h) != "2/006/897/468.gph")
    throw std::runtime_error("Unexpected graphtile suffix");

  if(GraphTile::FileSuffix(GraphId(64799, 1, 0), h) != "1/064/799.gph")
    throw std::runtime_error("Unexpected graphtile suffix");

  if(GraphTile::FileSuffix(GraphId(49, 0, 0), h) != "0/000/049.gph")
    throw std::runtime_error("Unexpected graphtile suffix");
}

}

int main() {
  test::suite suite("graphtile");

  suite.test(TEST_CASE(TestFileSuffix));

  return suite.tear_down();
}
