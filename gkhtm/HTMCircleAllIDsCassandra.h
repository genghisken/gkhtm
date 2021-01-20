#include <sstream>
#include <iomanip>
#include <map>
#include "SpatialInterface.h"

//std::string htmCircleRegion (size_t level, double ra, double dec, double radius, std::string columns, std::string table_name, size_t sublevel1, size_t sublevel2);
//std::string htmCircleRegionSet (size_t level, double ra, double dec, double radius, std::string columns, std::string table_name, size_t sublevel1, size_t sublevel2);
//std::vector<std::string> htmCircleRegionCassandraSetMultiQuery (size_t level, double ra, double dec, double radius, std::string columns, std::string table_name, size_t sublevel1, size_t sublevel2);
std::vector<std::string> htmCircleRegionCassandraMultiQuery (size_t deepestlevel, double ra, double dec, double radius, std::string columns, std::string table_name, size_t sublevels = 2, size_t step = 3, bool whereOnly = false);
std::vector<std::string> htmCircleRegionCassandra (double ra, double dec, double radius);
