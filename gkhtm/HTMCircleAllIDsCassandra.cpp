//============================================================================
// Name        : HTMCircle.cpp
// Author      : Ken Smith
// Version     :
// Copyright   :
// Description : Prints out the HTM IDs for a given level, ra, dec and radius
//               Assumes column name is always htm<level>ID
//============================================================================

#include "HTMCircleAllIDsCassandra.h"
#include <set>

/*
For primary levels <= HTM10, just generate the ID list.
For deep levels > HTM10 generate HTM10 IN clause, then generate deep level suffix
depending on how many levels required.  Let's hardwire two sublevels.  If the second
is zero, skip the sublevel.

For the API - we'll use:
HTM10 ~ 400 triangles r <= 1800" (0.5 degree radius).  Nothing allowed beyond that.
HTM13 ~ 250 triangles 15" < r < 200"
HTM16 ~ 250 triangles r <= 15"


NOTE: There is a fundamental problem with this approach if we don't clean up the RA/Dec pairs
      after data return.  If there are more than one triangle prefixes, then we will be returning
      suffix combinations for triangles which are incorrect.  E.g.  Take a simple case where a
      search radius straddles 2 HTM10 triangles.  Now we collect the suffixes into a sub clause,
      which means we will return data in full triangles that are NOT in the full triangle list.

In a normal relational database we'd get something like:

./HTMCircleAllIDs 16 1.0 0.0 4.0 tcs_transient_objects 1
select * from tcs_transient_objects where htm16ID IN (
'S00000002120120100',
'S00000002120120103',
'S00000002120121200',
'S00000002120123200',
'N32000001210210200',
'N32000001210210203',
'N32000001210212100',
'N32000001210213100');

But do this with cassandra subclauses and we get this:

./HTMCircleAllIDsCassandra 16 1.0 0.0 4.0 tcs_transient_objects 13 16
select * from tcs_transient_objects where htm10 IN (
'N32000001210','S00000002120')
AND (htm13,htm16) IN
(('120','100'),
 ('120','103'),
 ('121','200'),
 ('123','200'),
 ('210','200'),
 ('210','203'),
 ('212','100'),
 ('213','100'));

Fine - but now you can see that we are pulling out data from twice the number of triangles.
The htm10 prefixes are attached to htm13,htm16 suffixes from the wrong hemisphere.

We can always clean up the data afterwards.  But we need to compare time for that with time
for doing multiple single queries.  E.g. what we ACTUALLY want is to split the query into
individual htm10 queries:

select * from tcs_transient_objects where htm10 IN (
'N32000001210')
AND (htm13,htm16) IN
 ('210','200'),
 ('210','203'),
 ('212','100'),
 ('213','100'));

PLUS

select * from tcs_transient_objects where htm10 IN (
'S00000002120')
AND (htm13,htm16) IN
(('120','100'),
 ('120','103'),
 ('121','200'),
 ('123','200'));

Also - bear in mind that since htm10 is a partition key, having Cassandra cross more than one
node in a single query (and combining results internally) might be much less efficient than
just providing 2 consecutive queries which we know will be tied to individual nodes.

*/
/*
std::string htmCircleRegionCassandra (size_t level, double ra, double dec, double radius, std::string columns, std::string table_name, size_t sublevel1, size_t sublevel2)
{
  stringstream s;

  string name;
  string name10;
  string sub1;
  string sub2;
  string query;

  // create level index required
  htmInterface htm(level, 5);

  std::vector<htmRange> list=htm.circleRegion(ra, dec, radius);

  std::vector<string> htm10s;
  std::vector<string> htmsuffix1;
  std::vector<string> htmsuffix2;

  query = "select " + columns + " from " + table_name + " where htm" + to_string(level) + " IN (\n";

  for(size_t idx=0; idx<list.size(); idx++){
      for(long htmID = list[idx].lo; htmID <= list[idx].hi; htmID++){
          name = htm.lookupName(htmID);
          if (level > 10)
          {
              if (sublevel1 > 10)
              {
                  sub1 = name.substr(10+2, sublevel1 - 10);
                  if (sublevel2 > sublevel1)
                  {
                      sub2 = name.substr(sublevel1+2, sublevel2-sublevel1);
                      name10 = name.substr(0,2+10);
                      htm10s.push_back(name10);
                      htmsuffix1.push_back(sub1);
                      htmsuffix2.push_back(sub2);
                      query += "'" + name10 + "','" + sub1 + "','" + sub2 + "'";
                  }
              } 
          }
          else
          {
              query += "'" + name + "'";
          }
          

          if (htmID != list[idx].hi) query += ",";
          }
      if (idx != list.size() - 1) query += ",";
  }
  query += ");\n";

  s << "select " << columns << " from " << table_name << " where htm10 IN ";
  s << "(";
  for(size_t idx=0; idx<htm10s.size() - 1; idx++)
  {
      // Print out the HTM10s
      s << "'" << htm10s[idx] << "',";
  }
  s << "'" << htm10s.back() << "'";
  s << ")" << endl;

  s << "AND (htm" << sublevel1 << ",htm" << sublevel2 << ") IN" << endl;
  s << "(";
  for(size_t idx=0; idx<htmsuffix1.size() - 1; idx++)
  {
      // Print out the sub HTMs
      s << "('" << htmsuffix1[idx] << "','" << htmsuffix2[idx] << "'),"; 
  }
  s << "('" << htmsuffix1.back() << "','" << htmsuffix2.back() << "')"; 

  s << ");" << endl;
  return s.str();
}

// Same function again, but this time use std::set. Should reduce the size of our query.
// I need to double-check that the result of this query is THE SAME as the one above.
std::string htmCircleRegionCassandraSet (size_t level, double ra, double dec, double radius, std::string columns, std::string table_name, size_t sublevel1, size_t sublevel2)
{
  stringstream s;

  string name;
  string name10;
  string sub1;
  string sub2;
  string query;

  // create level index required
  htmInterface htm(level, 5);

  std::vector<htmRange> list=htm.circleRegion(ra, dec, radius);

  std::set<string> htm10s;
  std::vector<string> htmsuffix1;
  std::vector<string> htmsuffix2;

  query = "select " + columns + " from " + table_name + " where htm" + to_string(level) + " IN (\n";

  for(size_t idx=0; idx<list.size(); idx++){
      for(long htmID = list[idx].lo; htmID <= list[idx].hi; htmID++){
          name = htm.lookupName(htmID);
          if (level > 10)
          {
              if (sublevel1 > 10)
              {
                  sub1 = name.substr(10+2, sublevel1 - 10);
                  if (sublevel2 > sublevel1)
                  {
                      sub2 = name.substr(sublevel1+2, sublevel2-sublevel1);
                      name10 = name.substr(0,2+10);
                      htm10s.insert(name10);
                      htmsuffix1.push_back(sub1);
                      htmsuffix2.push_back(sub2);
                      query += "'" + name10 + "','" + sub1 + "','" + sub2 + "'";
                  }
              } 
          }
          else
          {
              query += "'" + name + "'";
          }
          

          if (htmID != list[idx].hi) query += ",";
          }
      if (idx != list.size() - 1) query += ",";
  }
  query += ");\n";

  s << "select " << columns << " from " << table_name << " where htm10 IN ";
  s << "(";

  // Pull out everything from the set except the last element. There's always at least one.
  std::set<string>::iterator it;
  for(it = htm10s.begin(); it != --htm10s.end(); ++it)
  {
      // Print out the HTM10s
      s << "'" << *it << "',";
  }
  s << "'" << *it << "'";
  s << ")" << endl;

  s << "AND (htm" << sublevel1 << ",htm" << sublevel2 << ") IN" << endl;
  s << "(";
  for(size_t idx=0; idx<htmsuffix1.size() - 1; idx++)
  {
      // Print out the sub HTMs
      s << "('" << htmsuffix1[idx] << "','" << htmsuffix2[idx] << "'),"; 
  }
  s << "('" << htmsuffix1.back() << "','" << htmsuffix2.back() << "')"; 

  s << ");" << endl;
  return s.str();
}

*/

/*
// The following returns one Cassandra query per htm10 triangle.  This may prove more efficient
// than cleaning the RA/Dec pairs after the superset of results has been returned. Also good to
// test total results.
std::vector<std::string> htmCircleRegionCassandraSetMultiQuery (size_t level, double ra, double dec, double radius, std::string columns, std::string table_name, size_t sublevel1, size_t sublevel2)
{
  //stringstream s;

  string name;
  string name10;
  string sub1;
  string sub2;
  string query;

  // create level index required
  htmInterface htm(level, 5);

  std::vector<htmRange> list=htm.circleRegion(ra, dec, radius);
  std::vector<std::string> queries;

  std::set<string> htm10s;
  std::vector<string> htmsuffix1;
  std::vector<string> htmsuffix2;

  //query = "select * from " + table_name + " where htm" + to_string(level) + " IN (\n";

  for(size_t idx=0; idx<list.size(); idx++){
      for(long htmID = list[idx].lo; htmID <= list[idx].hi; htmID++){
          name = htm.lookupName(htmID);
          if (level > 10)
          {
              if (sublevel1 > 10)
              {
                  sub1 = name.substr(10+2, sublevel1 - 10);
                  if (sublevel2 > sublevel1)
                  {
                      sub2 = name.substr(sublevel1+2, sublevel2-sublevel1);
                      name10 = name.substr(0,2+10);
                      htm10s.insert(name10);
                      htmsuffix1.push_back(sub1);
                      htmsuffix2.push_back(sub2);
                      //query += "'" + name10 + "','" + sub1 + "','" + sub2 + "'";
                  }
                  else if (sublevel2 == sublevel1 && sublevel1 == level) // just push onto sublevel1
                  {
                      name10 = name.substr(0,2+10);
                      htm10s.insert(name10);
                      htmsuffix1.push_back(sub1);
                  }
              } 
          }
          else
          {
              //query += "'" + name + "'";
              htm10s.insert(name);
          }
          

          if (htmID != list[idx].hi) query += ",";
          }
      if (idx != list.size() - 1) query += ",";
  }
  //query += ");\n";


  // Pull out everything from the set except the last element. There's always at least one.
  std::set<string>::iterator it;

  if (level > 10)
  {
      for(it = htm10s.begin(); it != htm10s.end(); ++it)
      {
          stringstream s;
          // Print out the HTM10s
          s << "select " << columns << " from " << table_name << " where htm10 IN ";
          s << "(";
          s << "'" << *it << "'";
          s << ")" << endl;

          if (level > 10)
          {
              if (sublevel2 > sublevel1)
              {
                  s << "AND (htm" << sublevel1 << ",htm" << sublevel2 << ") IN" << endl;
                  s << "(";
                  for(size_t idx=0; idx<htmsuffix1.size() - 1; idx++)
                  {
                      // Print out the sub HTMs
                      s << "('" << htmsuffix1[idx] << "','" << htmsuffix2[idx] << "'),"; 
                  }
                  s << "('" << htmsuffix1.back() << "','" << htmsuffix2.back() << "')"; 
              }
              else if (sublevel2 == sublevel1 && sublevel1 == level) // just push onto sublevel1
              {
                  s << "AND htm" << sublevel1 <<  " IN" << endl;
                  s << "(";
                  for(size_t idx=0; idx<htmsuffix1.size() - 1; idx++)
                  {
                      // Print out the sub HTMs
                      s << "'" << htmsuffix1[idx] <<  "',"; 
                  }
                  s << "'" << htmsuffix1.back() << "'"; 
              }

          }

          s << ");" << endl;
          queries.push_back(s.str());
      }
  }
  else
  {
      // Just return a single statement with all the HTM10 IDs.
      stringstream s;
      s << "select " << columns << " from " << table_name << " where htm10 IN ";
      s << "(";
      for(it = htm10s.begin(); it != --htm10s.end(); ++it)
      {
          // Print out the HTM10s
          s << "'" << *it << "',";
      }
      // Print out the last one
      s << "'" << *it << "'";
      s << ");" << endl;
      queries.push_back(s.str());
  }

  return queries;
}

*/

/*
int main(int argc, char *argv[]) {
    if (argc != 9)
    {
        cerr << "Usage: " << argv[0] << " <level> <ra> <dec> <radius> <tablename> <names 1|0>" << endl;
        cerr << endl;
        cerr << "<level>      HTM level (between 0 and 24)" << endl;
        cerr << "<ra>         RA in degrees" << endl;
        cerr << "<dec>        Dec in degrees" << endl;
        cerr << "<radius>     Radius in arcsec" << endl;
        cerr << "<columns>    Which columns you want - e.g. '*' or 'ra,dec' - no spaces" << endl;
        cerr << "<tablename>  The database table name." << endl;
        cerr << "<sublevel1>  HTM level (between 11 and 24) - must be lower than sublevel2" << endl;
        cerr << "<sublevel2>  HTM level (between 11 and 24) - must be equal to level" << endl;
        cerr << endl;
        cerr << "e.g.:" << argv[0] << " 16 185.76701238 -58.40231219 10.0 'ra,dec' tcs_cat_gaia_dr2 13 16" << endl;
        return -1;
    }


  int args              =1;
  size_t level          = atoi(argv[args++]);
  float64 ra            = atof(argv[args++]);
  float64 dec           = atof(argv[args++]);
  float64 radius        = atof(argv[args++])/60.0;
  char * c              = argv[args++];
  string columns(c);
  char * t              = argv[args++];
  string table_name(t);
  size_t sublevel1      = atoi(argv[args++]);
  size_t sublevel2      = atoi(argv[args++]);


  //printf("%s", htmCircleRegionCassandra(level, ra, dec, radius, table_name, sublevel1, sublevel2).c_str());
  std::vector<std::string> q = htmCircleRegionCassandraSetMultiQuery(level, ra, dec, radius, columns, table_name, sublevel1, sublevel2);
  for(size_t i=0; i<q.size(); i++)
  {
    printf("%s\n", q[i].c_str());
  }
  return 0;
}

*/

// New code that just generates results based on ra, dec and radius. Always assumes Levels 10, 13 and 16.
// Alwasy assumes the indexing is by level 10 and clustering index is 13, 16.

/*
int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        cerr << "Usage: " << argv[0] << " <level> <ra> <dec> <radius> <tablename> <names 1|0>" << endl;
        cerr << endl;
        cerr << "<ra>         RA in degrees" << endl;
        cerr << "<dec>        Dec in degrees" << endl;
        cerr << "<radius>     Radius in arcsec" << endl;
        cerr << "<columns>    Which columns you want - e.g. '*' or 'ra,dec' - no spaces" << endl;
        cerr << "<tablename>  The database table name." << endl;
        cerr << endl;
        cerr << "e.g.:" << argv[0] << " 185.76701238 -58.40231219 10.0 'ra,dec' tcs_cat_gaia_dr2" << endl;
        return -1;
    }


    int args              =1;
    float64 ra            = atof(argv[args++]);
    float64 dec           = atof(argv[args++]);
    float64 radius        = atof(argv[args++]);
    char * c              = argv[args++];
    string columns(c);
    char * t              = argv[args++];
    string table_name(t);

    size_t level; 
    size_t sublevel1;
    size_t sublevel2;


    // Boundaries are:
    // r <= 15.0 arcsec - use Level 16 - i.e. we pass 16, 13, 16
    // r  > 15.0 arcsec and r <= 200.0 arcsec - use Level 13 - i.e. we pass 13, 13, 13
    // r  > 200.0 arcsec and r <= 1800.0 arcsec - use Level 10 - i.e. we pass 10, 10, 10

    if (radius <= 15.0)
    {
        level = 16;
        sublevel1 = 13;
        sublevel2 = 16;
    }
    else if (radius > 15.0 && radius <= 200.0)
    {
        level = 13;
        sublevel1 = 13;
        sublevel2 = 13;
    }
    else if (radius > 200.0 && radius <= 1800.0)
    {
        level = 10;
        sublevel1 = 10;
        sublevel2 = 10;
    }
    else
    {
        cerr << "Radius must be less than (or equal to) 1800.0 arcsec (0.5 degrees)." << endl;
        return -1;
    }

    // HTM API wants radius in arcmins!
    radius = radius/60.0;

    std::vector<std::string> q = htmCircleRegionCassandraSetMultiQuery(level, ra, dec, radius, columns, table_name, sublevel1, sublevel2);

    for(size_t i=0; i<q.size(); i++)
    {
        printf("%s\n", q[i].c_str());
    }

    return 0;
}

*/

// Complete generic rewrite.
std::vector<std::string> htmCircleRegionCassandraMultiQuery (size_t deepestlevel, double ra, double dec, double radius, std::string columns, std::string table_name, size_t sublevels, size_t step, bool whereOnly)
{
  //stringstream s;

  string name;
  string query;

  // create level index required
  htmInterface htm(deepestlevel, 5);

  std::vector<htmRange> list=htm.circleRegion(ra, dec, radius);
  std::vector<std::string> queries;

  std::map<string, vector<string> > htmpartitionmap;

  // Populate the map.
  for(size_t idx=0; idx<list.size(); idx++)
  {
      for(long htmID = list[idx].lo; htmID <= list[idx].hi; htmID++)
      {
          name = htm.lookupName(htmID);

          if(sublevels > 0 && step > 0 && sublevels * step < (deepestlevel + 1))
          {
              htmpartitionmap[name.substr(0, (2+deepestlevel)-(sublevels*step))].push_back(name.substr((2+deepestlevel)-(sublevels*step), (sublevels*step))); 
          }
          else
          {
              htmpartitionmap[name]; // adds an empty vector, which we don't need.
          }
      }
  }

  // Spit out the contents of the map.
  map<string, vector<string> >::iterator it;

  // This variable used if we are using the lowest (coursest) HTM ID only.
  stringstream ss;

  for ( it = htmpartitionmap.begin(); it != htmpartitionmap.end(); it++ )
  {
      string key = it->first;

      vector<string> values = it->second;
      if(values.size() > 0)
      {
          stringstream s;
          if (!whereOnly)
          {
              s << "select " << columns << " from " << table_name;
          }
          s << " where htm" << deepestlevel - sublevels * step << " IN ";
          s << "(" << "'" << key << "'" << ")";
          s << " AND (";
          for(size_t sl = 0; sl < sublevels; sl++)
          {
              s << "htm" << deepestlevel - sublevels * step + (sl+1)*step;
              if (sl < sublevels - 1)
              {
                  s << ",";
              }
          }
          s << ") IN (";

          for (size_t i = 0; i < values.size(); i++)
          {
              s << "(";
              for(size_t j = 0; j < sublevels; j++)
              {
                  s << "'" << values[i].substr(j*step,step) << "'";
                  if (j < sublevels - 1)
                  {
                      s << ",";
                  }
              }
              s <<  ")";
              if (i < values.size() - 1)
              {
                  s << ",";
              }
          }
          s <<  ")";
          queries.push_back(s.str());
      }
      else
      {
          if (it == htmpartitionmap.begin())
          {
              // We only need this at the beginning of the query.
              if (!whereOnly)
              {
                  ss << "select " << columns << " from " << table_name;
              }
              ss << " where htm" << deepestlevel - sublevels * step << " IN (";
          }
          ss << "'" << key << "'";
          if (it != std::prev(htmpartitionmap.end()))
          {
              ss << ",";
          }
          if (it == std::prev(htmpartitionmap.end()))
          {
              // We only need this at the end of the query.
              ss << ")";
              queries.push_back(ss.str());
          }
      }

  }
  return queries;
}

std::vector<std::string> htmCircleRegionCassandra (double ra, double dec, double radius)
{
    // Boundaries are:
    // r <= 15.0 arcsec - use Level 16 - i.e. we pass 16, 2, 3
    // r  > 15.0 arcsec and r <= 200.0 arcsec - use Level 13 - i.e. we pass 13, 0, 3
    // r  > 200.0 arcsec and r <= 1800.0 arcsec - use Level 10 - i.e. we pass 10, 0, 0
    size_t deepestlevel; 
    size_t sublevels;
    size_t step;
    std::string columns = "";
    std::string table_name = "";
    std::vector<std::string> q;
    bool whereOnly = true;

    if (radius <= 15.0)
    {
        deepestlevel = 16;
        sublevels = 2;
        step = 3;
    }
    else if (radius > 15.0 && radius <= 200.0)
    {
        deepestlevel = 13;
        sublevels = 1;
        step = 3;
    }
    else if (radius > 200.0 && radius <= 1800.0)
    {
        deepestlevel = 10;
        sublevels = 0;
        step = 0;
    }
    else
    {
        return q;
    }

    // HTM API wants radius in arcmins!
    radius = radius/60.0;

    q = htmCircleRegionCassandraMultiQuery (deepestlevel, ra, dec, radius, columns, table_name, sublevels, step, whereOnly);

    return q;
}

/*
std::vector<std::string> htmCircleRegionCassandra (double ra, double dec, double radius)
{
    // Boundaries are:
    // r <= 15.0 arcsec - use Level 16 - i.e. we pass 16, 13, 16
    // r  > 15.0 arcsec and r <= 200.0 arcsec - use Level 13 - i.e. we pass 13, 13, 13
    // r  > 200.0 arcsec and r <= 1800.0 arcsec - use Level 10 - i.e. we pass 10, 10, 10
    std::vector<std::string> queries;
    size_t level; 
    size_t sublevel1;
    size_t sublevel2;

    if (radius <= 15.0)
    {
        level = 16;
        sublevel1 = 13;
        sublevel2 = 16;
    }
    else if (radius > 15.0 && radius <= 200.0)
    {
        level = 13;
        sublevel1 = 13;
        sublevel2 = 13;
    }
    else if (radius > 200.0 && radius <= 1800.0)
    {
        level = 10;
        sublevel1 = 10;
        sublevel2 = 10;
    }
    else
    {
        return queries;
    }

    radius = radius/60.0;
    string name;
    string name10;
    string sub1;
    string sub2;
    string query;

    // create level index required
    htmInterface htm(level, 5);

    std::vector<htmRange> list=htm.circleRegion(ra, dec, radius);

    std::set<string> htm10s;
    std::vector<string> htmsuffix1;
    std::vector<string> htmsuffix2;

    for(size_t idx=0; idx<list.size(); idx++){
        for(long htmID = list[idx].lo; htmID <= list[idx].hi; htmID++){
            name = htm.lookupName(htmID);
            if (level > 10)
            {
                if (sublevel1 > 10)
                {
                    sub1 = name.substr(10+2, sublevel1 - 10);
                    if (sublevel2 > sublevel1)
                    {
                        sub2 = name.substr(sublevel1+2, sublevel2-sublevel1);
                        name10 = name.substr(0,2+10);
                        htm10s.insert(name10);
                        htmsuffix1.push_back(sub1);
                        htmsuffix2.push_back(sub2);
                    }
                    else if (sublevel2 == sublevel1 && sublevel1 == level) // just push onto sublevel1
                    {
                        name10 = name.substr(0,2+10);
                        htm10s.insert(name10);
                        htmsuffix1.push_back(sub1);
                    }
                } 
            }
            else
            {
                htm10s.insert(name);
            }
            

            if (htmID != list[idx].hi) query += ",";
            }
        if (idx != list.size() - 1) query += ",";
    }


    // Pull out everything from the set except the last element. There's always at least one.
    std::set<string>::iterator it;

    if (level > 10)
    {
        for(it = htm10s.begin(); it != htm10s.end(); ++it)
        {
            stringstream s;
            // Print out the HTM10s
            s << " where htm10 IN ";
            s << "(";
            s << "'" << *it << "'";
            s << ")" << endl;

            if (level > 10)
            {
                if (sublevel2 > sublevel1)
                {
                    s << "AND (htm" << sublevel1 << ",htm" << sublevel2 << ") IN" << endl;
                    s << "(";
                    for(size_t idx=0; idx<htmsuffix1.size() - 1; idx++)
                    {
                        // Print out the sub HTMs
                        s << "('" << htmsuffix1[idx] << "','" << htmsuffix2[idx] << "'),"; 
                    }
                    s << "('" << htmsuffix1.back() << "','" << htmsuffix2.back() << "')"; 
                }
                else if (sublevel2 == sublevel1 && sublevel1 == level) // just push onto sublevel1
                {
                    s << "AND htm" << sublevel1 <<  " IN" << endl;
                    s << "(";
                    for(size_t idx=0; idx<htmsuffix1.size() - 1; idx++)
                    {
                        // Print out the sub HTMs
                        s << "'" << htmsuffix1[idx] <<  "',"; 
                    }
                    s << "'" << htmsuffix1.back() << "'"; 
                }

            }

            s << ");" << endl;
            queries.push_back(s.str());
        }
    }
    else
    {
        // Just return a single statement with all the HTM10 IDs.
        stringstream s;
        s << " where htm10 IN ";
        s << "(";
        for(it = htm10s.begin(); it != --htm10s.end(); ++it)
        {
            // Print out the HTM10s
            s << "'" << *it << "',";
        }
        // Print out the last one
        s << "'" << *it << "'";
        s << ");" << endl;
        queries.push_back(s.str());
    }

    return queries;
}
*/

/*
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        cerr << "Usage: " << argv[0] << " <level> <ra> <dec> <radius> <tablename> <names 1|0>" << endl;
        cerr << endl;
        cerr << "<ra>         RA in degrees" << endl;
        cerr << "<dec>        Dec in degrees" << endl;
        cerr << "<radius>     Radius in arcsec" << endl;
        cerr << endl;
        cerr << "e.g.:" << argv[0] << " 185.76701238 -58.40231219 10.0" << endl;
        return -1;
    }


    int args              =1;
    float64 ra            = atof(argv[args++]);
    float64 dec           = atof(argv[args++]);
    float64 radius        = atof(argv[args++]);

    // Boundaries are:
    // r <= 15.0 arcsec - use Level 16 - i.e. we pass 16, 13, 16
    // r  > 15.0 arcsec and r <= 200.0 arcsec - use Level 13 - i.e. we pass 13, 13, 13
    // r  > 200.0 arcsec and r <= 1800.0 arcsec - use Level 10 - i.e. we pass 10, 10, 10

    std::vector<std::string> q = htmCircleRegionCassandra(ra, dec, radius);

    for(size_t i=0; i<q.size(); i++)
    {
        printf("%s\n", q[i].c_str());
    }
}
*/
